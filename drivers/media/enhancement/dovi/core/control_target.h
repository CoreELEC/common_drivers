#ifndef DOVI_CONTROL_TARGET_H
#define DOVI_CONTROL_TARGET_H

/* Decoding accepts a null display block and leaves the stored block intact.
 * An invalid block returns the same status as the register-commit entry. */
int dovi_decode_display_block(void *context, const unsigned char *vsvdb);
void dovi_configure_output_target(void *context, const unsigned char *input_configuration,
                                  const int *target_parameters, const unsigned char *vsvdb);
#endif
