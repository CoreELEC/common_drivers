#ifndef DOVI_FIXED_TRANSCENDENTALS_H
#define DOVI_FIXED_TRANSCENDENTALS_H

/* Fixed-point integer approximations. Scalar widths retain the native contracts. */

void dovi_fx_assert(int test, const char *message);
int dovi_fxLOG2Taylor(unsigned int raw_value, unsigned int raw_scale_out);
int dovi_fxPOW2Taylor(unsigned int raw_value, unsigned int raw_scale_in,
                      unsigned int raw_scale_out);
int dovi_powTaylor(unsigned int value, unsigned int exponent);
long long dovi_LfxPOW2Taylor(unsigned int raw_value, unsigned int raw_scale_in);
int dovi_expNTaylor(unsigned int raw_value, unsigned int raw_scale_out);
unsigned long long dovi__powInSTaylor(unsigned long long raw_value, unsigned int raw_scale);
unsigned int dovi_ssqrt_s(unsigned int value);
int dovi_log10Taylor(unsigned int value);

#endif
