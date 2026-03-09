// SPDX-License-Identifier: (GPL-2.0+ OR MIT) AND BSD-3-Clause
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 *
 * Based on https://github.com/raspberrypi/pico-sdk/
 * File: src/rp2_common/hardware_pio/pio.c
 *
 * Original Copyright follows:
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 */

#include <linux/amlogic/pio/meson_pio.h>

inline void sm_config_set_out_pin_base(struct pio_sm_config *c, u32 out_base)
{
	if (out_base < 32 && c)
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_OUT_BASE_BITS) |
			     ((out_base & 31) << PIO_SM0_PINCTRL_OUT_BASE_LSB);
}

inline void sm_config_set_out_pin_count(struct pio_sm_config *c, u32 out_count)
{
	if (out_count < 32 && c)
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_OUT_COUNT_BITS) |
			     (out_count << PIO_SM0_PINCTRL_OUT_COUNT_LSB);
}

inline void sm_config_set_out_pins(struct pio_sm_config *c, u32 out_base, u32 out_count)
{
	sm_config_set_out_pin_base(c, out_base);
	sm_config_set_out_pin_count(c, out_count);
}

inline void sm_config_set_set_pin_base(struct pio_sm_config *c, u32 set_base)
{
	if (set_base < 32 && c)
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_SET_BASE_BITS) |
			     ((set_base & 31) << PIO_SM0_PINCTRL_SET_BASE_LSB);
}

inline void sm_config_set_set_pin_count(struct pio_sm_config *c, u32 set_count)
{
	if (c && set_count <= 5)
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_SET_COUNT_BITS) |
			     (set_count << PIO_SM0_PINCTRL_SET_COUNT_LSB);
}

inline void sm_config_set_set_pins(struct pio_sm_config *c, u32 set_base, u32 set_count)
{
	sm_config_set_set_pin_base(c, set_base);
	sm_config_set_set_pin_count(c, set_count);
}

inline void sm_config_set_in_pin_base(struct pio_sm_config *c, u32 in_base)
{
	if (c && in_base < 32)
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_IN_BASE_BITS) |
			     ((in_base & 31) << PIO_SM0_PINCTRL_IN_BASE_LSB);
}

inline void sm_config_set_in_pins(struct pio_sm_config *c, u32 in_base)
{
	sm_config_set_in_pin_base(c, in_base);
}

inline void sm_config_set_in_pin_count(struct pio_sm_config *c, u32 in_count)
{
	((void)c);
}

inline void sm_config_set_sideset_pin_base(struct pio_sm_config *c, u32 sideset_base)
{
	if (sideset_base < 32 && c)
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_SIDESET_BASE_BITS) |
			     ((sideset_base & 31) << PIO_SM0_PINCTRL_SIDESET_BASE_LSB);
}

inline void sm_config_set_sideset_pins(struct pio_sm_config *c, u32 sideset_base)
{
	sm_config_set_sideset_pin_base(c, sideset_base);
}

inline void sm_config_set_sideset(struct pio_sm_config *c, u32 bit_count, bool optional,
				  bool pindirs)
{
	if (c && bit_count <= 5 && (!optional || bit_count >= 1)) {
		c->pinctrl = (c->pinctrl & ~PIO_SM0_PINCTRL_SIDESET_COUNT_BITS) |
			     (bit_count << PIO_SM0_PINCTRL_SIDESET_COUNT_LSB);

		c->execctrl = (c->execctrl & ~(PIO_SM0_EXECCTRL_SIDE_EN_BITS |
			       PIO_SM0_EXECCTRL_SIDE_PINDIR_BITS)) |
			      (bool_to_bit(optional) << PIO_SM0_EXECCTRL_SIDE_EN_LSB) |
			      (bool_to_bit(pindirs) << PIO_SM0_EXECCTRL_SIDE_PINDIR_LSB);
	}
}

inline void sm_config_set_clkdiv_int_frac8(struct pio_sm_config *c, u32 div_int,
					   u8 div_frac8)
{
	if ((div_int >> 16) || (div_int == 0 && div_frac8 != 0) || !c)
		return;

	c->clkdiv = (((u32)div_frac8) << PIO_SM0_CLKDIV_FRAC_LSB) |
		    (((u32)div_int) << PIO_SM0_CLKDIV_INT_LSB);
}

inline void sm_config_set_clkdiv_int_frac(struct pio_sm_config *c, u16 div_int,
					  u8 div_frac8)
{
	sm_config_set_clkdiv_int_frac8(c, div_int, div_frac8);
}

inline void sm_config_set_wrap(struct pio_sm_config *c, u32 wrap_target, u32 wrap)
{
	if (c && wrap < 32 && wrap_target < 32)
		c->execctrl = (c->execctrl & ~(PIO_SM0_EXECCTRL_WRAP_TOP_BITS |
			       PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS)) |
			       (wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) |
			       (wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB);
}

inline void sm_config_set_jmp_pin(struct pio_sm_config *c, u32 pin)
{
	if (pin < 32 && c)
		c->execctrl = (c->execctrl & ~PIO_SM0_EXECCTRL_JMP_PIN_BITS) |
			       ((pin & 31) << PIO_SM0_EXECCTRL_JMP_PIN_LSB);
}

