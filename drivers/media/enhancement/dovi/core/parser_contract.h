/* Parser behavior selected by the Dolby module interface, not the kernel. */
#ifndef DOVI_PARSER_CONTRACT_H
#define DOVI_PARSER_CONTRACT_H

#define PARSER_VUI_ARGUMENT 0
#define PARSER_RESET_FLAGS 1
/* Application storage and metadata length widening are independent of reset flags. */
#define PARSER_ALLOCATED_APPLICATION_STORAGE 1
#define PARSER_SIGN_EXTEND_METADATA_COPY_LENGTH 1
#define PARSER_DRAIN_AFTER_ERROR 1
#define PARSER_FATAL_ARRAY_LIMITS 1
#define PARSER_NLQ_TRUNCATING_SIGN_SHIFT 1
#define PARSER_DECODER_EXTENSION_API 1
#define PARSER_NONFIRST_EXTENSION_KIND 1
#define PARSER_BOUNDED_UNKNOWN_EXTENSION 1
#define PARSER_DM_INITIAL_EXTENSION_ACCOUNTING 0
#define PARSER_DM_SECOND_UPDATE_ACCOUNTING 0
#define PARSER_WRAPPING_SEQUENCE_OFFSET 0
#define PARSER_DM_EXTENSION_LINE_BASE 0x585u
#define PARSER_DECODER_DIAGNOSTIC_SOURCE "../dolby_vision/dv_source_code/libs/vdr_rpu/src/rpu_decoder.c"
#define PARSER_HEADER_ERROR_LINE 0x22au
#define PARSER_PAYLOAD_ERROR_LINE 0x23bu
#define PARSER_DM_ERROR_LINE 0x248u
#define PARSER_DIAGNOSTIC_SOURCE "../dolby_vision/dv_source_code/libs/vdr_rpu/src/rpu_parser.c"

#endif
