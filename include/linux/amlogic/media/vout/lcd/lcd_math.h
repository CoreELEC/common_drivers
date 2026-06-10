/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LCD_MATH_H
#define __LCD_MATH_H

#include <linux/of.h>
#include <linux/types.h>
#include <linux/gcd.h>

static inline unsigned char __p_to_u8(void *p)
{
	return p ? (((__u8 *)(p))[0]) : 0;
}

/* unsafe, must ensure the length of memory */
static inline unsigned short __p_to_u16(void *p)
{
	return p ? (((__u8 *)(p))[0] | (((__u8 *)(p))[1] << 8)) : 0;
}

/* unsafe, must ensure the length of memory */
static inline unsigned int __p_to_u32(void *p)
{
	return p ? (((__u8 *)(p))[0] | (((__u8 *)(p))[1] << 8) |
		(((__u8 *)(p))[2] << 16) | (((__u8 *)(p))[3] << 24)) : 0;
}

static inline int lcd_s32_constraint(int v, int min, int max)
{
	return v > max ? max : v < min ? min : v;
}

static inline __s64 lcd_s64_constraint(__s64 v, __s64 min, __s64 max)
{
	return v > max ? max : v < min ? min : v;
}

static inline unsigned int lcd_u32_diff(unsigned int a, unsigned int b)
{
	return (a >= b) ? (a - b) : (b - a);
}

static inline unsigned long long lcd_u64_diff(unsigned long long a, unsigned long long b)
{
	return (a >= b) ? (a - b) : (b - a);
}

static inline unsigned long long div_around(unsigned long long num, unsigned int den)
{
	unsigned long long ret = num + den / 2;

	if (den == 0)
		return 0;
	if (den == 1)
		return num;

	do_div(ret, den);

	return ret;
}

static inline unsigned long long lcd_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long ret = num;

	if (den == 0)
		return 0;

	do_div(ret, den);

	return ret;
}

static inline long long lcd_s64_div(s64 num, s32 den)
{
	u64 _num, _den, res;
	s32 pn = 1;
	s64 ret = 0;

	if (den == 0)
		return 0;
	pn = ((num > 0) && (den > 0)) || ((num < 0) && (den < 0)) ? 1 : -1;

	_num = num >= 0 ? num : -num;
	_den = den > 0 ? den : -den;
	res = lcd_do_div(_num, _den);
	ret = (s64)res * pn;

	return ret;
}

struct s32_slide_filter_s {
	int rst;
	int len;
	int cnt;
	int full;
	long long sum;
	long long avg;
	int *data;
};

struct s32_slide_filter_s *alloc_slide_filter(int data_cnt);
int init_slide_filter(struct s32_slide_filter_s *ft, int data_cnt);
int slide_filter(struct s32_slide_filter_s *ft, int data);
void reset_slide_filter(struct s32_slide_filter_s *ft);

unsigned int cal_crc32(unsigned int crc, const unsigned char *buf, int buf_len);

#endif
