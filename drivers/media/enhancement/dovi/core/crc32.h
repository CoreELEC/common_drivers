#ifndef DOVI_RPU_CRC32_H
#define DOVI_RPU_CRC32_H

/*
 * Complement initial_crc, then consume length bytes, most-significant bit first.
 * There is no final complement. A zero length returns ~initial_crc and permits
 * data == NULL. To continue a preceding result, pass its complement.
 * Nonempty input requires length stable readable bytes.
 */
unsigned int dovi_rpu_crc32(unsigned int initial_crc, const unsigned char *data,
                            unsigned long long length);

#endif
