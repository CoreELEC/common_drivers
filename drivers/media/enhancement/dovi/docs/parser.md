# STB 2.4 parser and metadata contracts

## Host callbacks and storage

[metadata_init_body.c](../core/metadata_init_body.c) initializes the host adapter;
[metadata_runtime.c](../core/metadata_runtime.c) implements processing, reset and
release. Calls and callbacks must be serialized. Keep input, output, length
slots and parser storage disjoint. Errors can leave partial output and do not
roll back earlier writes.

The adapter owns one static `0x780`-byte application context and contiguous
backing storage. [parser_lifecycle.c](../core/parser_lifecycle.c) separately owns
the 334,048-byte parser/decoder singleton. Initialization replaces its callback
context, so only one pipeline may be active. The logger must remain valid and
non-reentrant for that pipeline's lifetime.

Initialization selects main profile, level 0 and payload-only input. Its flag
argument is ignored. It binds internal output storage but leaves the caller's
composer and metadata output pointers null until processing supplies them.
Direct sample-event or T.35 calls need those output pointers bound first.
Release does nothing. Reset ignores its host flag argument and retains the
frame counter.

The process callback expects:

| Argument | Required storage |
| --- | --- |
| Input | A five-byte prefix followed by payload, with readable padding for trailing-zero scans and bitstream refills |
| Composer output | 8,192 writable bytes; each appended configuration is 1,800 bytes |
| Metadata output | 1,024 writable bytes |
| Output lengths | Separate writable `int` slots |

Processing clears the per-call output counters, skips the prefix, trims at
most 63 trailing zero bytes and dispatches payload-only decoder input. The
input must contain a nonzero payload byte; the backward scan does not enforce
the input's lower bound. The nominal length does not establish safe backing.
The payload-only input slot is 4,096 bytes. The adapter passes its end-of-stream
byte to the low-level parser; the host-facing wrapper receives a `bool`.

Each sample event converts and validates composer configuration, appends it,
then validates and serializes display metadata. The frame counter advances for
every event, including error and unknown events. Neither processing nor
initialization resets it. On parser success, the process callback publishes the
output lengths, resets the parser and returns the flags. Parser failure returns
`-1` without publishing lengths. Event validation or capacity errors can leave
partial output even when the parser itself succeeds; consumers must use the
published lengths.

## Bitstream input

[bitstream.h](../core/bitstream.h) defines a 32-byte MSB-first reader.
Initialization accepts a nominal size of at least two bytes but reads an initial
four-byte word; refills read two bytes. The backing allocation must remain
readable through these padded accesses, beyond the nominal payload where
necessary. The reader does not provide general bounds checking.

Unsigned reads preserve masked-shift behavior outside the usual 1..32-bit
range. Signed reads use widths 1..32. Exp-Golomb decoding caps the leading-zero
scan at 31 bits. Narrowing, bit order and cursor updates are part of the
interface, including on malformed input.

## Decoder and parser state

The low-level parser configuration is `0x38` bytes: callback at `0`, user
pointer at `8`, decoder configuration at `0x10`, and logging state at `0x30`.
The low-level parser does not own its backing allocation. Destroy clears
the initialized flag and decoder pointer without freeing the caller's arena.
Initialization with a missing event callback still writes decoder state
before returning error 9.

An initialized parser stores its callback at `+8`, user pointer at `+0x10`,
last error at `+0x18`, direct-mode selector at `+0x2c`, decoder at `+0x38`,
and timestamps at `+0x40` and `+0x50`. Process flag bit 3 supplies a new
eight-byte timestamp. Processing returns 2 for a null parser, 1 for an
uninitialized parser, and otherwise zero or a decoder error. Error paths
can update state and invoke callbacks.

[decoder_lifecycle.c](../core/decoder_lifecycle.c) requires at least `0x18`
bytes of configuration and lays out caller-owned decoder storage in this order:

| Region | Bytes |
| --- | --- |
| Decoder context | `0x316e8` (202,472) |
| Input slots | `N * 0x1000`, with `N = 1` in payload-only mode and `N = 64` otherwise |
| Slot levels | `N * 4` |
| RPU context | `0x87c` |
| RPU data pointers | `0x18` |
| RPU header | `0x136` |
| DM payload | `0x10744` |
| Partition payloads | `(max_prev + 2) * max_partitions * 0x7038` |

The partition payload base is `0x437fa` in payload-only mode and `0x828f6` in
stream mode. Internal pointers may be unaligned, so the initializer accesses
their fields bytewise. Only rows 0 through 2 of the 16-row level table contain
nonzero operating limits. Level indices are unchecked.

Initialization always clears `0xe070` bytes at the partition payload base.
The required writable extent is therefore:

```text
payload_base + max((max_prev + 2) * max_partitions * 0x7038, 0xe070)
```

For the zero rows 3 through 15, this exceeds the size returned by
[dovi_rpu_decoder_mem_query](../core/memory_query.c). The host adapter selects
level 0 and payload-only input and uses the singleton storage defined in
[parser_lifecycle.h](../core/parser_lifecycle.h).

