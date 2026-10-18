/* Parser version and build-configuration strings exposed by the getters. */
#include "parser_info.h"
#include "error_strings.h"
#include "parser_access.h"

static const char api_version[] = "0.4.3\0";
static const char algorithm_version[] = "1.1.2\0";
static const char build_info[] =
    "Build configuration flags:\n" "\tENABLE_FLOAT\t\t0\n" "\tNUM_RPU_BUFFERS\t\t64\n";

const char *dovi_vdr_rpu_get_api_ver(void)
{
    return api_version;
}

const char *dovi_vdr_rpu_get_algo_ver(void)
{
    return algorithm_version;
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
