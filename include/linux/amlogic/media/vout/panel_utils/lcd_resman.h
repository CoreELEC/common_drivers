/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_LCD_RESMAN_H
#define _AML_LCD_RESMAN_H
#include <linux/amlogic/media/vout/panel_utils/json_parse.h>

/*reserved memory related========================================================================*/
void lrm_resource_device_prepare(char *name);
void lrm_resource_device_finish(char *name);

int lrm_get_by_name(char *name, u64 *pa, u32 *size);
int lrm_tee_protect_by_name(char *name, u32 type, s32 sec);
void *lrm_phys_to_virt(phys_addr_t paddr, u32 size);

phys_addr_t lrm_phys_alloc(u32 size, char *desc);
phys_addr_t lrm_phys_alloc_tail(u32 size, char *desc);
phys_addr_t lrm_phys_alloc_align(u32 size, u32 align, char *desc);
phys_addr_t lrm_phys_alloc_tail_align(u32 size, u32 align, char *desc);
void lrm_phys_free(phys_addr_t paddr);
void lrm_virt_free(void *vaddr);
void lrm_free(void *va, phys_addr_t pa);
void *lrm_alloc(u32 size, phys_addr_t *paddr, char *desc);
void *lrm_alloc_tail(u32 size, phys_addr_t *paddr, char *desc);
void *lrm_alloc_align(u32 size, phys_addr_t *paddr, u32 align, char *desc);
void *lrm_alloc_tail_align(u32 size, phys_addr_t *paddr, u32 align, char *desc);
void lrm_show(void);
void lrm_bootargs_dump(void);
unsigned char *lrm_bootargs_get_data(const char *name, u32 *len);
int lcd_reserved_memory_init(struct platform_device *pdev);
bool lrm_no_map(void);
int lrm_exist(void);

unsigned char *lcd_transmit_mem_get(char *name, u32 *size);
void lcd_transmit_mem_release(char *name);

u8 *lcd_vmap(ulong addr, u32 size);
void lcd_unmap_phyaddr(u8 *vaddr);

#endif

