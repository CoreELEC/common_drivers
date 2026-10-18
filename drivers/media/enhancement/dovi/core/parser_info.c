/* Parser version and build-configuration strings exposed by the getters. */
#include "parser_info.h"
#include "error_strings.h"
#include "parser_access.h"

/* The algorithm version follows the API version at byte offset eight. */
static const char version_strings[] = "0.4.1\0\0\0" "1.1.2";
static const char build_info[] =
    "Build configuration flags:\n" "\tENABLE_FLOAT\t\t0\n" "\tNUM_RPU_BUFFERS\t\t64\n";

const char *dovi_vdr_rpu_get_api_ver(void)
{
    return version_strings;
}

const char *dovi_vdr_rpu_get_algo_ver(void)
{
    return version_strings + 8;
}

const char *dovi_vdr_rpu_get_build_info(void)
{
    return build_info;
}

const char *dovi_md_parser_get_api_ver(void)
{
    return dovi_vdr_rpu_get_api_ver();
}

const char *dovi_md_parser_get_algo_ver(void)
{
    return dovi_vdr_rpu_get_algo_ver();
}

const char *dovi_md_parser_get_build_info(void)
{
    return dovi_vdr_rpu_get_build_info();
}

const char *dovi_md_parser_get_errstr(int error)
{
    return dovi_rpu_error_code_2_str(parser_clamp_error(error));
}
