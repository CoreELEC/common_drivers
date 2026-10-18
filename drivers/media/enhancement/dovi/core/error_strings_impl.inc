/* Preserve the shared pointer in table slots -19..-15 and the fallback path. */
static const char unknown_error[] = "Unknown error_code";

/* Error codes -22 through zero. */
static const char *const error_text[23] = {
    "The nalu input buffer is full. Increase NUM_RPU_BUFFERS?", /* -22 */
    "End of stream",                                         /* -21 */
    "Need more data",                                        /* -20 */
    unknown_error,                                            /* -19 */
    unknown_error,                                            /* -18 */
    unknown_error,                                            /* -17 */
    unknown_error,                                            /* -16 */
    unknown_error,                                            /* -15 */
    "Value is out of it's legal range",                       /* -14 */
    "Mapping or NLQ pivot values are to large for current bitdepth", /* -13 */
    "Output buffer is too small",                            /* -12 */
    "DM metadata extension can be 1 or 2 only",                /* -11 */
    "invalid DM metadata extension size",                     /* -10 */
    "Wrong RPU version number",                              /* -9 */
    "Unspecified error",                                     /* -8 */
    "Invalid argument",                                      /* -7 */
    "The rpu stream violates the spatial filter and mode limitations", /* -6 */
    "The rpu stream violates the profile limitations",        /* -5 */
    "The rpu stream violates the level limitations",          /* -4 */
    "Not supported",                                         /* -3 */
    "Invalid RPU, skip frame",                                /* -2 */
    "Not enough memory available",                           /* -1 */
    "RPU_ERR_NONE"                                           /* 0 */
};

const char *dovi_rpu_error_code_2_str(int code)
{
    /* Unsigned addition also rejects values outside the signed error range. */
    unsigned int index = (unsigned int)code + 22u;
    return index <= 22u ? error_text[index] : unknown_error;
}
