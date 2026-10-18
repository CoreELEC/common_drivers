/* Default RPU header fields and unavailable-ID diagnostics. */
static void unavailable_id_message(char *message, byte id)
{
    static const char prefix[] = "RPU ID ";
    static const char suffix[] = " not available";
    volatile char *zero = message;
    u32 i, pos = 0;
    for (i = 0; i < 256u; ++i)
        zero[i] = 0;
    for (i = 0; prefix[i] != 0; ++i)
        message[pos++] = prefix[i];
    if (id >= 100u)
        message[pos++] = (char)('0' + id / 100u);
    if (id >= 10u)
        message[pos++] = (char)('0' + (id / 10u) % 10u);
    message[pos++] = (char)('0' + id % 10u);
    for (i = 0; suffix[i] != 0; ++i)
        message[pos++] = suffix[i];
}

static void default_sequence(byte *header)
{
    header[0x14] = 0;
    header[0x15] = 0;
    header[0x07] = 0;
    header[0x08] = 0;
    header[0x09] = 0;
    header[0x0a] = 15;
    header[0x0d] = 1;
    header[0x0e] = 0;
    header[0x0f] = 10;
    header[0x10] = 10;
    header[0x11] = 14;
    header[0x12] = 0;
    header[0x16] = 1;
}

static void default_mapping(byte *header)
{
    u32 bl_depth = header[0x0f], el_depth;
    u32 limit;
    write16(header + 0x1c, 0);
    write16(header + 0x28, 0);
    header[0x25] = 2;
    header[0x26] = 2;
    header[0x27] = 2;
    if (bl_depth < 8u) {
        bl_depth = 8;
        header[0x0f] = 8;
    }
    write16(header + 0x48, 0);
    limit = ~(0xffffffffu << (bl_depth & 31u));
    el_depth = header[0x10];
    write16(header + 0x68, 0);
    header[0xf0] = 0;
    header[0xf1] = 2;
    write16(header + 0x2a, limit);
    write16(header + 0x4a, limit);
    write16(header + 0x6a, limit);
    if (el_depth < 8u) {
        el_depth = 8;
        header[0x10] = 8;
    }
    /* Exactly +0xf2; the next entry at +0xf4 is deliberately untouched. */
    write16(header + 0xf2, ~(0xffffffffu << (el_depth & 31u)));
    header[0x133] = 1;
    header[0x134] = 1;
}
