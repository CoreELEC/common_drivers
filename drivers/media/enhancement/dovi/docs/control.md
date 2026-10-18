# Control and register output

This guide compares the Dolby STB 2.4 and STB 2.6 control interfaces.
Library layout, callback, lifecycle and failure differences are independent of
the Linux kernel version. Common algorithms use matching source and declarations;
the version contracts retain actual native differences.

## Host boundary

### STB 2.4

`host_adapter.c` receives the host control arguments and delegates to
`host_control.c`. The adapter translates SDR BT.2020 from host enum value `6`
to core value `4`; values `-1` through `3` retain their meaning. Unsupported
formats return `-1`, except that input format `-1` selects reset without
requiring an ordinary output format.

The host output is 13,480 bytes. The core operates on its 13,468-byte prefix,
whose offsets are defined in `control_setting_layout.h`. `host_abi.h` checks
those offsets against `struct dovi_setting_s`, including nested register, LUT,
HDR and extension records. The final `vsvdb_len`, `vsvdb_changed` and
`mode_changed` words, and source/destination format fields, remain host-owned.

The output is an input/output object: processing reads dimensions, graphics
settings, low-latency flags and VSVDB before writing results. Video dimensions
use their upper 16 bits; target dimensions use the host output-width and
output-height fields. Missing video dimensions select 1920 by 1080, and missing
output dimensions use the source dimensions.

### STB 2.6

The adapter includes `amdv.h` and registers a persistent
`struct dolby_vision_func_s`. The control callback accepts
`struct m_dovi_setting_s`; parser callbacks use the declared host types,
including `enum dv_type_enum`. Unsupported callback slots remain null. The
version string starts after the stored `AML_DV_INFO1:` prefix.

The settings record is `0x4f58` bytes, with three `0x68`-byte private input
records and two Core 1 result slots. Its Core 1 registers start at `0x1a8`,
Core 2 registers at `0x2fe8`, Core 3 registers at `0x3048`, Core 2 LUT at
`0x30b0`, Core 3 metadata at `0x44b0`, and HDR information at `0x4cb4`.
`control_setting_layout.h` names the offsets; `host_abi.h` checks sizes and
offsets against the host declarations so an incompatible layout fails compilation.

## Persistent storage and lifecycle

Both control paths share mutable state and scratch storage. Callers serialize
control calls and avoid reentry; per-instance parser allocation does not make
control concurrent. Services must remain valid and output objects remain
writable for the full call. Memory requests describe register/LUT processing,
not CPU image-pixel processing. Preserve packed load/store order where views overlap.

### STB 2.4

`control_runtime.c` owns aligned static storage:

| Buffer | Bytes |
| --- | --- |
| Control arena | 20,040 |
| CLI parameters | 112 |
| Current and previous metadata | 66,626 each |
| Composition | 1,800 |
| Register scratch 1 / 2 / 3 | 900 / 96 / 1,024 |

Services are copied before registration. CPU identification, logging and
diagnostic callbacks must remain valid. The runtime and chroma scratch are
singletons with no internal locking.

`control_path.c` initializes the arena on first use. Input format `-1` invokes
the no-op `dovi_destroy_cp` and clears the initialized flag. Ordinary completion
also invokes that helper but leaves the flag set. The common helper ignores
all incoming argument registers and returns zero; its source declaration takes
no argument. Reset and ordinary completion remain distinct lifecycle transitions.

Composition records advance in 1,800-byte steps. Loop termination compares
consumed composition and metadata offsets for equality with supplied lengths.
Callers supply complete readable records and consistent lengths; these are
not general physical bounds checks. HDR10 input covers 28 readable bytes even
when a selected branch does not produce an HDR10 infoframe.

### STB 2.6

`control_memory.c` and `control_state.c` initialize the arena and source state.
`control_path.c` selects valid inputs and a primary input, prepares input/target
configuration, handles graphics/video parameters and commits display management.
Zero input count resets native staging and initialization/content bytes. Ordinary
calls retain shared control state. Initialization takes instance/configuration
inputs, rejects counts at least five and retains native zero/negative-count behavior.

`register_commit.c` preserves source preparation, source-flag updates and input
packing phases. `control_target.c` decodes capabilities and configures targets;
`control_publish.c` places registers, LUTs and metadata in host result fields.
Capacity checks can occur after partial state and output writes.

## Display capabilities and metadata

Both decoders consume complete 32-byte VSVDB storage without a physical length
argument. Version 0 reads 21 bytes, long version 1 reads 15, and compact version
1 or version 2 reads 12. Packed-primary arithmetic is shared; luminance units,
rounding, write phases and errors remain distinct.

