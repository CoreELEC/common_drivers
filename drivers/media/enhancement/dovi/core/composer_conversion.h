#ifndef DOVI_COMPOSER_CONVERSION_H
#define DOVI_COMPOSER_CONVERSION_H

#define DOVI_COMPOSER_CONVERSION_HEADER_MIN 0xf1u
#define DOVI_COMPOSER_CONVERSION_PAYLOAD_MIN 0x68c8u
#define DOVI_COMPOSER_CONVERSION_OUTPUT_MIN 0x708u

/*
 * Convert little-endian header/payload layouts into a sparse composer
 * configuration. Inputs and output must be accessible, disjoint and stable.
 * Unselected fields, padding, unused coefficients and disabled filter arrays
 * remain untouched. Counts outside nominal limits require backing for every
 * accessed byte. Fatal limit checks follow parser_contract.h and preserve
 * partial writes.
 */
void dovi_comp_rpu_2_main_cfg(const void *header, const void *payload, void *output);

#endif