inline void sm_config_set_in_shift(struct pio_sm_config *c, bool shift_right, bool autopush,
				   u32 push_threshold)
{
	if (push_threshold  <= 32 && c)
		c->shiftctrl = (c->shiftctrl &
				~(PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_BITS |
				PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS |
				PIO_SM0_SHIFTCTRL_PUSH_THRESH_BITS)) |
				(bool_to_bit(shift_right) << PIO_SM0_SHIFTCTRL_IN_SHIFTDIR_LSB) |
				(bool_to_bit(autopush) << PIO_SM0_SHIFTCTRL_AUTOPUSH_LSB) |
				((push_threshold & 0x1fu) << PIO_SM0_SHIFTCTRL_PUSH_THRESH_LSB);
}

inline void sm_config_set_out_shift(struct pio_sm_config *c, bool shift_right, bool autopull,
				    u32 pull_threshold)
{
	if (pull_threshold <= 32 && c)
		c->shiftctrl = (c->shiftctrl &
				~(PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_BITS |
				PIO_SM0_SHIFTCTRL_AUTOPULL_BITS |
				PIO_SM0_SHIFTCTRL_PULL_THRESH_BITS)) |
				(bool_to_bit(shift_right) << PIO_SM0_SHIFTCTRL_OUT_SHIFTDIR_LSB) |
				(bool_to_bit(autopull) << PIO_SM0_SHIFTCTRL_AUTOPULL_LSB) |
				((pull_threshold & 0x1fu) << PIO_SM0_SHIFTCTRL_PULL_THRESH_LSB);
}

inline void sm_config_set_fifo_join(struct pio_sm_config *c, enum pio_fifo_join join)
{
	if (c)
		c->shiftctrl = (c->shiftctrl & (u32)~(PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS |
			PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS)) |
			(((u32)join) << PIO_SM0_SHIFTCTRL_FJOIN_TX_LSB);
}

inline void sm_config_set_out_special(struct pio_sm_config *c, bool sticky, bool has_enable_pin,
				      u32 enable_bit_index)
{
	if (c)
		c->execctrl = (c->execctrl & (u32)~(PIO_SM0_EXECCTRL_OUT_STICKY_BITS |
				PIO_SM0_EXECCTRL_INLINE_OUT_EN_BITS |
				PIO_SM0_EXECCTRL_OUT_EN_SEL_BITS)) |
				(bool_to_bit(sticky) << PIO_SM0_EXECCTRL_OUT_STICKY_LSB) |
				(bool_to_bit(has_enable_pin) << PIO_SM0_EXECCTRL_INLINE_OUT_EN_LSB)
				| ((enable_bit_index << PIO_SM0_EXECCTRL_OUT_EN_SEL_LSB) &
				 PIO_SM0_EXECCTRL_OUT_EN_SEL_BITS);
}

inline void sm_config_set_mov_status(struct pio_sm_config *c, enum pio_mov_status_type status_sel,
				     u32 status_n)
{
	if (c)
		c->execctrl = (c->execctrl &
			       ~(PIO_SM0_EXECCTRL_STATUS_SEL_BITS | PIO_SM0_EXECCTRL_STATUS_N_BITS))
				| ((((u32)status_sel) << PIO_SM0_EXECCTRL_STATUS_SEL_LSB) &
				PIO_SM0_EXECCTRL_STATUS_SEL_BITS)
				| ((status_n << PIO_SM0_EXECCTRL_STATUS_N_LSB)
				& PIO_SM0_EXECCTRL_STATUS_N_BITS);
}

u32 pio_get_gpio_base(struct pio_hw *pio)
{
	((void)pio);
	return 0;
}

inline struct pio_sm_config pio_get_default_sm_config(void)
{
	struct pio_sm_config c = {};

	sm_config_set_clkdiv_int_frac8(&c, 1, 0);
	sm_config_set_wrap(&c, 0, 31);
	sm_config_set_in_shift(&c, true, false, 32);
	sm_config_set_out_shift(&c, true, false, 32);

	return c;
}

inline int pio_sm_set_config(struct pio_hw *pio, u32 sm, const struct pio_sm_config *config)
{
	if (!pio || !config)
		return -EINVAL;

	pio->sm[sm].clkdiv = config->clkdiv;
	pio->sm[sm].shiftctrl = config->shiftctrl;
	pio->sm[sm].execctrl = config->execctrl;
	pio->sm[sm].pinctrl = config->pinctrl;

	regmap_write(pio->regmap, PIO_SM0_CLKDIV_OFFSET + SM_CONFIG_OFFSET * sm,
		     pio->sm[sm].clkdiv);
	regmap_write(pio->regmap, PIO_SM0_SHIFTCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
		     pio->sm[sm].shiftctrl);
	regmap_write(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
		     pio->sm[sm].execctrl);
	regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
		     pio->sm[sm].pinctrl);

	return 0;
}

inline u32 pio_get_dreq(struct pio_hw *pio, u32 sm, bool is_tx)
{
	if (!pio)
		return -EINVAL;

	return 0;
}

