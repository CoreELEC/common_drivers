#ifndef DOVI_PARSER_INFO_H
#define DOVI_PARSER_INFO_H

/* Values describe the native parser ABI and algorithm versions. */
const char *dovi_md_parser_get_api_ver(void);
const char *dovi_md_parser_get_algo_ver(void);
const char *dovi_md_parser_get_build_info(void);
const char *dovi_vdr_rpu_get_api_ver(void);
const char *dovi_vdr_rpu_get_algo_ver(void);
const char *dovi_vdr_rpu_get_build_info(void);

/* Clamp signed error values >=12 to 12 before the error-table lookup. */
const char *dovi_md_parser_get_errstr(int error);

#endif
