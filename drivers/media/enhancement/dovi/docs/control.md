# STB 2.4 control and register output

## Host boundary

[host_adapter.c](../host_adapter.c) receives the host control arguments and
delegates to [host_control.c](../core/host_control.c). The adapter translates
SDR BT.2020 from host enum value `6` to core value `4`; values `-1` through `3`
retain their meaning. Unsupported formats return `-1`, except that input
format `-1` selects reset without requiring an ordinary output format.

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

## Persistent storage and lifecycle

[control_runtime.c](../core/control_runtime.c) owns aligned static storage:

| Buffer | Bytes |
| --- | --- |
| Control arena | 20,040 |
| CLI parameters | 112 |
| Current and previous metadata | 66,626 each |
| Composition | 1,800 |
| Register scratch 1 / 2 / 3 | 900 / 96 / 1,024 |

Services are copied before registration. CPU identification, logging and
diagnostic callbacks must remain valid. The runtime and chroma scratch are
singletons with no internal locking. Callers serialize control calls, avoid
reentry and retain writable output objects for the full call. Memory requests
describe register/LUT processing, not CPU image-pixel processing. Packed
load/store order matters where views overlap.

[control_path.c](../core/control_path.c) initializes the arena on first use.
Input format `-1` invokes the no-op `dovi_destroy_cp` and clears the initialized
flag. Ordinary completion also invokes that helper but leaves the flag set.
The helper takes no arguments and returns zero.

Composition records advance in 1,800-byte steps. Loop termination compares
consumed composition and metadata offsets for equality with supplied lengths.
Callers supply complete readable records and consistent lengths; these are
not general physical bounds checks. HDR10 input covers 28 readable bytes even
when a selected branch does not produce an HDR10 infoframe.

## Display capabilities and metadata

[control_target.c](../core/control_target.c) consumes complete 32-byte VSVDB
storage without a physical length argument and writes an 88-byte target
descriptor. Version 0 reads 21 bytes, long version 1 reads 15, and compact
version 1 or version 2 reads 12. Input and target storage must be disjoint.

Null or invalid identification returns `-1` without target writes. Unsupported
versions retain capability fields written before failure; unsupported
version-1 length also retains four luminance/PQ writes. Width, height and
low-latency request words are preserved. Target luminance uses decimal-scaled
values with the native integer rounding and write order.

[control_packets.c](../core/control_packets.c) builds the 29-byte HDR infoframe
and fixed metadata. [control_metadata.c](../core/control_metadata.c) filters
packed metadata and prepares HDMI output. Luminance getters require valid
active DM configuration because callers use their results without handling
failure first. HDR primary selection uses fixed primaries or HDR10 input
according to source format; matching level-6 extensions are processed in order.

Trim reduction uses at most sixteen level-2 and sixteen level-8 records in
local arrays. Its target argument addresses the stored 64-bit target pointer.
See `trim_reduction.h` for sorting and storage
requirements.

## Utility interfaces

[dm_configuration.h](../core/dm_configuration.h),
[dm_frame_format.h](../core/dm_frame_format.h) and
[dm_transfer.h](../core/dm_transfer.h) declare signal, frame-format and transfer
selection. [control_memory.h](../core/control_memory.h) declares memory sizing
and kernel-state accessors. Minimum sizing uses unsigned 32-bit dimensions and
result words. Products wrap at 32 bits; planar 4:2:0 minimum sizing divides the
signed interpretation of wrapped luma size toward zero. Compact row-pitch
sizing instead halves each signed dimension before multiplying. Gray color is
`4`, and sizing includes planar chroma storage.

The memory manager has 32 request slots and retains unchecked signed indexing.
Request APIs store the unsigned in-device word without changing its bits and
retain parameter/callback addresses. `dovi_InitMmg` returns the manager pointer.

Configuration getters select valid pending configuration, otherwise valid
active configuration. A null selected pointer fails without falling back.
`InitAlpSigEnv` and `InitDmCtrl` preserve unassigned fields, including caller-owned
pitches. `GetHDmKs` reads the singleton kernel pointer from a valid context.

[kernel_map_serialization.h](../core/kernel_map_serialization.h) declares
disjoint-buffer packing/unpacking without a capacity argument. Its variable-size
stream is selected by secondary input and graphics color. Loading preserves
existing frame dimensions/pitches and sets output dimensions from primary input.
Dump getters return unsigned byte counts. Callers must provide complete backing
for the selected stream.

## Commit and output ownership

[register_commit.c](../core/register_commit.c) invokes display-management
algorithms through [dm_bindings.h](../core/dm_bindings.h).
[dm_commit_pipeline.c](../core/dm_commit_pipeline.c) coordinates color-matrix
setup, graphics/video inputs, configuration tone maps, chroma resampling and
dynamic metadata. Configuration-change and metadata-change gates remain
ordered. Stages share context, cached pointers and callback order.

Commit copies a 984-byte pending configuration, packs composer/register scratch
and fills two 5,120-byte host LUT regions. The outer callback prepares HDMI data
and returns the commit result. CPU identifiers outside `0x28`, `0x29` and `0x2b`
clear composer, DM register 1 and LUT 1 regions. Other bytes remain unless their
selected branch writes them; the full output structure is not refreshed.

Raw HDMI publication calculates word count before payload writes. Above
512 words it caps length and retains the prior payload after clearing extension
availability. The raw register serializer can exceed its nominal 844-byte region
for oversized metadata. Respect the extents and disjoint-object requirements in
[register_commit.h](../core/register_commit.h).

## Packed records and tables

`composer_contract.h` selects a 900-byte composer register buffer with the
first composer word at byte 176. A nonzero `disable_residual_flag` produces
composer mode 1. `composer_registers_abi.h` names configuration fields and
derives output offsets from that base. The packer receives disjoint composer
and output objects, accepts unaligned buffers and returns unsigned-short zero.
DM registers, reserved words and unused composer fields remain untouched.

LUT packing reads tone/input maps at kernel offsets 1328/200. Four 15-bit pair
streams and the final 32-bit map copy retain their native output order.
`control_cli.h` uses bytewise little-endian accessors; CLI priority is byte 52.
`control_cli_contract.h` also names the priority-override reserved word.

Named table/state members provide stable addresses and explicit element widths.
Constant tables and strings are read-only. The host callback table is a separate
typed persistent object in `callback_registration.c`. Independently aligned
storage owners retain their lifetime, aliasing and load/store order.
