#ifndef DOVI_TONE_MAPS_H
#define DOVI_TONE_MAPS_H

int tone_map_s(int weight, int ratio, unsigned short mode, short *output, short count);
int saturation_map_s(int weight, int ratio, unsigned short mode, short *output, short count);
int saturation_map_i(int minimum, int maximum, int source_minimum, int gain, short *tone,
                     unsigned short mode, short *output, short count);

#endif