### STB 2.4

`control_target.c` writes an 88-byte target descriptor. Null or invalid
identification returns `-1` without target writes. Unsupported versions retain
capability fields written before failure; unsupported version-1 length also
retains four luminance/PQ writes. Width, height and low-latency request words
are preserved. Input and target storage must be disjoint.

`control_packets.c` builds the 29-byte HDR infoframe and fixed metadata.
`control_metadata.c` filters packed metadata and prepares HDMI output.
Luminance getters require valid active DM configuration because callers use
their results without handling failure first.

Trim reduction uses at most sixteen level-2 and sixteen level-8 records in local
arrays. Its target argument addresses the stored 64-bit target pointer. See
`trim_reduction.h` in the STB 2.4 tree for sorting and storage requirements.

### STB 2.6

`control_target.c` writes capability fields within the control context. A null
VSVDB retains state and succeeds; invalid identification/version/length returns
`-8` after the native partial writes. Version-1 minimum luminance squares and
left-shifts a wrapped 32-bit value, reinterprets it as signed, then divides toward
zero. This is distinct from the STB 2.4 decimal-scaled calculation.

VSEM/VSIF parsing validates packet headers and OUI, retains native packet/type/
status updates and returns `-5` on its failure paths. Transport configuration,
optional Dolby control packets and content-information callbacks retain their
native sequencing. HDR publication uses primary-input passthrough/debug selection
and the last matching level-6 extension. Four explicit LE64 copies at offsets
0, 8, 16 and 21 preserve the final overlapping HDR store.

## Utility interfaces

[dm_configuration.h](../core/dm_configuration.h),
[dm_frame_format.h](../core/dm_frame_format.h) and
[dm_transfer.h](../core/dm_transfer.h) declare signal, frame-format and transfer
selection. [control_memory.h](../core/control_memory.h) declares memory sizing
and kernel-state accessors. Minimum sizing uses unsigned 32-bit dimensions and
result words in both versions. Products wrap at 32 bits; planar 4:2:0 minimum
sizing divides the signed interpretation of the wrapped luma size toward zero.
Compact row-pitch sizing instead halves each signed dimension before multiplying.

STB 2.4 uses native gray color `4` and includes planar chroma storage; STB 2.6
uses gray color `5` and omits planar gray chroma. Both request APIs store an
unsigned in-device word without changing its bits. Native manager capacities,
signed indexing, count reloads and failure/partial-write behavior differ.
`InitMmg` returns its manager pointer in both versions. Manager initialization
writes and capacities remain distinct.

STB 2.4 configuration getters select valid pending configuration, otherwise
valid active configuration. A null selected pointer fails without falling back.
`InitAlpSigEnv` and `InitDmCtrl` preserve unassigned fields, including caller-owned
pitches. `GetHDmKs` reads the singleton kernel pointer from a valid context.

STB 2.6 signal getters select one of six `0x120`-byte records. Null context returns
failure/null; an out-of-range index with nonnull context reaches the fatal path.
Control/tone getters copy native records. `GetHDmCfg` returns context plus eight
when nonnull. `GetHDmKs` reads its signed index-minus-one pointer slot without
checking context or index.

[kernel_map_serialization.h](../core/kernel_map_serialization.h) declares complete,
disjoint-buffer packing/unpacking without a capacity argument. STB 2.4 has a
variable-size stream selected by secondary input and graphics color, preserves
existing frame dimensions/pitches on load and sets output dimensions from primary
input. STB 2.6 uses a fixed `0x15d0`-byte stream and retains native field order/frame
state. Its kernel output-map pointer must already address 112 readable bytes for
dumping or 112 writable bytes for loading. Loading first clears that existing map,
then restores the stream map at kernel offset `0x1568` and retargets the output-map
pointer at `0x15f8` to this internal storage. The dump getters return unsigned byte
counts; STB 2.6 additionally selects an indexed kernel slot and traps for indices
at least four.

## Commit and output ownership

[register_commit.c](../core/register_commit.c) invokes concrete display-management
algorithms through [dm_bindings.h](../core/dm_bindings.h).
`dm_commit_pipeline.c` coordinates color-matrix setup, graphics/video inputs,
configuration tone maps, chroma resampling and dynamic metadata. Configuration-
change and metadata-change gates remain ordered. Stages share context, cached
pointers and callback order.

STB 2.4 commits a 984-byte pending configuration, packs composer/register scratch
and fills two 5,120-byte host LUT regions. Its outer callback prepares HDMI data
and returns the commit result. CPU identifiers outside `0x28`, `0x29` and `0x2b`
clear composer, DM register 1 and LUT 1 regions. Other bytes remain unless their
selected branch writes them; the full output structure is not refreshed.

