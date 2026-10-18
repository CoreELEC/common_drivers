/* Format one decimal value in the range 0..255. */
static void message_decimal(char *message, const char *format, u32 value)
{
    u32 pos = 0;
    while (*format != 0) {
        if (format[0] == '%' && format[1] == 'd') {
            if (value >= 100u)
                message[pos++] = (char)('0' + value / 100u);
            if (value >= 10u)
                message[pos++] = (char)('0' + (value / 10u) % 10u);
            message[pos++] = (char)('0' + value % 10u);
            format += 2;
        } else {
            message[pos++] = *format++;
        }
    }
    message[pos] = 0;
}
