/* Default DM payload fields; padding remains untouched. */
static void default_dm_payload(byte *dm)
{
    /* Write the fields without changing padding bytes. */
    write16(dm, 0);
    write64(dm + 0x04, 0x2567399600002567ull);
    write64(dm + 0x0c, 0x43dc2567eee1f926ull);
    write64(dm + 0x18, 0x2000000004000000ull);
    write32(dm + 0x20, 0x20000000u);
    dm[0x02] = 0;
    write64(dm + 0x24, 0x0a08034525e616d5ull);
    write64(dm + 0x34, 0x00e43dfb999a3d59ull);
    write16(dm + 0x14, 0);
    write32(dm + 0x44, 0x0e70003eu);
    write64(dm + 0x2c, 0x02a7000006192fe0ull);
    write64(dm + 0x3c, 0x0100000e00151cb4ull);
    write16(dm + 0x48, 0x2a);
    dm[0x4a] = 0;
}