STB 2.4 raw HDMI publication calculates word count before payload writes. Above
512 words it caps length and retains the prior payload after clearing extension
availability. The raw register serializer can exceed its nominal 844-byte region
for oversized metadata. Respect the extents and disjoint-object requirements in
[register_commit.h](../core/register_commit.h).

STB 2.6 uses multiple source/signal/kernel contexts and separate Core 1 result
slots. Metadata decoding separates fixed prefix and extension records; output
generation separates fixed headers and extensions through explicit cursor/capacity
state. Capacity checks remain interleaved with writes, including failure paths.
`dm_metadata_output.c` selects/serializes extensions. `dm_trim*` files separate trim
selection, luminance/chroma coordinates and interpolation. `dm_workspace.h` and
`dm_map_workspace.h` assert native overlapping views; `dm_backlight.c` handles
adaptive backlight/global dimming, and `dm_kernel_maps.c` manages kernel records.
`dm_layout.h` names established fields while reserved spans retain native layout.

## Shared tables and storage

Named table/state members provide stable addresses and explicit element widths.
Constant tables and strings are read-only. The host callback table is a separate
typed persistent object in `callback_registration.c`.

Each module retains its own storage owner and packed layout. Shared table values
do not merge mutable instances or alter lifetime. STB 2.6 owns contiguous native
control storage with asserted offsets and size, mutable version/chip-name buffers,
one set of PQ arrays and shared display records. It has no duplicate display/
PQ mirrors, indirect-call descriptors or unused callback table. Preserve relative
offsets, aliasing and load/store order in both modules.

## STB version contracts

| Operation | STB 2.4 | STB 2.6 |
| --- | --- | --- |
| Control entry | One source plus a graphics descriptor; reset by input format -1 | Three private input records, selected primary input, multiple source/signal contexts; reset by zero inputs |
| CLI storage / priority | 112 bytes / byte 52 | 7,468 bytes / byte 0x1d14 |
| Composer register buffer / first composer word | 900 bytes / byte 176 | 968 bytes / byte 200 |
| Composer mode word | Nonzero `disable_residual_flag` becomes 1 | Always 0 |
| Composer preparation | Source configuration is read by the packer | Selected 8-bit BL/EL configurations can be adjusted in place to 10 bits before packing |
| Memory-manager requests | 32 slots; signed indexing is not trapped | 24 slots; bounds traps and interleaved partial writes are retained |
| Configuration getter | Select pending or active configuration by validity | Return context + 8 when nonnull |
| Kernel-state getter | Read the stored singleton pointer | Read the native signed index-minus-one pointer slot |
| VSVDB null / invalid identifier | -1 / -1 | 0 with retained state / -8 |
| Target luminance storage | Target descriptor uses decimal-scaled luminance | Control state retains the native fixed-point luminance words |
| Output-format getters | Both 16-byte output records are required | Optional expanded HDMI/core output records retain PQ, luminance and primaries fields |
| HDR primary source | Fixed primaries or HDR10 input by source format | Target primaries, with native HDR10 passthrough selection |
| Level-6 HDR extensions | Process matching records in order | Select the last matching record before applying its values |
| Raw HDMI publication | Calculate size first; cap overflow at 512 words and retain previous payload | Preserve capacity traps interleaved with raw-word and extension writes |
| Content information | No STB 2.6 notification interface | Publish content type, desired white point and two level-11 bytes through the stored callback |

`control_registers.c` and its declarations match between modules.
`composer_contract.h` selects only native buffer extent, composer-word base and
mode rule. The common pointer/dimension API returns a constant-zero
unsigned-short result. `composer_registers_abi.h` names configuration fields and
derives output offsets from the base. STB 2.4 callers require disjoint objects;
the common implementation also preserves STB 2.6 interleaved overlapping reads/
writes. Do not bulk-snapshot sources or add `restrict` to these pointers.

LUT packing uses one byte-access implementation. Callers select native tone-map/
input-map bases: kernel plus 1328/200 for STB 2.4 and `0x540`/`0xd8` for STB 2.6.
Four 15-bit pair streams and the final 32-bit map copy retain native output order.
Pointer parameters describe buffers; scalar widths describe stored words.

`control_cli.h` uses bytewise little-endian accessors. `control_cli_contract.h`
names priority and the native priority-override reserved word without assigning
meaning to unknown fields. STB 2.6 metadata slots have `0x1000`-byte
stride; input/source indices, byte offsets, cursors and signed sizes remain
separate values. Fatal checks retain earlier writes.
