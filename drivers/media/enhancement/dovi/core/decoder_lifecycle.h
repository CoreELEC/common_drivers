#ifndef DOVI_DECODER_LIFECYCLE_H
#define DOVI_DECODER_LIFECYCLE_H

#define DOVI_DECODER_BASE_SIZE 0x316e8u
#define DOVI_DECODER_INIT_CONFIG_MIN_SIZE 0x18u

/* Initialize caller-owned decoder storage using 64-bit little-endian layouts.
 * Decoder and config storage must be disjoint and externally serialized.
 * No allocation happens here.
 * Setters store callback addresses and severity through byte accesses without
 * requiring aligned decoder storage. Decoding invokes callbacks through their
 * declared function types; logger return values are ignored.
 */

/*
 * config provides at least DOVI_DECODER_INIT_CONFIG_MIN_SIZE readable bytes
 * of the disjoint rpu_init_flags_t layout.
 * Reads LE u32 level (+4), disable_crc_check (+8), payload_only (+0xc), and
 * disable_MEL_detection (+0x14). Supported level indices are 0..15.
 * decoder must be writable for the arena described in ../docs/parser.md.
 * Initialization requires a valid level index and returns zero after
 * installing the native decoder state.
 * Levels 3..15 retain the fixed 0xe070-byte payload clear beyond the
 * memory query; their callers must provide that additional backing.
 */

typedef enum {
    S_RPU_EMERGENCY = 0,
    S_RPU_ALERT = 1,
    S_RPU_CRITICAL = 2,
    S_RPU_ERROR = 3,
    S_RPU_WARNING = 4,
    S_RPU_NOTICE = 5,
    S_RPU_INFORMATIONAL = 6,
    S_RPU_DEBUG = 7,
    S_RPU_NEVERPRINT = 8,
} rpu_print_severity_t;

typedef int (*rpu_msg_log_func_t)(const char *format, ...);
typedef void (*dec_event_callback_t)(rpu_print_severity_t severity, unsigned int code,
                                     char *message);

/* Layout used by the decoder flush interface. */
struct rpu_setter_view {
    unsigned char before_last[0x28];
    int last; /* +0x28 */
    unsigned char before_msg_log[0x3159c];
    rpu_msg_log_func_t msg_log;          /* +0x315c8 */
    rpu_print_severity_t print_severity; /* +0x315d0 */
    unsigned char before_event_callback[0x104];
    dec_event_callback_t event_callback; /* +0x316d8 */
    unsigned char after_event_callback[8];
};

typedef char setters_pointer_is_eight_bytes[(sizeof(void *) == 8) ? 1 : -1];
typedef char setters_int_is_four_bytes[(sizeof(int) == 4) ? 1 : -1];
typedef char setters_severity_is_four_bytes[(sizeof(rpu_print_severity_t) == 4) ? 1 : -1];
typedef char setters_view_is_202472_bytes[(sizeof(struct rpu_setter_view) == 0x316e8) ? 1 : -1];
#if defined(__GNUC__) || defined(__clang__)
typedef char
    setters_last_offset[(__builtin_offsetof(struct rpu_setter_view, last) == 0x28) ? 1 : -1];
typedef char setters_msg_log_offset[(__builtin_offsetof(struct rpu_setter_view, msg_log) == 0x315c8)
                                        ? 1
                                        : -1];
typedef char setters_severity_offset
    [(__builtin_offsetof(struct rpu_setter_view, print_severity) == 0x315d0) ? 1 : -1];
typedef char setters_event_offset
    [(__builtin_offsetof(struct rpu_setter_view, event_callback) == 0x316d8) ? 1 : -1];
#endif

int dovi_rpu_decoder_init(void *decoder, const unsigned char *config);
int dovi_rpu_decoder_cleanup(void *decoder);
void dovi_rpu_decoder_set_msg_log(void *decoder, rpu_msg_log_func_t msg_log,
                                  unsigned int print_severity);
void dovi_register_dec_event_callback(void *decoder, dec_event_callback_t callback);

#endif
