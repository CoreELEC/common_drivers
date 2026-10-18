#ifndef DOVI_REGISTER_COMMIT_H
#define DOVI_REGISTER_COMMIT_H

/* Register commit. Registers cover 900,
 * 96 and 1024 bytes; LUTs cover 5120 bytes each. Raw metadata serialization
 * is unchecked and may exceed the nominal 844-byte raw register array; the
 * caller must supply every byte accessed. Objects must remain disjoint.
 */
int dovi_commit_reg(void *context, int input_format, int output_format, void *metadata,
                    const void *composition, const void *target, const void *graphics,
                    const void *source, int priority, void *register1, void *register2,
                    void *register3, void *lut1, void *lut2);

#endif
