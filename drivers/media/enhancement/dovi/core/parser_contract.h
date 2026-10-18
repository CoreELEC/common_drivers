/* Parser behavior selected by the Dolby module interface, not the kernel. */
#ifndef DOVI_PARSER_CONTRACT_H
#define DOVI_PARSER_CONTRACT_H

#define PARSER_VUI_ARGUMENT 1
#define PARSER_RESET_FLAGS 0
/* Application storage and metadata length widening are independent of reset flags. */
#define PARSER_ALLOCATED_APPLICATION_STORAGE 0
#define PARSER_SIGN_EXTEND_METADATA_COPY_LENGTH 0
#define PARSER_DRAIN_AFTER_ERROR 0
#define PARSER_FATAL_ARRAY_LIMITS 0
#define PARSER_NLQ_TRUNCATING_SIGN_SHIFT 0
#define PARSER_DECODER_EXTENSION_API 0
#define PARSER_NONFIRST_EXTENSION_KIND 0
#define PARSER_BOUNDED_UNKNOWN_EXTENSION 0
#define PARSER_DM_INITIAL_EXTENSION_ACCOUNTING 1
#define PARSER_DM_SECOND_UPDATE_ACCOUNTING 1
#define PARSER_WRAPPING_SEQUENCE_OFFSET 1
#define PARSER_DM_EXTENSION_LINE_BASE 0x581u
#define PARSER_DECODER_DIAGNOSTIC_SOURCE "/mnt/fileroot/yao.liu/androidq/hardware/amlogic/dolby_vision/dv_source_code/libs/vdr_rpu/src/rpu_decoder.c"
#define PARSER_HEADER_ERROR_LINE 0x225u
#define PARSER_PAYLOAD_ERROR_LINE 0x236u
#define PARSER_DM_ERROR_LINE 0x243u
#define PARSER_DIAGNOSTIC_SOURCE "/mnt/fileroot/yao.liu/androidq/hardware/amlogic/dolby_vision/dv_source_code/libs/vdr_rpu/src/rpu_parser.c"

#endif
