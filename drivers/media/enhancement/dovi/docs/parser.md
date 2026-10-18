# Parser and metadata contracts

## Host callbacks and storage

[metadata_init_body.c](../core/metadata_init_body.c) initializes the host adapter;
[metadata_runtime.c](../core/metadata_runtime.c) implements processing, reset and
release. The adapter contracts follow the Dolby library version. Calls and
callbacks must be serialized; keep inputs, outputs, length slots and parser
storage disjoint. The event frame counter is shared, including across STB 2.6
instances. An error can leave partial output and does not roll back earlier
writes.

`PARSER_ALLOCATED_APPLICATION_STORAGE` selects the callback state owners;
`PARSER_SIGN_EXTEND_METADATA_COPY_LENGTH` selects signed-64 versus unsigned-32
metadata copy-length widening. These policies are independent of reset API flags.

### STB 2.4 (4.9)

The adapter owns one static `0x780`-byte application context and contiguous
backing storage. [parser_lifecycle.c](../core/parser_lifecycle.c) separately owns
the parser/decoder singleton. Initialization replaces its callback context, so
only one pipeline may be active. The logger must remain valid and non-reentrant
for that pipeline's lifetime.

Initialization selects main profile, level 0 and payload-only input. Its flag
argument is ignored. It binds internal output storage but leaves the caller's
composer and metadata output pointers null until processing supplies them.
Direct sample-event or T.35 calls need those output pointers bound first.
Release does nothing; it neither frees nor resets the singleton. Reset ignores
its host flag argument and retains the frame counter.

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
The payload-only input slot is 4,096 bytes. The full end-of-stream byte reaches
the low-level parser.

Each sample event converts and validates composer configuration, appends it,
then validates and serializes display metadata. The frame counter advances for
every event, including error and unknown events. Neither processing nor
initialization resets it. On parser success, the callback publishes the output
lengths, resets the parser and returns the flags. Failure returns `-1` without
publishing lengths. Callback validation or capacity errors may leave partial
output, so consumers must use the published lengths.

### STB 2.6 (5.15)

Each instance owns a `0x770`-byte context, a `0x10442`-byte serialization scratch
buffer and a queried decoder/parser arena. `module_parser_context.h` asserts
the callback configuration, buffers, arena and per-instance size offsets.
Release takes a pointer to the instance pointer, frees owned allocations in
the native order and clears that pointer. Initialization failure performs the
same ordered cleanup. The host initializer has no logger or flag argument.

The caller provides disjoint composer and metadata outputs with capacities
`0x2000` and `0x400`, and separate writable `unsigned int` output-length slots.
Input kinds 1 and 2 run VUI, ST2086 and T.35 ingestion from the native composite
input layout. Other kinds discard five prefix bytes and trim at most 63
trailing zero bytes. Preserve readable padding before and after short inputs,
as well as the bit-reader padding below. Only host flags bit 0 reaches the
payload parser as its flush argument.

On parser success, processing publishes accumulated lengths and returns flags,
retaining state for subsequent input. Failure returns `-1` without publishing
lengths. Capacity checks retain native signedness and partial writes. Reset is
a separate callback: it requests a deep reset and clears the shared frame
counter after successful reset. A null instance makes process/reset return
`-1` with diagnostics.

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
nonzero operating limits. STB 2.4 leaves level indices unchecked. STB 2.6
reaches its fatal path for the configuration level word at `+4` of 16 or
greater, after partial initialization.

Initialization always clears `0xe070` bytes at the partition payload base.
The required writable extent is therefore:

```text
payload_base + max((max_prev + 2) * max_partitions * 0x7038, 0xe070)
```

For the zero rows 3 through 15, this exceeds the size returned by
[dovi_rpu_decoder_mem_query](../core/memory_query.c). Do not allocate solely from
that query for those rows.

Both host adapters select level 0 and payload-only input. STB 2.4 uses the
singleton storage defined in [parser_lifecycle.h](../core/parser_lifecycle.h);
STB 2.6 allocates the parser prefix and decoder arena in
[metadata_init_body.c](../core/metadata_init_body.c).

STB 2.4 reset clears indices, state and MEL without deep payload clearing. STB
2.6 adds a flags argument: flags equal to one clear the current `0x136`-byte
header, `0x10744`-byte DM payload and two payload records (`0xe070` bytes) in
direct mode. Buffered STB 2.6 reset traps after writing the first fifteen
payload handles when the slot count is larger; STB 2.4 retains unchecked
copying. Payload-handle indexing uses pointer-width displacement in both.

Both low-level parser loops stop on queue sentinels `-20` and `-21`. STB 2.4
reports the first other nonpositive output once and stops; STB 2.6 continues
according to its native unsigned loop condition and retains the last error.
STB 2.4 reduces a nonzero MEL byte to flag bit 0; STB 2.6 ORs the raw MEL byte
into the output flags.

## RPU and output conversion

[rpu_buffering.c](../core/rpu_buffering.c) handles byte ingress;
[rpu_pipeline.c](../core/rpu_pipeline.c) consumes queued buffers and runs header,
level, spatial-filter, payload, profile, MEL and display-metadata stages. Queue
consumption also occurs on processing errors. The pipeline rotates buffer
ownership within the caller-provided decoder arena; it does not allocate an
independent payload per frame. The payload partition is `0x7038` bytes. Current
and saved payloads, headers and DM records must remain live for the corresponding
decode and callback sequence.

