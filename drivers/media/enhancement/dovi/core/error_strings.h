#ifndef DOVI_ERROR_STRINGS_H
#define DOVI_ERROR_STRINGS_H

/*
 * Return a stable, non-NULL pointer to the diagnostic text. Codes -22..0 select
 * the table. Codes -19..-15 and all codes outside -22..0 share the
 * "Unknown error_code" pointer. Static storage must not be written or freed.
 */
const char *dovi_rpu_error_code_2_str(int code);

#endif