inline void pio_sm_set_enabled(struct pio_hw *pio, u32 sm, bool enabled)
{
	if (!pio)
		return;

	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET, bool_to_bit(enabled) << sm,
			   bool_to_bit(enabled) << sm);
}

inline void pio_set_sm_mask_enabled(struct pio_hw *pio, u32 mask, bool enabled)
{
	if (!pio)
		return;

	mask = 0xf & mask;

	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET, mask, enabled ? mask : 0u);
}

static int find_offset_for_program(struct pio_hw *pio, const struct pio_program *program)
{
	u32 used_mask;
	u32 program_mask;
	int i;

	if (!pio)
		return -EINVAL;

	used_mask = pio->used_instruction_space;
	program_mask = (1u << program->length) - 1;

	if (program->length > 32)
		return -EINVAL;

	if (program->origin >= 0) {
		if (program->origin > 32 - program->length)
			return -EINVAL;
		return used_mask & (program_mask << program->origin) ? -1 : program->origin;
	}
	// work down from the top always
	for (i = 32 - program->length; i >= 0; i--) {
		if (!(used_mask & (program_mask << (u32)i)))
			return i;
	}
	return -ENOMEM;
}

static int add_program_at_offset_check(struct pio_hw *pio, const struct pio_program *program,
				       u32 offset)
{
	u32 used_mask;
	u32 program_mask;
	u64 final_mask;

	if (!pio || !program)
		return -EINVAL;

	used_mask = pio->used_instruction_space;
	program_mask = (1u << program->length) - 1;

	if (offset > INSTR_SIZE || offset + program->length > INSTR_SIZE)
		return -EINVAL;

	if (program->origin >= 0 && (u32)program->origin != offset)
		return -EINVAL;

	final_mask = (u64)program_mask << offset;
	return (used_mask & final_mask) ? -EINVAL : 0;
}

bool pio_can_add_program(struct pio_hw *pio, const struct pio_program *program)
{
	int rc;

	if (!pio || !program)
		return false;

	mutex_lock(&pio->lock);
	rc = find_offset_for_program(pio, program);
	if (rc >= 0)
		rc = add_program_at_offset_check(pio, program, (u32)rc);

	mutex_unlock(&pio->lock);
	return rc == 0;
}

bool pio_can_add_program_at_offset(struct pio_hw *pio, const struct pio_program *program,
				   u32 offset)
{
	bool rc = add_program_at_offset_check(pio, program, offset) == 0;
	return rc;
}

static int add_program_at_offset(struct pio_hw *pio, const struct pio_program *program, u32 offset)
{
	u32 program_mask = (1u << program->length) - 1;
	u16 instr;
	u32 i;
	int rc = add_program_at_offset_check(pio, program, offset);

	if (rc != 0)
		return rc;

	for (i = 0; i < program->length; ++i) {
		/* For A9 later, neet to change */
		if (pio->version >= 2)
			instr = convert_intr(program->instructions[i]);
		else
			instr = program->instructions[i];
		regmap_write(pio->regmap, PIO_INSTR_MEM0_OFFSET + 4 * (i + offset),
			     pio_instr_bits_jmp != _pio_major_instr_bits(instr) ? instr
						   : instr + offset);
	}
	pio->used_instruction_space |= program_mask << offset;
	return (int)offset;
}

int pio_add_program(struct pio_hw *pio, const struct pio_program *program)
{
	int offset = find_offset_for_program(pio, program);

	mutex_lock(&pio->lock);
	if (offset >= 0)
		offset = add_program_at_offset(pio, program, (u32)offset);
	mutex_unlock(&pio->lock);
	return offset;
}

int pio_add_program_at_offset(struct pio_hw *pio, const struct pio_program *program, u32 offset)
{
	int rc = add_program_at_offset(pio, program, offset);
	return rc;
}

void pio_remove_program(struct pio_hw *pio, const struct pio_program *program, u32 loaded_offset)
{
	u32 program_mask = (1u << program->length) - 1;

	program_mask <<= loaded_offset;

	if (program_mask == (pio->used_instruction_space & program_mask))
		pio->used_instruction_space &= ~program_mask;
	else
		pr_err("program_mask not match used_instruction_space\n");
}

void pio_clear_instruction_memory(struct pio_hw *pio)
{
	u32 i;

	mutex_lock(&pio->lock);
	pio->used_instruction_space = 0;
	for (i = 0; i < PIO_INSTRUCTION_COUNT; i++)
		regmap_write(pio->regmap, PIO_INSTR_MEM0_OFFSET + 4 * i, pio_encode_jmp(i));
	mutex_unlock(&pio->lock);
}

