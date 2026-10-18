#ifndef DOVI_REGISTER_COMMIT_H
#define DOVI_REGISTER_COMMIT_H

/* Register commit for the selected inputs and output target. */
int dovi_commit_reg(void *context, const unsigned char *input_configuration, void *sources,
                    const int *target_parameters, const unsigned char *vsvdb,
                    void *register1, void *register2, void *register3, void *luts);

#endif
