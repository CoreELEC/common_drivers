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
/* The loop executes (scale-2) modulo 2^32 times, with zero meaning 2^32.
 * Small/negative scale values therefore remain extremely long-running. */
unsigned long long dovi__powInSTaylor(unsigned long long raw_value, unsigned int raw_scale);
unsigned int dovi_ssqrt_s(unsigned int value);
unsigned long long dovi_log10Taylor(unsigned int value);

unsigned long long dovi_powIn4Taylor(unsigned long long value, unsigned int exponent);
unsigned long long dovi_powInExp2Taylor(unsigned int value, unsigned long long exponent);
unsigned long long dovi_powIn4Exp2Taylor(unsigned long long value, unsigned long long exponent);
unsigned long long dovi_powIn4Exp4Taylor(unsigned long long value, unsigned long long exponent);
unsigned long long dovi_powIn8Exp2Taylor(unsigned long long value, unsigned long long exponent);
unsigned long long dovi_powIn32Exp2Taylor(unsigned long long value, unsigned long long exponent);
unsigned long long dovi_powInsExpsTaylor(unsigned int value, unsigned int input_fraction,
                                         unsigned int exponent, unsigned int exponent_fraction);
unsigned long long dovi_log10TaylorB(unsigned int value);
unsigned int dovi_expNTaylorIn2(unsigned int value);
unsigned int dovi_expNTaylorIn8(unsigned int value);
unsigned int dovi_ATan2(unsigned int y, unsigned int x);
unsigned long long dovi_powIn2Exp128Taylor(unsigned long long value, unsigned long long exponent);
unsigned long long dovi_powIn16Exp8Taylor(unsigned long long value, unsigned long long exponent);

#endif
