#ifndef DOVI_FIXED_ARITHMETIC_H
#define DOVI_FIXED_ARITHMETIC_H

/* Raw inputs preserve every native bit pattern. Signed add/subtract and
 * absolute-value results support callers that perform signed arithmetic; the
 * fixed helpers define wrapping and signed minima without signed-C overflow.
 */
int dovi_sadd_sss(unsigned int first, unsigned int second);
int dovi_ssub_sss(unsigned int first, unsigned int second);
unsigned int dovi_uabs_ss(unsigned int value);
unsigned long long dovi_uabs_ls(unsigned long long value);
unsigned long long dovi_smpy_ss(unsigned int first, unsigned int second);
unsigned int dovi_unorm_s(unsigned int value);
unsigned long long dovi_unorm_u(unsigned long long value);
unsigned long long dovi_unorm_u_lut(unsigned long long value);
unsigned int dovi_unorm_sl(unsigned long long value);
unsigned int dovi_unorm_ul(unsigned long long value);
int dovi_sshl_ssu(unsigned int raw_value, unsigned int raw_shift);
int dovi_sshr_sss(unsigned int raw_value, unsigned int raw_shift);
unsigned long long dovi_sdiv_sls(unsigned long long numerator, unsigned int denominator);
int dovi_abs_int32(unsigned int value);
long long dovi_abs_int64(unsigned long long value);
unsigned long long dovi_sdiv(unsigned long long numerator, unsigned int denominator);

/* Products write the low word first, then the high word, in little-endian
 * byte order. Output slots need eight writable bytes each and may overlap.
 * The signed version interprets the inputs as two's-complement 64-bit values.
 */
void dovi_uint64_mul(unsigned long long a, unsigned long long b, void *high, void *low);
void dovi_int64_mul(unsigned long long a, unsigned long long b, void *high, void *low);

/* Preserve these differently defined original functions despite their names:
 * unsigned: (a << ((-shift)&63)) | (b >> (shift&63));
 * signed:   (a << (shift&63)) | arithmetic_shift_right(b, shift&63).
 */
unsigned long long dovi_uint64_lshift(unsigned long long a, unsigned long long b,
                                      unsigned int shift);
unsigned long long dovi_int64_lshift(unsigned long long a, unsigned long long b,
                                     unsigned long long shift);

#endif