int pio_sm_init(struct pio_hw *pio, u32 sm, u32 initial_pc, const struct pio_sm_config *config)
{
	int rc;

	const u32 fdebug_sm_mask =
			(1u << PIO_FDEBUG_TXOVER_LSB) |
			(1u << PIO_FDEBUG_RXUNDER_LSB) |
			(1u << PIO_FDEBUG_TXSTALL_LSB) |
			(1u << PIO_FDEBUG_RXSTALL_LSB);

	if (initial_pc > PIO_INSTRUCTION_COUNT)
		return -EINVAL;

	pio_sm_set_enabled(pio, sm, false);

	if (config) {
		rc = pio_sm_set_config(pio, sm, config);
	} else {
		struct pio_sm_config c = pio_get_default_sm_config();

		rc = pio_sm_set_config(pio, sm, &c);
	}

	if (rc)
		return rc;

	pio_sm_clear_fifos(pio, sm);

	// Clear FIFO debug flags
	regmap_write(pio->regmap, PIO_FDEBUG_OFFSET, fdebug_sm_mask << sm);

	// Finally, clear some internal SM state
	pio_sm_restart(pio, sm);
	pio_sm_clkdiv_restart(pio, sm);
	pio_sm_exec(pio, sm, pio_encode_jmp(initial_pc));
	return 0;
}

inline void pio_sm_restart(struct pio_hw *pio, u32 sm)
{
	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET, PIO_CTRL_SM_RESTART_BITS,
			   1u << (PIO_CTRL_SM_RESTART_LSB + sm));
}

inline void pio_restart_sm_mask(struct pio_hw *pio, u32 mask)
{
	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET, PIO_CTRL_SM_RESTART_BITS,
			   mask << PIO_CTRL_SM_RESTART_LSB);
}

inline void pio_sm_clkdiv_restart(struct pio_hw *pio, u32 sm)
{
	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET, PIO_CTRL_CLKDIV_RESTART_BITS,
			   1u << (PIO_CTRL_CLKDIV_RESTART_LSB + sm));
}

inline void pio_clkdiv_restart_sm_mask(struct pio_hw *pio, u32 mask)
{
	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET, PIO_CTRL_CLKDIV_RESTART_BITS,
			   mask << PIO_CTRL_CLKDIV_RESTART_LSB);
}

void pio_enable_sm_mask_in_sync(struct pio_hw *pio, u32 mask)
{
	regmap_update_bits(pio->regmap, PIO_CTRL_OFFSET,
			   PIO_CTRL_CLKDIV_RESTART_BITS | PIO_CTRL_SM_ENABLE_BITS,
			   (mask << PIO_CTRL_CLKDIV_RESTART_LSB) |
			   (mask << PIO_CTRL_SM_ENABLE_LSB));
}

inline u8 pio_sm_get_pc(struct pio_hw *pio, u32 sm)
{
	u32 value;

	regmap_read(pio->regmap, PIO_SM0_ADDR_OFFSET + SM_CONFIG_OFFSET * sm, &value);

	return 0x1fu & value;
}

inline void pio_sm_exec(struct pio_hw *pio, u32 sm, u32 instr)
{
	regmap_write(pio->regmap, PIO_SM0_INSTR_OFFSET + SM_CONFIG_OFFSET * sm,
		     (pio->version >= 2) ? convert_intr(instr) : instr);
}

inline bool pio_sm_is_exec_stalled(struct pio_hw *pio, u32 sm)
{
	u32 value;

	regmap_read(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &value);
	return value & PIO_SM0_EXECCTRL_EXEC_STALLED_BITS;
}

inline void pio_sm_exec_wait_blocking(struct pio_hw *pio, u32 sm, u32 instr)
{
	/* no need this */
}

void pio_sm_set_wrap(struct pio_hw *pio, u32 sm, u32 wrap_target, u32 wrap)
{
	u32 value, mask;

	mask = PIO_SM0_EXECCTRL_WRAP_TOP_BITS | PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS;
	value = (wrap_target << PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB) |
		(wrap << PIO_SM0_EXECCTRL_WRAP_TOP_LSB);

	regmap_update_bits(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   mask, value);
}

void pio_sm_set_out_pins(struct pio_hw *pio, u32 sm, u32 out_base, u32 out_count)
{
	u32 value, mask;

	mask = PIO_SM0_PINCTRL_OUT_BASE_BITS | PIO_SM0_PINCTRL_OUT_COUNT_BITS;
	value =  (out_base << PIO_SM0_PINCTRL_OUT_BASE_LSB) |
		 (out_count << PIO_SM0_PINCTRL_OUT_COUNT_LSB);

	regmap_update_bits(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   mask, value);
}

void pio_sm_set_set_pins(struct pio_hw *pio, u32 sm, u32 set_base, u32 set_count)
{
	u32 value, mask;

	mask = PIO_SM0_PINCTRL_SET_BASE_BITS | PIO_SM0_PINCTRL_SET_COUNT_BITS;
	value = (set_base << PIO_SM0_PINCTRL_SET_BASE_LSB) |
		(set_count << PIO_SM0_PINCTRL_SET_COUNT_LSB);

	regmap_update_bits(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   mask, value);
}

void pio_sm_set_in_pins(struct pio_hw *pio, u32 sm, u32 in_base)
{
	u32 value, mask;

	mask = PIO_SM0_PINCTRL_IN_BASE_BITS;
	value = (in_base << PIO_SM0_PINCTRL_IN_BASE_LSB);

	regmap_update_bits(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   mask, value);
}

void pio_sm_set_sideset_pins(struct pio_hw *pio, u32 sm, u32 sideset_base)
{
	u32 value, mask;

	mask = PIO_SM0_PINCTRL_SIDESET_BASE_BITS;
	value = (sideset_base << PIO_SM0_PINCTRL_SIDESET_BASE_LSB);

	regmap_update_bits(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   mask, value);
}

