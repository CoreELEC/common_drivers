# Core source map

Metadata processing converts an RPU into composer configuration and packed
display metadata. Control processing consumes those records and display
settings to produce hardware register and LUT contents. Kernel services enter
through [runtime_bridge.c](../runtime_bridge.c).

## Metadata path

1. [metadata_init_body.c](metadata_init_body.c) initializes the application
   pipeline; [metadata_runtime.c](metadata_runtime.c) implements host reset,
   process and release. `module_parser_context.h` preserves each application's
   native output storage and running sizes.
2. [parser_lifecycle.c](parser_lifecycle.c), [parser_state.c](parser_state.c)
   and [parser_process.c](parser_process.c) share parser initialization, reset
   and dispatch through the common `parser_context.h` and `parser_layout.h`.
3. [rpu_buffering.c](rpu_buffering.c) queues input;
   [rpu_pipeline.c](rpu_pipeline.c) runs header, level, spatial, payload,
   profile, MEL and display-metadata stages.
4. [metadata_callbacks.c](metadata_callbacks.c) converts and validates
   composer configuration and serializes display metadata into caller output.

`parser_contract.h` selects actual Dolby library differences, including reset,
fatal bounds, extension handling, input accounting and diagnostics. STB 2.4
owns singleton application/parser storage; STB 2.6 allocates parser instances.
The shared `nlq_layout.h` preserves prediction/parameter unions and access
widths. See [Parser and metadata](../docs/parser.md) for exact lifetimes,
backing extents and malformed-input behavior.

## Control and display management

STB 2.4 enters through `host_control.c`, which translates host signal formats,
and `control_runtime.c`, which owns single-input storage and copied services.
STB 2.6 enters through its multi-input host adapter and shared storage owners.
Both use [control_path.c](control_path.c), [control_memory.c](control_memory.c),
[control_state.c](control_state.c) and [control_target.c](control_target.c)
for their native source selection, arena initialization and target configuration.

[register_commit.c](register_commit.c) invokes display management and packs
registers/LUTs. [dm_commit_pipeline.c](dm_commit_pipeline.c) retains each
version's configuration, metadata, color, map, tone and trim ordering.
`control_setting_layout.h`, `dm_layout.h` and the workspace headers name
verified fields and preserve reserved and overlapping storage.

| Area | Common source units | Additional version-specific work |
| --- | --- | --- |
| Bit input and decoding | `bitstream`, `align_to_byte`, `crc32`, `decoder_lifecycle`, `memory_query`, `rpu_header`, `rpu_payload` | Policies in `parser_contract.h` |
| Display metadata | `dm_payload`, `ext_blocks`, `dm_extensions`, `dm_serialization`, `supplemental_dispatch`, `supplemental_metadata` | STB 2.6 adds output selection/serialization and input consolidation |
| Composition and publication | `composer_conversion`, `composer_validation`, `control_registers`, `register_commit`, `control_packets`, `control_publish` | Single-source versus multi-source host records and transport/content notification |
| Signal and maps | `dm_configuration`, `dm_frame_format`, `dm_transfer`, `dm_input_map`, `dm_output_map`, `dm_kernel_maps`, `kernel_map_serialization` | Indexed STB 2.6 signal/global-dimming and primary-map composition |
| Numeric transforms | `color_basis`, `color_narrow`, `color_primaries`, `numeric_color_matrix`, `matrix_arithmetic`, `fixed_arithmetic`, `fixed_transcendentals`, `numeric_transfer`, `pq_conversion` | Native table, width, domain, rounding and narrowing differences |
| Tone and trim | `tone_controls`, `tone_parameters`, `tone_curve`, `tone_curve_lut`, `tone_maps`, `dm_trim`, `dm_trim_chroma` | STB 2.4 trim reduction; STB 2.6 luminance/chroma interpolation and adaptive backlight |

Shared bytewise access and raw arithmetic helpers live in `fixed_access.h`,
`fixed_bits.h` and `numeric_common.h`. Shared implementation includes retain
equivalent subalgorithms while wrappers select native policies such as matrix
truncation/saturation and PQ endpoints. Shared table values keep independent
native storage ownership and lifetime. STB 2.6 `original_data.c`,
`data_layout.h` and `shared_data.h` preserve contiguous objects and relative
aliases; STB 2.4 retains its independently aligned owners. No executable
binary data or precompiled processing object is included.

Read the [control](../docs/control.md), [numeric](../docs/numerics.md) and
[comparative changelog](../docs/changes.md) before changing layouts, integer
types, pointer reloads, table indices or call order.
