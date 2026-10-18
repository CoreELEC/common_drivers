/* Update tone and saturation curves from their control inputs. */
#include "tone_curve.h"
#include "control_memory.h"

int dovi_CreateToneCurve(void *control, void *output, unsigned int mode, void *curve,
                                  void *context)
{
    void *parameters;
    unsigned short first_lut_sample;
    unsigned int change_flags;

    (void)mode;
    (void)context;
    parameters = (void *)((long)control + 0x1650);
    change_flags = dovi_MemEqByte(curve, control, 0x1a0);
    if ((change_flags == 0) ||
        (change_flags = dovi_MemEqByte((void *)((long)curve + 0x1650), parameters, 0xf8),
         change_flags == 0)) {
        dovi_curve_prepare_parameters(control, parameters);
        dovi_curve_build_luts(control, output, parameters);
        dovi_curve_pack_luts(control, output);
        first_lut_sample = *(unsigned short *)output;
        *(unsigned short *)((long)control + 0xe2e) = first_lut_sample;
        *(unsigned short *)((long)control + 0xe2c) = first_lut_sample;
        *(unsigned short *)((long)control + 0x21e) = first_lut_sample;
        *(unsigned short *)((long)control + 0x21c) = first_lut_sample;
        dovi_MemCpyByte(curve, control, 0x1a0);
        change_flags = 0x100000;
        if (*(int *)((long)control + 0x220) != *(int *)((long)control + 0xa1c)) {
            change_flags = 1;
        }
    } else {
        change_flags = 0;
    }
    return change_flags;
}
