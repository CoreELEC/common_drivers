/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * aml_env_encrypt.c
 *
 * Copyright (C) 2020 Amlogic, Inc. All rights reserved.
 *
 */
#ifndef __AML_ENV_ENCRYPT__
#define __AML_ENV_ENCRYPT__

#include <linux/types.h>

#define AMLOGIC_ENV_ENCRYPT_MAGIC	0xb2d05e13

#define SECURITY_ENV_ENCRYPT		0x820000d0
#define GET_SHARE_MEM_INPUT_BASE	0x82000020
#define GET_SHARE_MEM_OUTPUT_BASE	0x82000021

#define ENV_DECRYPT			0
#define ENV_ENCRYPT			1
#define ENV_DATA_FIRST			0x80000000
#define ENV_DATA_LAST			0x40000000

#define ENV_IOC_MAGIC			'A'

#define ENV_IOC_ENCRYPT			_IO(ENV_IOC_MAGIC, 0x01)

typedef struct env_ioc_args {
	unsigned int size;
	unsigned int encrypt;
	unsigned long long data;
} env_ioc_args_t;

#endif
