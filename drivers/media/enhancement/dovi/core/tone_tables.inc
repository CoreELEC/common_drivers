/* Lookup tables for fixed-point tone arithmetic. */
const unsigned short tone_log10_lut[1][4096] = {
    {
#include "tone_log10_values.inc"
    }
};
const unsigned int tone_log10_rate_lut[1][4096] = {
    {
#include "tone_log10_rates.inc"
    }
};
const unsigned int tone_power10_lut[10][1024] = {
#include "tone_power10_values.inc"
};
