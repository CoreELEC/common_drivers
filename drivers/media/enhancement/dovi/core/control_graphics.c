#include "control_state.h"
#include "fixed_access.h"

static unsigned int read32(const void *object, unsigned int offset)
{
    return fixed_load_le32((const unsigned char *)object + offset);
}

static void write32(void *object, unsigned int offset, unsigned int value)
{
    fixed_store_le32((unsigned char *)object + offset, value);
}

void populate_graphics_param_body(void *graphics, const void *cli)
{
    unsigned int format = read32(cli, CONTROL_CLI_G_FORMAT_OFFSET);
    write32(graphics, CONTROL_GRAPHICS_ON_OFFSET, read32(cli, CONTROL_CLI_F_GRAPHIC_ON_OFFSET));
    write32(graphics, CONTROL_GRAPHICS_MIN_OFFSET, read32(cli, CONTROL_CLI_G_MIN_LUM_OFFSET));
    write32(graphics, CONTROL_GRAPHICS_MAX_OFFSET, read32(cli, CONTROL_CLI_G_MAX_LUM_OFFSET));
    write32(graphics, CONTROL_GRAPHICS_BIT_DEPTH_OFFSET,
            read32(cli, CONTROL_CLI_G_BIT_DEPTH_OFFSET));
    write32(graphics, CONTROL_GRAPHICS_CHROMA_FORMAT_OFFSET, read32(cli, CONTROL_CLI_G_CF_OFFSET));
    if (format <= 3u) {
        unsigned int hdr = format >= 2u;
        write32(graphics, CONTROL_GRAPHICS_GAMMA_OFFSET, hdr ? 0u : 0x999au);
        write32(graphics, CONTROL_GRAPHICS_A_OFFSET, hdr ? 0u : 0x181u);
        write32(graphics, CONTROL_GRAPHICS_B_OFFSET, hdr ? 0u : 0x433u);
        write32(graphics, CONTROL_GRAPHICS_G_OFFSET, hdr ? 0u : 0x151cb4u);
        write32(graphics, CONTROL_GRAPHICS_EOTF_OFFSET, hdr);
        write32(graphics, CONTROL_GRAPHICS_YUV_RNG_OFFSET, format & 1u);
        write32(graphics, CONTROL_GRAPHICS_CLR_OFFSET, format & 1u);
        write32(graphics, CONTROL_GRAPHICS_XFER_SPEC_OFFSET, hdr ? 3u : 2u);
        write32(graphics, CONTROL_GRAPHICS_RGB_DEF_OFFSET, hdr ? 3u : 2u);
    }
}
