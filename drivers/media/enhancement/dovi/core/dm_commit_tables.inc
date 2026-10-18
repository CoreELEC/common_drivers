/* Constants used by DM color and metadata commits. */
static const short FilterUvColDs_m[15] = {
    11, 0, 47, 0, -262, 0, 1228, 2048,
    1228, 0, -262, 0, 47, 0, 11,
};
static const unsigned int g2lSdr256Lut[256] = {
#include "gamma_sdr_values.inc"
};
static const unsigned int g2lPulsar256Lut[256] = {
#include "gamma_pulsar_values.inc"
};
