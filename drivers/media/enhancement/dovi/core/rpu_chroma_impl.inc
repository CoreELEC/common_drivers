/* Chroma filter dispatch with ordered inner and outer diagnostics. */
static int chroma_read(byte *decoder, int partition, log_fn outer_log, u32 outer_level)
{
    byte *header = pointer_at(pointer_at(decoder));
    log_fn log = logger_at(decoder + 0x315c8);
    u32 level = read32(decoder + 0x315d0);
    u32 component;
    int result;
    if (header[0x1e] != 14 || header[0x1d] == 0)
        return 0;
    for (component = 0; component < 2; ++component) {
        if (header[0x1d] == 2) {
            result = dovi_rpu_data_chroma_resampling_filter_1D_exp_read(decoder, partition,
                                                                        component, 1);
            if (result < 0) {
                error_line(log, level, 0x21e, result);
                error_line(outer_log, outer_level, 0x3c6, result);
                return result;
            }
        }
        result =
            dovi_rpu_data_chroma_resampling_filter_1D_exp_read(decoder, partition, component, 0);
        if (result < 0) {
            error_line(log, level, 0x221, result);
            error_line(outer_log, outer_level, 0x3c6, result);
            return result;
        }
        if (header[0x1e] != 14 || header[0x1d] == 0)
            break;
    }
    return 0;
}
