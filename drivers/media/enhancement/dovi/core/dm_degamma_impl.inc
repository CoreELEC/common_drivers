/* Common fixed-point calculation. The enclosing native entry binds its
 * immutable table views and narrows or preserves the resulting 64-bit value. */
unsigned int sum = (sample & 0xffffu) + (offset & 0xffffu);
unsigned int input = sum > 0xffffu ? sum >> 1 : sum;
unsigned int index = input >> 4;
unsigned int log_value, power_row, table_value;
unsigned long long result = 0;

log_value = (log_values[index] -
             ((log_rates[index] * (input & 0xfu)) >> 8)) *
            (exponent & 0xffffu);
power_row = log_value >> 26;
if (power_row < 10u) {
    table_value = power_values[power_row][(log_value >> 16) & 0x3ffu];
    if (sum > 0xffffu)
        table_value = (unsigned int)(((unsigned long long)table_value *
                                      overflow_scale) >> 18);
    result = ((unsigned long long)table_value * (gain & 0xffffu)) >>
             ((power_scales[power_row] - 16u) & 0x3fu);
}