void pio_sm_set_jmp_pin(struct pio_hw *pio, u32 sm, u32 pin)
{
	u32 value, mask;

	mask = PIO_SM0_EXECCTRL_JMP_PIN_BITS;
	value = pin << PIO_SM0_EXECCTRL_JMP_PIN_LSB;

	regmap_update_bits(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   mask, value);
}

inline void pio_sm_put(struct pio_hw *pio, u32 sm, u32 data)
{
	regmap_write(pio->regmap, PIO_TXF0_OFFSET + sm * 4, data);
}

inline u32 pio_sm_get(struct pio_hw *pio, u32 sm)
{
	u32 value;

	regmap_read(pio->regmap, PIO_RXF0_OFFSET + sm * 4, &value);
	return value;
}

bool pio_sm_is_rx_fifo_full(struct pio_hw *pio, u32 sm)
{
	u32 value;

	if (pio->version == 0) {
		pr_err("%s:This version does not support this function\n", __func__);
		return true;
	}

	regmap_read(pio->regmap, PIO_FSTAT_OFFSET, &value);

	return (value & (1u << (PIO_FSTAT_RXFULL_LSB + sm))) != 0;
}

bool pio_sm_is_rx_fifo_empty(struct pio_hw *pio, u32 sm)
{
	u32 value;

	if (pio->version == 0) {
		regmap_read(pio->regmap, PIO_INTR_OFFSET, &value);
		return !!(value & (1u << (0 + sm)));
	}
	regmap_read(pio->regmap, PIO_FSTAT_OFFSET, &value);
	return (value & (1u << (PIO_FSTAT_RXEMPTY_LSB + sm))) != 0;
}

u32 pio_sm_get_rx_fifo_level(struct pio_hw *pio, u32 sm)
{
	u32 value;
	u32 mask = PIO_FLEVEL_RX0_BITS >> PIO_FLEVEL_RX0_LSB;
	u32 bitoffs = PIO_FLEVEL_RX0_LSB + sm * (PIO_FLEVEL_RX1_LSB - PIO_FLEVEL_RX0_LSB);

	if (pio->version == 0) {
		pr_err("%s:This version does not support this function\n", __func__);
		return true;
	}

	regmap_read(pio->regmap, PIO_FLEVEL_OFFSET, &value);

	return (value >> bitoffs) & mask;
}

bool pio_sm_is_tx_fifo_full(struct pio_hw *pio, u32 sm)
{
	u32 value;

	if (pio->version == 0) {
		regmap_read(pio->regmap, PIO_INTR_OFFSET, &value);
		return !!(value & (1u << (4 + sm)));
	}

	regmap_read(pio->regmap, PIO_FSTAT_OFFSET, &value);
	return (value & (1u << (PIO_FSTAT_TXFULL_LSB + sm))) != 0;
}

bool pio_sm_is_tx_fifo_empty(struct pio_hw *pio, u32 sm)
{
	u32 value;

	if (pio->version == 0) {
		pr_err("%s:This version does not support this function\n", __func__);
		return true;
	}
	regmap_read(pio->regmap, PIO_FSTAT_OFFSET, &value);

	return (value & (1u << (PIO_FSTAT_TXEMPTY_LSB + sm))) != 0;
}

u32 pio_sm_get_tx_fifo_level(struct pio_hw *pio, u32 sm)
{
	u32 value;
	u32 bitoffs = PIO_FLEVEL_TX0_LSB + sm * (PIO_FLEVEL_TX1_LSB - PIO_FLEVEL_TX0_LSB);
	u32 mask = PIO_FLEVEL_TX0_BITS >> PIO_FLEVEL_TX0_LSB;

	if (pio->version == 0) {
		pr_err("%s:This version does not support this function\n", __func__);
		return true;
	}

	regmap_read(pio->regmap, PIO_FLEVEL_OFFSET, &value);

	return (value >> bitoffs) & mask;
}

void pio_sm_put_blocking(struct pio_hw *pio, u32 sm, u32 data)
{
	while (pio_sm_is_tx_fifo_full(pio, sm))
		usleep_range(1, 2);
	pio_sm_put(pio, sm, data);
}

u32 pio_sm_get_blocking(struct pio_hw *pio, u32 sm)
{
	while (pio_sm_is_rx_fifo_empty(pio, sm))
		usleep_range(1, 2);
	return pio_sm_get(pio, sm);
}

void pio_sm_drain_tx_fifo(struct pio_hw *pio, u32 sm)
{
	u32 instr, value, i;

	regmap_read(pio->regmap, PIO_SM0_SHIFTCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &value);

	instr = (value & PIO_SM0_SHIFTCTRL_AUTOPULL_BITS) ?
		pio_encode_out(pio_null, 32) : pio_encode_pull(false, false);

	if (pio->version == 0) {
		for (i = 0; i < 4; i++)
			pio_sm_exec(pio, sm, instr);
		return;
	}

	while (!pio_sm_is_tx_fifo_empty(pio, sm))
		pio_sm_exec(pio, sm, instr);
}