Reset clears indices, state and MEL without deep payload clearing. Buffered
reset copies payload handles without an array-bound trap; handle indexing
uses pointer-width displacement. The parser loop stops on queue sentinels
`-20` and `-21`, reports the first other nonpositive output once and stops.
A nonzero MEL byte becomes output flag bit 0.

## RPU and output conversion

[rpu_buffering.c](../core/rpu_buffering.c) handles byte ingress;
[rpu_pipeline.c](../core/rpu_pipeline.c) consumes queued buffers and runs header,
level, spatial-filter, payload, profile, MEL and display-metadata stages. Queue
consumption also occurs on processing errors. The pipeline rotates buffer
ownership within the caller-provided decoder arena. The payload partition is
`0x7038` bytes. Current and saved payloads, headers and DM records must remain
live for the corresponding decode and callback sequence.

Composer conversion requires at least `0xf1` bytes of header, `0x68c8` bytes
of payload and `0x708` bytes of output. It writes sparse fields and preserves
untouched bytes. Unsupported indices retain their native returns, and nested
array accesses are not generally bounds checked.

The direct unknown-extension payload helper copies the declared length and
requires sufficient backing. The outer extension reader rejects lengths above
256 before invoking that helper. This is separate from the unchecked
unknown-record copy during serialization below. Extension payload reading
receives explicit dependencies. The non-first-record diagnostic tests kind
equal to two; the selected byte index is read-only.

## Metadata representations

The parser and control path use different formats:

| Representation | Base | Extension record |
| --- | --- | --- |
| Parser DM payload | 76 bytes, little-endian fields | 264 bytes; length at `+0`, level at `+4`, payload at `+6` |
| Packed control metadata | 71 bytes, big-endian multibyte fields | Four-byte length, one-byte level, then payload |
| Control storage | 71-byte base | Fixed 261-byte slots for the five-byte header and up to 256 payload bytes |

[dm_serialization.c](../core/dm_serialization.c) converts the parser format into
a packed byte sequence. [dm_metadata.c](../core/dm_metadata.c) consumes it in
display management. These buffers are not interchangeable. The parser's full
DM payload is `0x10744` bytes. Control storage is 66,626 bytes. Fixed-structure
conversion in [dm_struct_conversion.c](../core/dm_struct_conversion.c) uses a
separate `0x10442`-byte representation and retains its unknown-level and
level-10 length behavior. Backing extent alone does not validate malformed
record lengths.

## Serialization and partial output

`dovi_dm_rpu_payload_2_byte_sequence` takes separate input, output and signed
capacity objects. Capacity below 71 returns `-12` without output writes. Later
exhaustion also returns `-12`, retains the caller's capacity value and may leave
the base, earlier records and part of the current record written. Success
returns zero and replaces capacity with the produced byte count.

| Extension level | Serialized payload bytes |
| --- | --- |
| 1, 3 | 6 |
| 2 | 14 |
| 4 | 4 |
| 5, 6 | 8 |
| 8 | 13, 15, 17, 23 or 29, according to optional input fields |
| 9 | 1, or 17 when the primary index is at least 19 |
| 10 | 6, or 22 when the primary index is at least 19 |
| 11 | 4 |
| 254 | 2 |
| 255 | 6 |
| Other | The declared input length |

Level 8 adds fields at input-length thresholds 11, 13, 14 and 20. Levels 8
through 10 check capacity in stages and write the length header after their
payload succeeds. A failed later stage can leave payload bytes behind an
unchanged length header. Record order and duplicate levels are preserved.

Unknown levels copy their declared length without a 256-byte union limit;
larger values require additional readable backing. Keep DM validation before
serialization. Extension readers and updates in [ext_blocks.c](../core/ext_blocks.c)
and [dm_extensions.c](../core/dm_extensions.c) require sentinel backing,
and some appends happen before the extension count saturates. Initial-extension
and second incremental-update read results contribute to bit-count accounting.
Unknown packed extension payloads use signed 32-bit wrapping sequence-offset
arithmetic.

## Nonlinear quantization and MEL

`nlq_layout.h` names prediction flags, parameter records, prediction distances
and coefficient references within each RPU payload partition. Parameters use a
28-byte stride indexed by pivot and component. Linear-deadzone and mu-law data
share a union; their active interpretation follows the RPU header.

MEL detection examines only the three components of pivot zero. It requires a
zero NLQ offset, a VDR input maximum of one with zero fractional bits, and zero
linear-deadzone parameters. The two padding bytes after the 16-bit offset are
ignored. Prediction reuse, buffer reloads and partial writes retain their
native behavior. Identity fallback initializes complete 28-byte NLQ records.
The NLQ coefficient sign mask wraps `width + 1` before selecting a one-bit mask.

`parser_context.h` describes the `0x70`-byte parser prefix and `0x38`-byte
configuration. `parser_layout.h` names the offsets; scalar and pointer accesses
retain the little-endian, potentially unaligned arena contract. The callback
receives read-only opaque payload views and signed 32-bit length/result values.