Composer conversion requires at least `0xf1` bytes of header, `0x68c8` bytes
of payload and `0x708` bytes of output. It writes sparse fields and preserves
untouched bytes. STB 2.6 retains fatal limits for more than nine pivots,
polynomial orders above two and MMR orders above three; fatal paths may follow
partial writes. STB 2.4 retains unchecked paths and its unsupported-index
returns. STB 2.6 also checks decoder/query levels, validation indices, mapping
pivots, payload IDs, spatial modes and sentinel-table bounds, and retains the
extra profile-16 row. These limits do not provide general bounds checking for
all nested extents, prediction references or extension appends.

The direct unknown-extension payload helper copies the declared length in STB
2.4 and requires sufficient backing. STB 2.6 stops after 256 bytes. The outer
extension reader in both versions rejects lengths above 256 before invoking
that helper. This is separate from the unchecked unknown-record copy during
serialization below. Extension payload reading receives explicit dependencies
in STB 2.4 and derives them from the decoder in STB 2.6. The non-first-record
diagnostic tests kind equal to two in STB 2.4 and kind unequal to one in STB 2.6.
The selected byte index is read-only in both.

## Metadata representations

The parser and control path use different formats:

| Representation | Base | Extension record |
| --- | --- | --- |
| Parser DM payload | 76 bytes, little-endian fields | 264 bytes; length at `+0`, level at `+4`, payload at `+6` |
| Packed control metadata | 71 bytes, big-endian multibyte fields | Four-byte length, one-byte level, then payload |
| STB 2.4 control storage | 71-byte base | Fixed 261-byte slots for the five-byte header and up to 256 payload bytes |

[dm_serialization.c](../core/dm_serialization.c) converts the parser format into
a packed byte sequence. [dm_metadata.c](../core/dm_metadata.c) consumes it in
the display-management path. These buffers are not interchangeable. The
parser's full DM payload is `0x10744` bytes. STB 2.4 control storage is 66,626
bytes. Fixed-structure conversion in [dm_struct_conversion.c](../core/dm_struct_conversion.c)
uses a separate `0x10442`-byte representation and retains its unknown-level
and level-10 length behavior. No backing extent validates malformed record
lengths by itself.

## Serialization and partial output

`dovi_dm_rpu_payload_2_byte_sequence` takes separate input, output and signed capacity
objects. Capacity below 71 returns `-12` without output writes. Later exhaustion
also returns `-12`, retains the caller's capacity value and may leave the base,
earlier records and part of the current record written. Success returns zero
and replaces capacity with the produced byte count.

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
and [dm_extensions.c](../core/dm_extensions.c) also require sentinel backing,
and some appends happen before the extension count saturates. STB 2.4 adds
initial-extension and second incremental-update read results to its bit-count
accounting; STB 2.6 omits those additions. For unknown packed extension payloads,
STB 2.4 uses signed 32-bit wrapping sequence-offset arithmetic, while STB 2.6
selects the block pointer before indexing its payload.

## Nonlinear quantization and MEL

`nlq_layout.h` names the prediction flags, parameter records, prediction distances
and coefficient references within each RPU payload partition. Parameters use a
28-byte stride indexed by pivot and component. Linear-deadzone and mu-law data
share a union; their active interpretation follows the RPU header.

MEL detection examines only the three components of pivot zero. It requires a
zero NLQ offset, a VDR input maximum of one with zero fractional bits, and zero
linear-deadzone parameters. The two padding bytes after the 16-bit offset are
ignored. Prediction reuse, buffer reloads and the parser's partial writes retain
their native behavior. STB 2.4 identity fallback initializes complete 28-byte
NLQ records; STB 2.6 retains sparse overlapping stores and their untouched bytes.

## Shared source and versioned behavior

`parser_context.h` describes the common 0x70-byte parser prefix and 0x38-byte
configuration. `parser_layout.h` names its byte offsets; scalar and pointer
accesses retain the little-endian, potentially unaligned arena contract. The
shared callback receives read-only opaque payload views and signed 32-bit
length/result values. These source declarations preserve the ARM64 argument
bits and native partial writes.

`parser_contract.h` selects Dolby library behavior independently of Linux.
STB 2.6 adds reset flags and direct-mode payload clearing, drains buffered
errors differently, preserves the raw MEL byte in output flags, and retains
fatal array bounds and its extra profile-16 row. STB 2.4 retains its historical
unsupported-index returns and unchecked paths. Extension callback ownership,
unknown-payload bounds, bit-count adjustments, sparse identity fallback writes
and packed sequence offset arithmetic also remain versioned.

The NLQ coefficient sign mask has an observable width boundary: STB 2.4 wraps
`width + 1` before selecting a one-bit mask, while STB 2.6 shifts a 32-bit value
of two and drops that mask when `width & 31` is 31. The selector is named
`PARSER_NLQ_TRUNCATING_SIGN_SHIFT`. Equivalent single-bit readers, byte alignment,
synthetic defaults, trailing-zero scan arithmetic and ST2094 target windows
use one representation in both modules.

Ownership and host callback signatures remain explicit: STB 2.4 uses singleton
application/parser storage; STB 2.6 allocates independent parser instances and
retains its native release, failure cleanup, input-kind routing and reset
sequencing. The [changelog](changes.md) records these changes separately from
kernel build and host registration differences.