void pio_sm_set_clkdiv_int_frac8(struct pio_hw *pio, u32 sm, u32 div_int, u8 div_frac8)
{
	u32 mask, value;

	mask = PIO_SM0_CLKDIV_FRAC_BITS | PIO_SM0_CLKDIV_INT_BITS;
	value =  (((u32)div_frac8) << PIO_SM0_CLKDIV_FRAC_LSB) |
		 (((u32)div_int) << PIO_SM0_CLKDIV_INT_LSB);

	regmap_update_bits(pio->regmap, PIO_SM0_CLKDIV_OFFSET + SM_CONFIG_OFFSET * sm, mask, value);
}

void pio_sm_set_clkdiv_int_frac(struct pio_hw *pio, u32 sm, u16 div_int, u8 div_frac8)
{
	pio_sm_set_clkdiv_int_frac8(pio, sm, div_int, div_frac8);
}

void pio_sm_clear_fifos(struct pio_hw *pio, u32 sm)
{
	u32 value;

	/* flip fifojoint rx/tx bit and write back */
	regmap_read(pio->regmap, PIO_SM0_SHIFTCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &value);
	value ^= (PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS | PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS);
	regmap_write(pio->regmap, PIO_SM0_SHIFTCTRL_OFFSET + SM_CONFIG_OFFSET * sm, value);

	regmap_read(pio->regmap, PIO_SM0_SHIFTCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &value);
	value ^= (PIO_SM0_SHIFTCTRL_FJOIN_RX_BITS | PIO_SM0_SHIFTCTRL_FJOIN_TX_BITS);
	regmap_write(pio->regmap, PIO_SM0_SHIFTCTRL_OFFSET + SM_CONFIG_OFFSET * sm, value);
}

void pio_sm_set_pins_internal(struct pio_hw *pio, u32 sm, u32 pins)
{
	u32 pinctrl_saved;
	u32 execctrl_saved;
	u32 remaining = 32;
	u32 base = 0;
	u32 decrement;

	regmap_read(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &pinctrl_saved);
	regmap_read(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &execctrl_saved);

	regmap_update_bits(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   1u << PIO_SM0_EXECCTRL_OUT_STICKY_LSB, 0);

	while (remaining) {
		decrement = remaining > 5 ? 5 : remaining;

		regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			     (decrement << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
			     (base << PIO_SM0_PINCTRL_SET_BASE_LSB));

		pio_sm_exec(pio, sm, pio_encode_set(pio_pins, pins & 0x1fu));
		remaining -= decrement;
		base += decrement;
		pins >>= 5;
	}

	regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm, pinctrl_saved);
	regmap_write(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, execctrl_saved);
}

void pio_sm_set_pins(struct pio_hw *pio, u32 sm, u32 pin_values)
{
	pio_sm_set_pins_internal(pio, sm, pin_values);
}

static void pio_sm_set_pins_with_mask_internal(struct pio_hw *pio, u32 sm, u32 pin_values,
					       u32 pin_mask, bool pindirs)
{
	u32 pinctrl_saved;
	u32 execctrl_saved;

	regmap_read(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &pinctrl_saved);
	regmap_read(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &execctrl_saved);

	regmap_update_bits(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   1u << PIO_SM0_EXECCTRL_OUT_STICKY_LSB, 0);

	while (pin_mask) {
		/* return first non-0 bit offset */
		u32 base = (u32)__builtin_ctz(pin_mask);

		regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			     (1u << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
			     (base << PIO_SM0_PINCTRL_SET_BASE_LSB));

		if (pindirs)
			pio_sm_exec(pio, sm,
				    pio_encode_set(pio_pindirs, (pin_values >> base) & 0x1u));
		else
			pio_sm_exec(pio, sm,
				    pio_encode_set(pio_pins, (pin_values >> base) & 0x1u));

		pin_mask &= pin_mask - 1;
	}

	regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm, pinctrl_saved);
	regmap_write(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, execctrl_saved);
}

void pio_sm_set_pins_with_mask(struct pio_hw *pio, u32 sm, u32 pin_values, u32 pin_mask)
{
	pio_sm_set_pins_with_mask_internal(pio, sm, pin_values, pin_mask, false);
}

void pio_sm_set_pindirs_with_mask(struct pio_hw *pio, u32 sm, u32 pin_dirs, u32 pin_mask)
{
	pio_sm_set_pins_with_mask_internal(pio, sm, pin_dirs, pin_mask, true);
}

