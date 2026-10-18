# STB 2.4 core source map

Metadata processing converts an RPU into composer configuration and packed
display metadata. Control processing consumes those records and display
settings to produce hardware register and LUT contents. Kernel services enter
through [runtime_bridge.c](../runtime_bridge.c); the host callback adapter is
[host_adapter.c](../host_adapter.c).

## Metadata path

1. [metadata_init_body.c](metadata_init_body.c) initializes the application
   singleton; [metadata_runtime.c](metadata_runtime.c) implements host reset,
   process and release. `module_parser_context.h` defines its output backing
   storage and persistent counters.
2. [parser_lifecycle.c](parser_lifecycle.c), [parser_state.c](parser_state.c)
   and [parser_process.c](parser_process.c) initialize, reset and dispatch the
   parser/decoder singleton. `parser_context.h` and `parser_layout.h` describe
   its configuration and packed arena fields.
3. [rpu_buffering.c](rpu_buffering.c) queues input;
   [rpu_pipeline.c](rpu_pipeline.c) runs header, level, spatial, payload,
   profile, MEL and display-metadata stages.
4. [metadata_callbacks.c](metadata_callbacks.c) converts and validates composer
   configuration, then validates and serializes display metadata into caller
   output.

`parser_contract.h` specifies STB 2.4 reset, input accounting, extension and
diagnostic behavior. `nlq_layout.h` names prediction/parameter unions and access
widths. See [Parser and metadata](../docs/parser.md) for lifetimes, backing
extents and malformed-input behavior.

## Control and display management

[host_control.c](host_control.c) translates host signal formats;
[control_runtime.c](control_runtime.c) owns single-input storage and copied
services. [control_path.c](control_path.c), [control_memory.c](control_memory.c),
[control_state.c](control_state.c) and [control_target.c](control_target.c)
initialize the arena and configure the source, graphics and display target.

[register_commit.c](register_commit.c) invokes display management and packs
registers/LUTs. [dm_commit_pipeline.c](dm_commit_pipeline.c) coordinates
configuration, metadata, color, map, tone and trim stages.
`control_setting_layout.h`, `control_abi_recovered.h` and `dm_layout.h` name
packed fields while preserving reserved and overlapping storage.

| Area | Source units |
| --- | --- |
| Bit input and decoding | `bitstream`, `align_to_byte`, `crc32`, `decoder_lifecycle`, `memory_query`, `rpu_header`, `rpu_payload` |
| Display metadata | `dm_payload`, `ext_blocks`, `dm_extensions`, `dm_serialization`, `dm_struct_conversion`, `supplemental_dispatch`, `supplemental_metadata` |
| Composition and publication | `composer_conversion`, `composer_validation`, `control_registers`, `register_commit`, `control_packets`, `control_publish` |
| Signal and maps | `dm_configuration`, `dm_frame_format`, `dm_transfer`, `dm_input_map`, `dm_output_map`, `dm_kernel_maps`, `kernel_map_serialization` |
| Numeric transforms | `color_basis`, `color_narrow`, `color_primaries`, `numeric_color_matrix`, `matrix_arithmetic`, `fixed_arithmetic`, `fixed_transcendentals`, `numeric_transfer`, `pq_conversion` |
| Tone and trim | `tone_controls`, `tone_parameters`, `tone_curve`, `tone_curve_lut`, `tone_maps`, `trim_reduction`, `dm_trim`, `dm_trim_chroma` |

Bytewise access and raw arithmetic helpers live in `fixed_access.h`,
`fixed_bits.h` and `numeric_common.h`. Implementation includes and table-value
includes retain explicit integer widths and native table ownership. Mutable
parser and control storage have separate singleton owners. No executable
binary data or precompiled processing object is included.

Read the [runtime](../docs/runtime.md), [control](../docs/control.md) and
[numeric](../docs/numerics.md) contracts before changing layouts, integer
types, pointer reloads, table indices or call order.