int pio_sm_set_consecutive_pindirs(struct pio_hw *pio, u32 sm, u32 pins_base, u32 pin_count,
				   bool is_out)
{
	u32 pinctrl_saved;
	u32 execctrl_saved;
	u32 pindir_val = is_out ? 0x1f : 0;

	regmap_read(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &pinctrl_saved);
	regmap_read(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, &execctrl_saved);

	regmap_update_bits(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			   1u << PIO_SM0_EXECCTRL_OUT_STICKY_LSB, 0);

	while (pin_count > 5) {
		regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
			     (5u << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
			     (pins_base << PIO_SM0_PINCTRL_SET_BASE_LSB));

		pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, pindir_val));
		pin_count -= 5;
		pins_base = (pins_base + 5) & 0x1f;
	}

	regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm,
		     (pin_count << PIO_SM0_PINCTRL_SET_COUNT_LSB) |
		     (pins_base << PIO_SM0_PINCTRL_SET_BASE_LSB));

	pio_sm_exec(pio, sm, pio_encode_set(pio_pindirs, pindir_val));

	regmap_write(pio->regmap, PIO_SM0_PINCTRL_OFFSET + SM_CONFIG_OFFSET * sm, pinctrl_saved);
	regmap_write(pio->regmap, PIO_SM0_EXECCTRL_OFFSET + SM_CONFIG_OFFSET * sm, execctrl_saved);

	return 0;
}

void pio_sm_claim(struct pio_hw *pio, u32 sm)
{
	mutex_lock(&pio->lock);
	if (!pio->sm[sm].claimed)
		pio->sm[sm].claimed = true;
	else
		pr_err("state machine [%d] is already claimed\n", sm);
	mutex_unlock(&pio->lock);
}

void pio_claim_sm_mask(struct pio_hw *pio, u32 sm_mask)
{
	u32 i;

	mutex_lock(&pio->lock);
	for (i = 0; sm_mask; i++, sm_mask >>= 1u) {
		if (sm_mask & 1u)
			pio_sm_claim(pio, i);
	}
	mutex_unlock(&pio->lock);
}

void pio_sm_unclaim(struct pio_hw *pio, u32 sm)
{
	mutex_lock(&pio->lock);
	if (pio->sm[sm].claimed)
		pio->sm[sm].claimed = false;
	mutex_unlock(&pio->lock);
}

int pio_claim_unused_sm(struct pio_hw *pio, bool required)
{
	u32 i;

	mutex_lock(&pio->lock);
	for (i = 0; i < NUM_PIO_STATE_MACHINES; i++) {
		if (!pio->sm[i].claimed) {
			pio->sm[i].claimed = true;
			mutex_unlock(&pio->lock);
			return i;
		}
	}
	mutex_unlock(&pio->lock);
	if (required)
		pr_err("No PIO state machines are available\n");
	return -ENOMEM;
}

bool pio_sm_is_claimed(struct pio_hw *pio, u32 sm)
{
	return (!pio->sm[sm].claimed);
}

void pio_remove_program_and_unclaim_sm(const struct pio_program *program, struct pio_hw *pio,
				       u32 sm, u32 offset)
{
	pio_remove_program(pio, program, offset);
	pio_sm_unclaim(pio, sm);
}

inline bool pio_interrupt_get(struct pio_hw *pio, u32 pio_interrupt_num)
{
	u32 value;

	regmap_read(pio->regmap, PIO_IRQ_OFFSET, &value);
	return value & (1u << pio_interrupt_num);
}

inline void pio_interrupt_clear(struct pio_hw *pio, u32 pio_interrupt_num)
{
	regmap_update_bits(pio->regmap, PIO_IRQ_OFFSET, (1u << pio_interrupt_num),
			   (1u << pio_interrupt_num));
}

inline void pio_set_irq0_source_enabled(struct pio_hw *pio, enum pio_interrupt_source source,
					bool enabled)
{
	if (enabled)
		regmap_update_bits(pio->regmap, PIO_IRQ0_INTE_OFFSET, (1u << source),
				   (1u << source));
	else
		regmap_update_bits(pio->regmap, PIO_IRQ0_INTE_OFFSET, (1u << source),
				   (0 << source));
}

inline void pio_set_irq1_source_enabled(struct pio_hw *pio, enum pio_interrupt_source source,
					bool enabled)
{
	if (enabled)
		regmap_update_bits(pio->regmap, PIO_IRQ1_INTE_OFFSET, (1u << source),
				   (1u << source));
	else
		regmap_update_bits(pio->regmap, PIO_IRQ1_INTE_OFFSET, (1u << source),
				   (0 << source));
}

inline void pio_set_irq0_source_mask_enabled(struct pio_hw *pio, u32 source_mask, bool enabled)
{
	if (enabled)
		regmap_update_bits(pio->regmap, PIO_IRQ0_INTE_OFFSET, source_mask, source_mask);
	else
		regmap_update_bits(pio->regmap, PIO_IRQ0_INTE_OFFSET, source_mask, 0);
}

inline void pio_set_irq1_source_mask_enabled(struct pio_hw *pio, u32 source_mask, bool enabled)
{
	if (enabled)
		regmap_update_bits(pio->regmap, PIO_IRQ1_INTE_OFFSET, source_mask, source_mask);
	else
		regmap_update_bits(pio->regmap, PIO_IRQ1_INTE_OFFSET, source_mask, 0);
}

inline void pio_set_irqn_source_enabled(struct pio_hw *pio, u32 irq_index,
					enum pio_interrupt_source source, bool enabled)
{
	if (!irq_index)
		pio_set_irq0_source_enabled(pio, source, enabled);
	else
		pio_set_irq1_source_enabled(pio, source, enabled);
}

bool pio_claim_free_sm_and_add_program_for_gpio_range(const struct pio_program *program,
						      struct pio_hw **pio,
						      u32 *sm, u32 *offset, u32 gpio_base,
						      u32 gpio_count, bool set_gpio_base)
{
	int sm_index;
	int rc;

	*pio = pio_get_instance(0);
	sm_index = (int8_t)pio_claim_unused_sm(*pio, false);
	if (sm_index < 0)
		return false;

	rc = find_offset_for_program(*pio, program);
	if (rc >= 0)
		rc = add_program_at_offset(*pio, program, (u32)rc);
	if (rc >= 0) {
		*sm = (u32)sm_index;
		*offset = (u32)rc;
	}

	if (rc >= 0)
		return true;

	/* revert claim back */
	pio_sm_unclaim(*pio, sm_index);

	*pio = NULL;
	return false;
}

bool pio_claim_free_sm_and_add_program(const struct pio_program *program, struct pio_hw **pio,
				       u32 *sm, u32 *offset)
{
	return pio_claim_free_sm_and_add_program_for_gpio_range(program, pio, sm, offset, 0, 0,
								false);
}

/* return vir irq number */
int pio_get_irq_num(struct pio_hw *pio, u32 irqn)
{
	return pio->irq[irqn];
}

inline enum pio_interrupt_source pio_get_tx_fifo_not_full_interrupt_source(u32 sm)
{
	return ((enum pio_interrupt_source)(pis_sm0_tx_fifo_not_full + sm));
}

inline enum pio_interrupt_source pio_get_rx_fifo_not_empty_interrupt_source(u32 sm)
{
	return ((enum pio_interrupt_source)(pis_sm0_rx_fifo_not_empty + sm));
}

static void pio_dma_common_callback(void *arg)
{
	struct pio_dma *dma = (struct pio_dma *)arg;
	struct pio_hw *pio = dma->pio;
	struct device *dev = &pio->pdev->dev;

	dma_unmap_single(dev, dma->dma_buf, dma->dma_len, dma->dma_data_dir);
	complete(&dma->dma_complete);
	pr_info("transfer done !!! dir: %s\n", (dma->dma_data_dir == DMA_TO_DEVICE) ? "tx" : "rx");
}

static int pio_sm_dma_xfer(struct pio_dma *dma, struct device *dev, u32 *data, u8 len)
{
	struct dma_async_tx_descriptor *txdesc;
	int ret = 0;

	dma->dma_len = sizeof(u32) * len;

	dma->dma_buf = dma_map_single(dev, data, dma->dma_len, dma->dma_data_dir);
	if (dma_mapping_error(dev, dma->dma_buf)) {
		dev_err(dev, "DMA mapping failed\n");
		return -EINVAL;
	}

	txdesc = dmaengine_prep_slave_single(dma->chan, dma->dma_buf, dma->dma_len,
					     dma->dma_transfer_dir,
					     DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!txdesc) {
		dev_err(dev, "Not able to get desc for DMA xfer\n");
		ret = -EINVAL;
		goto err_desc;
	}

	reinit_completion(&dma->dma_complete);

	txdesc->callback = pio_dma_common_callback;
	txdesc->callback_param = dma;
	ret = dma_submit_error(dmaengine_submit(txdesc));
	if (ret < 0) {
		dev_err(dev, "DMA submit failed\n");
		goto err_submit;
	}

	dma_async_issue_pending(dma->chan);
	return 0;

err_submit:
	dmaengine_terminate_sync(dma->chan);

err_desc:
	dma_unmap_single(dev, dma->dma_buf, dma->dma_len, dma->dma_data_dir);

	return ret;
}

int pio_sm_dma_put(struct pio_hw *pio, u32 sm, u32 *data, u8 len)
{
	struct device *dev = &pio->pdev->dev;
	struct pio_dma *dma = pio->dma_tx[sm];

	if (!pio->has_dma || !dma->enable) {
		pr_err("pio do not support dma or dma channel not enabled\n");
		return -EINVAL;
	}

	return pio_sm_dma_xfer(dma, dev, data, len);
}

int pio_sm_dma_get(struct pio_hw *pio, u32 sm, u32 *data, u8 len)
{
	struct device *dev = &pio->pdev->dev;
	struct pio_dma *dma = pio->dma_rx[sm];

	if (!pio->has_dma || !dma->enable) {
		pr_err("pio do not support dma or dma channel not enabled\n");
		return -EINVAL;
	}

	return pio_sm_dma_xfer(dma, dev, data, len);
}

u16 convert_intr(u16 intr)
{
	u8 opc = (GENMASK(15, 13) & intr) >> 13;
	u8 gray_opc = opc ^ (opc >> 1);

	return (gray_opc << 13) | (~GENMASK(15, 13) & intr);
}

void pio_get_clkdiv_param(struct pio_hw *pio, u64 clk, u64 *div_int, u64 *frac8)
{
	u64 clk_rate;
	u64 reminder;
	u64 precision = 1000000UL;

	clk_rate = pio->clk_rate;
	*div_int = clk_rate;
	*frac8 = precision;

	reminder = do_div(*div_int, clk);
	reminder *= precision;
	do_div(reminder, clk);
	do_div(*frac8, reminder);
}
