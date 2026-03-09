/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) AND BSD-3-Clause */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 *
 * Based on:
 * https://github.com/raspberrypi/pico-sdk/
 * File: src/rp2_common/hardware_pio/include/hardware/pio.h
 *
 * Original Copyright follows:
 *
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 */

#ifndef _AML_PIO_H_
#define _AML_PIO_H_
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/bits.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/dmapool.h>

#define _u(x) x ## u
#include "regs.h"
#include "regs-dma.h"
#include "pio_instructions.h"

#define bool_to_bit(x) ((u32)!!(x))
#define PIO_INSTRUCTION_COUNT _u(32)

#define NUM_PIO_STATE_MACHINES 4
#define INSTR_SIZE       32
#define SM_CONFIG_OFFSET	0x18

struct pio_sm_config {
	u32 clkdiv;
	u32 execctrl;
	u32 shiftctrl;
	u32 pinctrl;
	bool claimed;
};

enum pio_irq {
	PIO_IRQ0 = 0,
	PIO_IRQ1,
	PIO_DEBUG,
	PIO_IRQ_NUM,
};

struct pio_dma;
struct pio_hw;

struct pio_hw {
	bool has_dma;
	u8 version;//default 0 x1 version
	void __iomem *reg;
	unsigned long phys_base;
	u64 clk_rate;
	struct regmap *regmap;
	struct platform_device *pdev;
	struct clk *clk;
	/* use to lock pio data */
	struct mutex lock;
	struct pio_dma *dma_rx[NUM_PIO_STATE_MACHINES];
	struct pio_dma *dma_tx[NUM_PIO_STATE_MACHINES];
	u32 instr[PIO_INSTRUCTION_COUNT];
	u32 irq[PIO_IRQ_NUM];
	struct pio_sm_config sm[NUM_PIO_STATE_MACHINES];
	DECLARE_BITMAP(used_instr, INSTR_SIZE);
	u32 used_instruction_space;
};

struct pio_dma {
	bool enable;
	u32 *buf;
	struct pio_hw *pio;
	struct dma_chan *chan;
	dma_addr_t dma_buf;
	unsigned int dma_len;
	enum dma_transfer_direction dma_transfer_dir;
	enum dma_data_direction dma_data_dir;
	struct completion dma_complete;
	dma_async_tx_callback callback;
};

/** \brief FIFO join states
 *  \ingroup hardware_pio
 */
enum pio_fifo_join {
	//< TX FIFO length=4 is used for transmit, RX FIFO length=4 is used for receive
	PIO_FIFO_JOIN_NONE = 0,
	PIO_FIFO_JOIN_TX = 1,  //< TX FIFO length=8 is used for transmit, RX FIFO is disabled
	PIO_FIFO_JOIN_RX = 2,  //< RX FIFO length=8 is used for receive, TX FIFO is disabled
};

/** \brief MOV status types
 *  \ingroup hardware_pio
 */
enum pio_mov_status_type {
	STATUS_TX_LESSTHAN = 0,
	STATUS_RX_LESSTHAN = 1,
};

/*! \brief Set the base of the 'out' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'out' pins can overlap with the 'in', 'set' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param out_base First pin to set as output. See \ref sm_config_pins "sm_config_ pins" for more
 * detail on pin arguments
 */
inline void sm_config_set_out_pin_base(struct pio_sm_config *c, u32 out_base);

/*! \brief Set the number of 'out' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'out' pins can overlap with the 'in', 'set' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param out_count 0-32 Number of pins to set.
 */
inline void sm_config_set_out_pin_count(struct pio_sm_config *c, u32 out_count);

/*! \brief Set the 'out' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'out' pins can overlap with the 'in', 'set' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param out_base First pin to set as output. See \ref sm_config_pins "sm_config_ pins" for more
 * detail on pin arguments
 * \param out_count 0-32 Number of pins to set.
 */
inline void sm_config_set_out_pins(struct pio_sm_config *c, u32 out_base, u32 out_count);

/*! \brief Set the base of the 'set' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'set' pins can overlap with the 'in', 'out' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param set_base First pin to use as 'set'. See \ref sm_config_pins "sm_config_ pins" for more
 * detail on pin arguments
 */
inline void sm_config_set_set_pin_base(struct pio_sm_config *c, u32 set_base);

/*! \brief Set the count of 'set' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'set' pins can overlap with the 'in', 'out' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param set_count 0-5 Number of pins to set.
 */
inline void sm_config_set_set_pin_count(struct pio_sm_config *c, u32 set_count);

/*! \brief Set the 'set' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'set' pins can overlap with the 'in', 'out' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param set_base First pin to use as 'set'. See \ref sm_config_pins "sm_config_ pins" for more
 * detail on pin arguments
 * \param set_count 0-5 Number of pins to set.
 */
inline void sm_config_set_set_pins(struct pio_sm_config *c, u32 set_base, u32 set_count);

/*! \brief Set the base of the 'in' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'in' pins can overlap with the 'out', 'set' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param in_base First pin to use as input. See \ref sm_config_pins "sm_config_ pins" for more
 * detail on pin arguments
 */
inline void sm_config_set_in_pin_base(struct pio_sm_config *c, u32 in_base);

/*! \brief Set the base for the 'in' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'in' pins can overlap with the 'out', 'set' and 'sideset' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param in_base First pin to use as input. See \ref sm_config_pins "sm_config_ pins" for more
 * detail on pin arguments
 */
inline void sm_config_set_in_pins(struct pio_sm_config *c, u32 in_base);

/*! \brief Set the count of 'in' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * When reading pins using the IN pin mapping, this many (low) bits will be read, with the rest
 * taking the value zero.
 *
 * \if rp2040_specific
 * RP2040 does not have the ability to mask unused input pins, so the in_count must be 32
 * \endif
 *
 * \param c Pointer to the configuration structure to modify
 * \param in_count 1-32 The number of pins to include when reading via the IN pin mapping
 */
inline void sm_config_set_in_pin_count(struct pio_sm_config *c, u32 in_count);

/*! \brief Set the base of the 'sideset' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * 'sideset' pins can overlap with the 'in', 'out' and 'set' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param sideset_base First pin to use for 'side set'. See \ref sm_config_pins "sm_config_ pins"
 * for more detail on pin arguments
 */
inline void sm_config_set_sideset_pin_base(struct pio_sm_config *c, u32 sideset_base);

/*! \brief Set the 'sideset' pins in a state machine configuration
 *  \ingroup sm_config
 *
 * This method is identical to \ref sm_config_set_sideset_pin_base, and is provided
 * for backwards compatibility
 *
 * 'sideset' pins can overlap with the 'in', 'out' and 'set' pins
 *
 * \param c Pointer to the configuration structure to modify
 * \param sideset_base First pin to use for 'side set'. See \ref sm_config_pins "sm_config_ pins"
 * for more detail on pin arguments
 */
inline void sm_config_set_sideset_pins(struct pio_sm_config *c, u32 sideset_base);

/*! \brief Set the 'sideset' options in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param bit_count Number of bits to steal from delay field in the instruction for use of side set
 * (max 5)
 * \param optional True if the topmost side set bit is used as a flag for whether to apply side set
 * on that instruction
 * \param pindirs True if the side set affects pin directions rather than values
 */
inline void sm_config_set_sideset(struct pio_sm_config *c, u32 bit_count, bool optional,
				  bool pindirs);

/*! \brief Set the state machine clock divider (from integer and fractional parts - 16:8) in a state
 * machine configuration
 *  \ingroup sm_config
 *
 * The clock divider can slow the state machine's execution to some rate below
 * the system clock frequency, by enabling the state machine on some cycles
 * but not on others, in a regular pattern. This can be used to generate e.g.
 * a given UART baud rate. See the datasheet for further detail.
 *
 * \param c Pointer to the configuration structure to modify
 * \param div_int Integer part of the divisor
 * \param div_frac8 Fractional part in 1/256ths
 * \sa sm_config_set_clkdiv()
 */
inline void sm_config_set_clkdiv_int_frac8(struct pio_sm_config *c, u32 div_int,
						  u8 div_frac8);
// backwards compatibility
inline void sm_config_set_clkdiv_int_frac(struct pio_sm_config *c, u16 div_int,
					  u8 div_frac8);

/*! \brief Set the wrap addresses in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param wrap_target the instruction memory address to wrap to
 * \param wrap        the instruction memory address after which to set the program counter to
 *                    wrap_target
 *                    if the instruction does not itself update the program_counter
 */
inline void sm_config_set_wrap(struct pio_sm_config *c, u32 wrap_target, u32 wrap);

/*! \brief Set the 'jmp' pin in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param pin The raw GPIO pin number to use as the source for a `jmp pin` instruction. See \ref
sm_config_pins "sm_config_ pins" for more detail on pin arguments
 */
inline void sm_config_set_jmp_pin(struct pio_sm_config *c, u32 pin);

/*! \brief Setup 'in' shifting parameters in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param shift_right true to shift ISR to right, false to shift ISR to left
 * \param autopush whether autopush is enabled
 * \param push_threshold threshold in bits to shift in before auto/conditional re-pushing of the ISR
 */
inline void sm_config_set_in_shift(struct pio_sm_config *c, bool shift_right, bool autopush,
				   u32 push_threshold);

/*! \brief Setup 'out' shifting parameters in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param shift_right true to shift OSR to right, false to shift OSR to left
 * \param autopull whether autopull is enabled
 * \param pull_threshold threshold in bits to shift out before auto/conditional re-pulling of the
 *  OSR
 */
inline void sm_config_set_out_shift(struct pio_sm_config *c, bool shift_right, bool autopull,
				    u32 pull_threshold);

/*! \brief Setup the FIFO joining in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param join Specifies the join type. \see enum pio_fifo_join
 */
inline void sm_config_set_fifo_join(struct pio_sm_config *c, enum pio_fifo_join join);

/*! \brief Set special 'out' operations in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param sticky to enable 'sticky' output (i.e. re-asserting most recent OUT/SET pin values on
 * subsequent cycles)
 * \param has_enable_pin true to enable auxiliary OUT enable pin
 * \param enable_bit_index Data bit index for auxiliary OUT enable.
 */
inline void sm_config_set_out_special(struct pio_sm_config *c, bool sticky, bool has_enable_pin,
				      u32 enable_bit_index);

/*! \brief Set source for 'mov status' in a state machine configuration
 *  \ingroup sm_config
 *
 * \param c Pointer to the configuration structure to modify
 * \param status_sel the status operation selector. \see enum pio_mov_status_type
 * \param status_n parameter for the mov status operation (currently a bit count)
 */
inline void sm_config_set_mov_status(struct pio_sm_config *c, enum pio_mov_status_type status_sel,
				     u32 status_n);

/*! \brief  Get the default state machine configuration
 *  \ingroup sm_config
 *
 * Setting | Default
 * --------|--------
 * Out Pins | 32 starting at 0
 * Set Pins | 0 starting at 0
 * In Pins | 32 starting at 0
 * Side Set Pins (base) | 0
 * Side Set | disabled
 * Wrap | wrap=31, wrap_to=0
 * In Shift | shift_direction=right, autopush=false, push_threshold=32
 * Out Shift | shift_direction=right, autopull=false, pull_threshold=32
 * Jmp Pin | 0
 * Out Special | sticky=false, has_enable_pin=false, enable_pin_index=0
 * Mov Status | status_sel=STATUS_TX_LESSTHAN, n=0
 *
 * \return the default state machine configuration which can then be modified.
 */
inline struct pio_sm_config pio_get_default_sm_config(void);

/*! \brief Return the base GPIO base for the PIO instance
 *  \ingroup hardware_pio
 *
 * \if rp2040_specific
 * This method always return 0 in RP2040
 * \endif
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \return the current GPIO base for the PIO instance
 */
u32 pio_get_gpio_base(struct pio_hw *pio);

/*! \brief Apply a state machine configuration to a state machine
 *  \ingroup hardware_pio
 * * \if rp2350_specific * See \ref sm_config_pins "sm_config_ pins" for more detail on why this
 * method might fail o
 * \endif
 * \param pio Handle to PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param config the configuration to apply
 * \return PICO_OK (0) on success, negative error code otherwise
 */
inline int pio_sm_set_config(struct pio_hw *pio, u32 sm, const struct pio_sm_config *config);

/*! \brief Return the instance number of a PIO instance
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \return the PIO instance number (0, 1, ...)
 */
u32 pio_get_index(struct pio_hw *pio);

/*! \brief Return the funcsel number of a PIO instance
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \return the PIO instance number (0, 1, ...)
 * \see gpio_function
 */
u32 pio_get_funcsel(struct pio_hw *pio);

/*! \brief Convert PIO instance to hardware instance
 *  \ingroup hardware_pio
 *
 * \return the PIO hardware instance
 * currently only one
 */
struct pio_hw *pio_get_instance(u32 instance);

/*! \brief Setup the function select for a GPIO to use output from the given PIO instance
 *  \ingroup hardware_pio
 *
 * PIO appears as an alternate function in the GPIO muxing, just like an SPI
 * or UART. This function configures that multiplexing to connect a given PIO
 * instance to a GPIO. Note that this is not necessary for a state machine to
 * be able to read the *input* value from a GPIO, but only for it to set the
 * output value or output enable.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param pin the GPIO pin whose function select to set
 */
void pio_gpio_init(struct pio_hw *pio, u32 pin);

/*! \brief Return the DREQ to use for pacing transfers to/from a particular state machine FIFO
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param is_tx true for sending data to the state machine, false for receiving data from the state
 *  machine
 */
inline u32 pio_get_dreq(struct pio_hw *pio, u32 sm, bool is_tx);

struct pio_program {
	const u16 *instructions;
	u8 length;
	s8 origin; // required instruction memory origin or -1
	u8 pio_version;
};

/*! \brief Set the base GPIO base for the PIO instance
 *  \ingroup hardware_pio
 *
 * Since an individual PIO accesses only 32 pins, to be able to access more pins, the PIO
 * instance must specify a base GPIO where the instance's "pin 0" maps. For RP2350 the valid
 * values are 0 and 16, indicating the PIO instance has access to pins 0-31, or 16-47 respectively.
 *
 * NOTE: This method simply changes the underlying PIO register, it does not detect or attempt
 * to prevent any side effects this change will have on in use state machines on this PIO.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param gpio_base the GPIO base (either 0 or 16)
 * \return PICO_OK (0) on success, error code otherwise
 */
int pio_set_gpio_base(struct pio_hw *pio, u32 gpio_base);

/*! \brief Determine whether the given program can (at the time of the call) be loaded onto the PIO
 *  instance
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param program the program definition
 * \return true if the program can be loaded;
 *         false if not, e.g. if there is not suitable space in the instruction memory
 */
bool pio_can_add_program(struct pio_hw *pio, const struct pio_program *program);

/*! \brief Determine whether the given program can (at the time of the call) be loaded onto the PIO
 * instance starting at a particular location
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param program the program definition
 * \param offset the instruction memory offset wanted for the start of the program
 * \return true if the program can be loaded at that location;
 *         false if not, e.g. if there is not space in the instruction memory
 */
bool pio_can_add_program_at_offset(struct pio_hw *pio, const struct pio_program *program,
				   u32 offset);

/*! \brief Attempt to load the program
 *  \ingroup hardware_pio
 *
 * \see pio_can_add_program() if you need to check whether the program can be loaded
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param program the program definition
 * \return the instruction memory offset the program is loaded at, or negative for error (for
 * backwards compatibility with prior SDK the error value is -1 i.e. PICO_ERROR_GENERIC)
 */
int pio_add_program(struct pio_hw *pio, const struct pio_program *program);

/*! \brief Attempt to load the program at the specified instruction memory offset
 *  \ingroup hardware_pio
 *
 * \see pio_can_add_program_at_offset() if you need to check whether the program can be loaded
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param program the program definition
 * \param offset the instruction memory offset wanted for the start of the program
 * \return the instruction memory offset the program is loaded at, or negative for error (for
 * backwards compatibility with prior SDK the error value is -1 i.e. PICO_ERROR_GENERIC)
 */
int pio_add_program_at_offset(struct pio_hw *pio, const struct pio_program *program, u32 offset);

/*! \brief Remove a program from a PIO instance's instruction memory
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param program the program definition
 * \param loaded_offset the loaded offset returned when the program was added
 */
void pio_remove_program(struct pio_hw *pio, const struct pio_program *program, u32 loaded_offset);

/*! \brief Clears all of a PIO instance's instruction memory
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 */
void pio_clear_instruction_memory(struct pio_hw *pio);

/*! \brief Resets the state machine to a consistent state, and configures it
 *  \ingroup hardware_pio
 *
 * This method:
 * - Disables the state machine (if running)
 * - Clears the FIFOs
 * - Applies the configuration specified by 'config'
 * - Resets any internal state e.g. shift counters
 * - Jumps to the initial program location given by 'initial_pc'
 *
 * The state machine is left disabled on return from this call.
 *
 * \if rp2350_specific
 * See \ref sm_config_pins "sm_config_ pins" for more detail on why this method might fail on
 * RP2350B
 * \endif
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param initial_pc the initial program memory offset to run from
 * \param config the configuration to apply (or NULL to apply defaults)
 * \return PICO_OK, or < 0 for an error (see \enum pico_error_codes)
 */
int pio_sm_init(struct pio_hw *pio, u32 sm, u32 initial_pc, const struct pio_sm_config *config);

/*! \brief Enable or disable a PIO state machine
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param enabled true to enable the state machine; false to disable
 */
inline void pio_sm_set_enabled(struct pio_hw *pio, u32 sm, bool enabled);

/*! \brief Enable or disable multiple PIO state machines
 *  \ingroup hardware_pio
 *
 * Note that this method just sets the enabled state of the state machine;
 * if now enabled they continue exactly from where they left off.
 *
 * \see pio_enable_sm_mask_in_sync() if you wish to enable multiple state machines
 * and ensure their clock dividers are in sync.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param mask bit mask of state machine indexes to modify the enabled state of
 * \param enabled true to enable the state machines; false to disable
 */
inline  void pio_set_sm_mask_enabled(struct pio_hw *pio, u32 mask, bool enabled);

/*! \brief Restart a state machine with a known state
 *  \ingroup hardware_pio
 *
 * This method clears the ISR, shift counters, clock divider counter
 * pin write flags, delay counter, latched EXEC instruction, and IRQ wait condition.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 */
inline  void pio_sm_restart(struct pio_hw *pio, u32 sm);

/*! \brief Restart multiple state machine with a known state
 *  \ingroup hardware_pio
 *
 * This method clears the ISR, shift counters, clock divider counter
 * pin write flags, delay counter, latched EXEC instruction, and IRQ wait condition.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param mask bit mask of state machine indexes to modify the enabled state of
 */
inline  void pio_restart_sm_mask(struct pio_hw *pio, u32 mask);

/*! \brief Restart a state machine's clock divider from a phase of 0
 *  \ingroup hardware_pio
 *
 * Each state machine's clock divider is a free-running piece of hardware,
 * that generates a pattern of clock enable pulses for the state machine,
 * based *only* on the configured integer/fractional divisor. The pattern of
 * running/halted cycles slows the state machine's execution to some
 * controlled rate.
 *
 * This function clears the divider's integer and fractional phase
 * accumulators so that it restarts this pattern from the beginning. It is
 * called automatically by pio_sm_init() but can also be called at a later
 * time, when you enable the state machine, to ensure precisely consistent
 * timing each time you load and run a given PIO program.
 *
 * More commonly this hardware mechanism is used to synchronise the execution
 * clocks of multiple state machines -- see pio_clkdiv_restart_sm_mask().
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 */
inline void pio_sm_clkdiv_restart(struct pio_hw *pio, u32 sm);

/*! \brief Enable multiple PIO state machines synchronizing their clock dividers
 *  \ingroup hardware_pio
 *
 * This is equivalent to calling both pio_set_sm_mask_enabled() and
 * pio_clkdiv_restart_sm_mask() on the *same* clock cycle. All state machines
 * specified by 'mask' are started simultaneously and, assuming they have the
 * same clock divisors, their divided clocks will stay precisely synchronised.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param mask bit mask of state machine indexes to modify the enabled state of
 */
inline void pio_enable_sm_mask_in_sync(struct pio_hw *pio, u32 mask);

/*! \brief PIO interrupt source numbers for pio related IRQs
 * \ingroup hardware_pio
 */
enum pio_interrupt_source {
	pis_interrupt0 = PIO_INTR_SM0_LSB,                    //< PIO interrupt 0 is raised
	pis_interrupt1 = PIO_INTR_SM1_LSB,                    //< PIO interrupt 1 is raised
	pis_interrupt2 = PIO_INTR_SM2_LSB,                    //< PIO interrupt 2 is raised
	pis_interrupt3 = PIO_INTR_SM3_LSB,                    //< PIO interrupt 3 is raised
	pis_sm0_tx_fifo_not_full = PIO_INTR_SM0_TXNFULL_LSB,  //< SM 0 TX FIFO is not full
	pis_sm1_tx_fifo_not_full = PIO_INTR_SM1_TXNFULL_LSB,  //< SM 1 TX FIFO is not full
	pis_sm2_tx_fifo_not_full = PIO_INTR_SM2_TXNFULL_LSB,  //< SM 2 TX FIFO is not full
	pis_sm3_tx_fifo_not_full = PIO_INTR_SM3_TXNFULL_LSB,  //< SM 3 TX FIFO is not full
	pis_sm0_rx_fifo_not_empty = PIO_INTR_SM0_RXNEMPTY_LSB,//< SM 0 RX FIFO is not empty
	pis_sm1_rx_fifo_not_empty = PIO_INTR_SM1_RXNEMPTY_LSB,//< SM 1 RX FIFO is not empty
	pis_sm2_rx_fifo_not_empty = PIO_INTR_SM2_RXNEMPTY_LSB,//< SM 2 RX FIFO is not empty
	pis_sm3_rx_fifo_not_empty = PIO_INTR_SM3_RXNEMPTY_LSB,//< SM 3 RX FIFO is not empty
};

/*! \brief  Enable/Disable a single source on a PIO's IRQ 0
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param source the source number (see \ref pio_interrupt_source)
 * \param enabled true to enable IRQ 0 for the source, false to disable.
 */
inline void pio_set_irq0_source_enabled(struct pio_hw *pio, enum pio_interrupt_source source,
					bool enabled);

/*! \brief  Enable/Disable a single source on a PIO's IRQ 1
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param source the source number (see \ref pio_interrupt_source)
 * \param enabled true to enable IRQ 0 for the source, false to disable.
 */
inline void pio_set_irq1_source_enabled(struct pio_hw *pio, enum pio_interrupt_source source,
					bool enabled);

 /*! \brief  Enable/Disable multiple sources on a PIO's IRQ 0
  *  \ingroup hardware_pio
  *
  * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
  * \param source_mask Mask of bits, one for each source number (see \ref pio_interrupt_source) to
  *  affect
  * \param enabled true to enable all the sources specified in the mask on IRQ 0, false to disable
  * all the sources specified in the mask on IRQ 0
  */
inline void pio_set_irq0_source_mask_enabled(struct pio_hw *pio, u32 source_mask,
					     bool enabled);

/*! \brief  Enable/Disable multiple sources on a PIO's IRQ 1
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param source_mask Mask of bits, one for each source number (see \ref pio_interrupt_source) to
 *  affect
 * \param enabled true to enable all the sources specified in the mask on IRQ 1, false to disable
 *  all the source specified in the mask on IRQ 1
 */
inline void pio_set_irq1_source_mask_enabled(struct pio_hw *pio, u32 source_mask,
					     bool enabled);

/*! \brief  Enable/Disable a single source on a PIO's specified (0/1) IRQ index
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param irq_index the IRQ index; either 0 or 1
 * \param source the source number (see \ref pio_interrupt_source)
 * \param enabled true to enable the source on the specified IRQ, false to disable.
 */
inline void pio_set_irqn_source_enabled(struct pio_hw *pio, u32 irq_index,
					enum pio_interrupt_source source, bool enabled);

/*! \brief  Enable/Disable multiple sources on a PIO's specified (0/1) IRQ index
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param irq_index the IRQ index; either 0 or 1
 * \param source_mask Mask of bits, one for each source number (see \ref pio_interrupt_source) to
 *  affect
 * \param enabled true to enable all the sources specified in the mask on the specified IRQ, false
 *  to disable all the sources
 * specified in the mask on the specified IRQ
 */
inline void pio_set_irqn_source_mask_enabled(struct pio_hw *pio, u32 irq_index, u32 source_mask,
					     bool enabled);

/*! \brief  Determine if a particular PIO interrupt is set
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param pio_interrupt_num the PIO interrupt number 0-7
 * \return true if corresponding PIO interrupt is currently set
 */
inline bool pio_interrupt_get(struct pio_hw *pio, u32 pio_interrupt_num);

/*! \brief  Clear a particular PIO interrupt
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param pio_interrupt_num the PIO interrupt number 0-7
 */
inline void pio_interrupt_clear(struct pio_hw *pio, u32 pio_interrupt_num);

/*! \brief Return the current program counter for a state machine
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return the program counter
 */
inline u8 pio_sm_get_pc(struct pio_hw *pio, u32 sm);

/*! \brief Immediately execute an instruction on a state machine
 *  \ingroup hardware_pio
 *
 * This instruction is executed instead of the next instruction in the normal control flow on the
 * state machine. Subsequent calls to this method replace the previous executed instruction if it is
 * still running. \see pio_sm_is_exec_stalled() to see if an executed instruction
 * is still running (i.e. it is stalled on some condition)
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param instr the encoded PIO instruction
 */
inline void pio_sm_exec(struct pio_hw *pio, u32 sm, u32 instr);

/*! \brief Determine if an instruction set by pio_sm_exec() is stalled executing
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return true if the executed instruction is still running (stalled)
 */
inline bool pio_sm_is_exec_stalled(struct pio_hw *pio, u32 sm);

/*! \brief Immediately execute an instruction on a state machine and wait for it to complete
 *  \ingroup hardware_pio
 *
 * This instruction is executed instead of the next instruction in the normal control flow on the
 * state machine. Subsequent calls to this method replace the previous executed instruction if it
 * is still running. \see pio_sm_is_exec_stalled() to see if an executed instruction
 * is still running (i.e. it is stalled on some condition)
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param instr the encoded PIO instruction
 */
inline void pio_sm_exec_wait_blocking(struct pio_hw *pio, u32 sm, u32 instr);

/*! \brief Set the current wrap configuration for a state machine
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param wrap_target the instruction memory address to wrap to
 * \param wrap        the instruction memory address after which to set the program counter
 *                    to wrap_target
 *                    if the instruction does not itself update the program_counter
 */
void pio_sm_set_wrap(struct pio_hw *pio, u32 sm, u32 wrap_target, u32 wrap);

/*! \brief Set the current 'out' pins for a state machine
 *  \ingroup hardware_pio
 *
 * 'out' pins can overlap with the 'in', 'set' and 'sideset' pins
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param out_base First pin to set as output. See \ref pio_sm_pins "pio_sm_ pins" for more detail
 *  on pin arguments
 * \param out_count 0-32 Number of pins to set.
 */
void pio_sm_set_out_pins(struct pio_hw *pio, u32 sm, u32 out_base, u32 out_count);

/*! \brief Set the current 'set' pins for a state machine
 *  \ingroup hardware_pio
 *
 * 'set' pins can overlap with the 'in', 'out' and 'sideset' pins
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param set_base First pin to set as 'set'. See \ref pio_sm_pins "pio_sm_ pins" for more detail
 *  on pin arguments
 * \param set_count 0-5 Number of pins to set.
 */
void pio_sm_set_set_pins(struct pio_hw *pio, u32 sm, u32 set_base, u32 set_count);

/*! \brief Set the current 'in' pins for a state machine
 *  \ingroup hardware_pio
 *
 * 'in' pins can overlap with the 'out', 'set' and 'sideset' pins
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param in_base First pin to use as input. See \ref pio_sm_pins "pio_sm_ pins" for more detail on
 *  pin arguments
 */
void pio_sm_set_in_pins(struct pio_hw *pio, u32 sm, u32 in_base);

/*! \brief Set the current 'sideset' pins for a state machine
 *  \ingroup hardware_pio
 *
 * 'sideset' pins can overlap with the 'in', 'out' and 'set' pins
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param sideset_base Base pin for 'side set'. See \ref pio_sm_pins "pio_sm_ pins" for more detail
 *  on pin arguments
 */
void pio_sm_set_sideset_pins(struct pio_hw *pio, u32 sm, u32 sideset_base);

/*! \brief Set the 'jmp' pin for a state machine
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param pin The pin number to use as the source for a `jmp pin` instruction. See \ref pio_sm_pins
 *  "pio_sm_ pins" for more detail on pin arguments
 */
void pio_sm_set_jmp_pin(struct pio_hw *pio, u32 sm, u32 pin);

/*! \brief Write a word of data to a state machine's TX FIFO
 *  \ingroup hardware_pio
 *
 * This is a raw FIFO access that does not check for fullness. If the FIFO is
 * full, the FIFO contents and state are not affected by the write attempt.
 * Hardware sets the TXOVER sticky flag for this FIFO in FDEBUG, to indicate
 * that the system attempted to write to a full FIFO.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param data the 32 bit data value
 *
 * \sa pio_sm_put_blocking()
 */
inline void pio_sm_put(struct pio_hw *pio, u32 sm, u32 data);

/*! \brief Read a word of data from a state machine's RX FIFO
 *  \ingroup hardware_pio
 *
 * This is a raw FIFO access that does not check for emptiness. If the FIFO is
 * empty, the hardware ignores the attempt to read from the FIFO (the FIFO
 * remains in an empty state following the read) and the sticky RXUNDER flag
 * for this FIFO is set in FDEBUG to indicate that the system tried to read
 * from this FIFO when empty. The data returned by this function is undefined
 * when the FIFO is empty.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 *
 * \sa pio_sm_get_blocking()
 */
inline u32 pio_sm_get(struct pio_hw *pio, u32 sm);

/*! \brief Determine if a state machine's RX FIFO is full
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return true if the RX FIFO is full
 */
bool pio_sm_is_rx_fifo_full(struct pio_hw *pio, u32 sm);

/*! \brief Determine if a state machine's RX FIFO is empty
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return true if the RX FIFO is empty
 */
bool pio_sm_is_rx_fifo_empty(struct pio_hw *pio, u32 sm);

/*! \brief Return the number of elements currently in a state machine's RX FIFO
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return the number of elements in the RX FIFO
 */
u32 pio_sm_get_rx_fifo_level(struct pio_hw *pio, u32 sm);

/*! \brief Determine if a state machine's TX FIFO is full
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return true if the TX FIFO is full
 */
bool pio_sm_is_tx_fifo_full(struct pio_hw *pio, u32 sm);

/*! \brief Determine if a state machine's TX FIFO is empty
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return true if the TX FIFO is empty
 */
bool pio_sm_is_tx_fifo_empty(struct pio_hw *pio, u32 sm);

/*! \brief Return the number of elements currently in a state machine's TX FIFO
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \return the number of elements in the TX FIFO
 */
u32 pio_sm_get_tx_fifo_level(struct pio_hw *pio, u32 sm);

/*! \brief Write a word of data to a state machine's TX FIFO, blocking if the FIFO is full
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param data the 32 bit data value
 */
void pio_sm_put_blocking(struct pio_hw *pio, u32 sm, u32 data);

/*! \brief Read a word of data from a state machine's RX FIFO, blocking if the FIFO is empty
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 */
u32 pio_sm_get_blocking(struct pio_hw *pio, u32 sm);

/*! \brief Empty out a state machine's TX FIFO
 *  \ingroup hardware_pio
 *
 * This method executes `pull` instructions on the state machine until the TX
 * FIFO is empty. This disturbs the contents of the OSR, so see also
 * pio_sm_clear_fifos() which clears both FIFOs but leaves the state machine's
 * internal state undisturbed.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 *
 * \sa pio_sm_clear_fifos()
 */
void pio_sm_drain_tx_fifo(struct pio_hw *pio, u32 sm);

/*! \brief set the current clock divider for a state machine using a 16:8 fraction
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param div_int the integer part of the clock divider
 * \param div_frac8 the fractional part of the clock divider in 1/256s
 */
void pio_sm_set_clkdiv_int_frac8(struct pio_hw *pio, u32 sm, u32 div_int, u8 div_frac8);

// backwards compatibility
void pio_sm_set_clkdiv_int_frac(struct pio_hw *pio, u32 sm, u16 div_int, u8 div_frac8);

/*! \brief set the current clock divider for a state machine
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param div the floating point clock divider
 */
void pio_sm_set_clkdiv(struct pio_hw *pio, u32 sm, float div);

/*! \brief Clear a state machine's TX and RX FIFOs
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 */
void pio_sm_clear_fifos(struct pio_hw *pio, u32 sm);

/*! \brief Use a state machine to set a value on all pins for the PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set values on all 32 pins,
 * before restoring the state machine's pin configuration to what it was.
 *
 * This method is provided as a convenience to set initial pin states, and should not be used
 * against a state machine that is enabled.
 * Note: This method only works for pins < 32. To use with pins >= 32 call pio_sm_set_pins64
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pin_values the pin values to set. See \ref pio_sm_pins "pio_sm_ pins" for more detail on
 *  pin arguments
 */
void pio_sm_set_pins(struct pio_hw *pio, u32 sm, u32 pin_values);

/*! \brief Use a state machine to set a value on all pins for the PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set values on all 32 pins,
 * before restoring the state machine's pin configuration to what it was.
 *
 * This method is provided as a convenience to set initial pin states, and should not be used
 * against a state machine that is enabled.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pin_values the pin values to set. See \ref pio_sm_pins "pio_sm_ pins" for more detail on
 *  pin arguments
 */
void pio_sm_set_pins64(struct pio_hw *pio, u32 sm, u64 pin_values);

/*! \brief Use a state machine to set a value on multiple pins for the PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set values on up to 32 pins,
 * before restoring the state machine's pin configuration to what it was.
 *
 * This method is provided as a convenience to set initial pin states, and should not be used
 * against a state machine that is enabled.
 * Note: This method only works for pins < 32. To use with pins >= 32 call
 * pio_sm_set_pins_with_mask64
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pin_values the pin values to set (if the corresponding bit in pin_mask is set)
 * \param pin_mask a bit for each pin to indicate whether the corresponding pin_value for that pin
 * should be applied. See \ref pio_sm_pins "pio_sm_pins" for more detail on pin arguments
 */
void pio_sm_set_pins_with_mask(struct pio_hw *pio, u32 sm, u32 pin_values, u32 pin_mask);

/*! \brief Use a state machine to set a value on multiple pins for the PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set values on up to 32 pins, before restoring the state machine's pin
 * configuration to what it was.
 *
 * This method is provided as a convenience to set initial pin states, and should not be used
 * against a state machine that is enabled.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pin_values the pin values to set (if the corresponding bit in pin_mask is set)
 * \param pin_mask a bit for each pin to indicate whether the corresponding pin_value for that pin
 * should be applied. See \ref pio_sm_pins "pio_sm_pins" for more detail on pin arguments
 */
void pio_sm_set_pins_with_mask64(struct pio_hw *pio, u32 sm, u64 pin_values, u64 pin_mask);

/*! \brief Use a state machine to set the pin directions for multiple pins for the PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set pin directions on up to 32pins,
 * before restoring the state machine's pin configuration to what it was.
 * This method is provided as a convenience to set initial pin directions, and should not be used
 * against a state machine that is enabled.
 * Note: This method only works for pins < 32. To use with pins >= 32 call
 * pio_sm_set_pindirs_with_mask64
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pin_dirs the pin directions to set - 1 = out, 0 = in (if the corresponding
 * bit in pin_mask is set)
 * \param pin_mask a bit for each pin to indicate whether the corresponding pin_value for that pin
 * should be applied.
 */
void pio_sm_set_pindirs_with_mask(struct pio_hw *pio, u32 sm, u32 pin_dirs, u32 pin_mask);

/*! \brief Use a state machine to set the pin directions for multiple pins for the PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set pin directions on up to 32pins, before restoring the state machine's
 * pin configuration to what it was. This method is provided as a convenience to set initial pin
 * directions, and should not be used against a state machine that is enabled.
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pin_dirs the pin directions to set - 1 = out, 0 = in (if the corresponding bit in pin_mask
 * is set)
 * \param pin_mask a bit for each pin to indicate whether the corresponding pin_value for that pin
 * should be applied.
 */
void pio_sm_set_pindirs_with_mask64(struct pio_hw *pio, u32 sm, u64 pin_dirs, u64 pin_mask);

/*! \brief Use a state machine to set the same pin direction for multiple consecutive pins for the
 * PIO instance
 *  \ingroup hardware_pio
 *
 * This method repeatedly reconfigures the target state machine's pin configuration and executes
 * 'set' instructions to set the pin direction on consecutive pins, before restoring the state
 * machine's pin configuration to what it was.
 * This method is provided as a convenience to set initial pin directions, and should not be used
 * against a state machine that is enabled.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3) to use
 * \param pins_base the first pin to set a direction for. See \ref pio_sm_pins "pio_sm_ pins" for
 * more detail on pin arguments
 * \param pin_count the count of consecutive pins to set the direction for
 * \param is_out the direction to set; true = out, false = in
 * \return PICO_OK (0) on success, error code otherwise
 */
int pio_sm_set_consecutive_pindirs(struct pio_hw *pio, u32 sm, u32 pins_base, u32 pin_count,
				   bool is_out);

/*! \brief Mark a state machine as used
 *  \ingroup hardware_pio
 *
 * Method for cooperative claiming of hardware. Will cause a panic if the state machine
 * is already claimed. Use of this method by libraries detects accidental
 * configurations that would fail in unpredictable ways.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 */
void pio_sm_claim(struct pio_hw *pio, u32 sm);

/*! \brief Mark multiple state machines as used
 *  \ingroup hardware_pio
 *
 * Method for cooperative claiming of hardware. Will cause a panic if any of the state machines
 * are already claimed. Use of this method by libraries detects accidental
 * configurations that would fail in unpredictable ways.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm_mask Mask of state machine indexes
 */
void pio_claim_sm_mask(struct pio_hw *pio, u32 sm_mask);

/*! \brief Mark a state machine as no longer used
 *  \ingroup hardware_pio
 *
 * Method for cooperative claiming of hardware.
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 */
void pio_sm_unclaim(struct pio_hw *pio, u32 sm);

/*! \brief Claim a free state machine on a PIO instance
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param required if true the function will panic if none are available
 * \param dev owner of this sm
 * \return the state machine index or negative if required was false, and none were free (for
 * backwards compatibility with prior SDK the error value is -1 i.e. PICO_ERROR_GENERIC)
 */
int pio_claim_unused_sm(struct pio_hw *pio, bool required);

/*! \brief Determine if a PIO state machine is claimed
 *  \ingroup hardware_pio
 *
 * \param pio The PIO instance; e.g. \ref pio0 or \ref pio1
 * \param sm State machine index (0..3)
 * \param dev owner of using this sm
 * \return true if claimed, false otherwise
 * \see pio_sm_claim
 * \see pio_claim_sm_mask
 */
bool pio_sm_is_claimed(struct pio_hw *pio, u32 sm);

/*! \brief Finds a PIO and statemachine and adds a program into PIO memory
 *  \ingroup hardware_pio
 *
 * \param program PIO program to add
 * \param pio Returns the PIO hardware instance or NULL if no PIO is available
 * \param sm Returns the index of the PIO state machine that was claimed
 * \param offset Returns the instruction memory offset of the start of the program
 * \return true on success, false otherwise
 * \see pio_remove_program_unclaim_sm
 */
bool pio_claim_free_sm_and_add_program(const struct pio_program *program, struct pio_hw **pio,
				       u32 *sm, u32 *offset);

/*! \brief Finds a PIO and statemachine and adds a program into PIO memory
 *  \ingroup hardware_pio
 *
 * This variation of \ref pio_claim_free_sm_and_add_program is useful on RP2350 QFN80 where the
 * "GPIO Base" must be set per PIO instance to either address the 32 GPIOs (0->31) or the 32
 * GPIOS (16-47). No single PIO instance can interact with both pins 0->15 or 32->47 at the same
 * time.
 * This method takes additional information about the GPIO pins needed (via gpio_base and
 * gpio_count), and optionally will set the GPIO base (\see pio_set_gpio_base) of an unused PIO
 * instance if necessary
 * \param program PIO program to add
 * \param pio Returns the PIO hardware instance or NULL if no PIO is available
 * \param sm Returns the index of the PIO state machine that was claimed
 * \param offset Returns the instruction memory offset of the start of the program
 * \param gpio_base the lowest GPIO number required (0-47 on RP2350B, 0-31 otherwise)
 * \param gpio_count the count of GPIOs required
 * \param set_gpio_base if there is no free SM on a PIO instance with the right GPIO base, and there
 * IS an unused PIO instance, then that PIO will be reconfigured so that this method can succeed
 * \return true on success, false otherwise
 * \see pio_remove_program_unclaim_sm
 */
bool pio_claim_free_sm_and_add_program_for_gpio_range(const struct pio_program *program,
						      struct pio_hw **pio,
						      u32 *sm, u32 *offset, u32 gpio_base,
						      u32 gpio_count, bool set_gpio_base);

/*! \brief Removes a program from PIO memory and unclaims the state machine
 *  \ingroup hardware_pio
 *
 * \param program PIO program to remove from memory
 * \param pio PIO hardware instance being used
 * \param sm PIO state machine that was claimed
 * \param offset the offset of the program in PIO memory
 * \see pio_claim_free_sm_and_add_program
 */
void pio_remove_program_and_unclaim_sm(const struct pio_program *program, struct pio_hw *pio,
				       u32 sm, u32 offset);

/*! \brief Return an IRQ for a PIO hardware instance
 *  \ingroup hardware_pio
 *
 * \param pio PIO hardware instance
 * \param irqn 0 for PIOx_IRQ_0 or 1 for PIOx_IRQ_1 etc where x is the PIO number
 * \return The IRQ number to use for the PIO
 */
int pio_get_irq_num(struct pio_hw *pio, u32 irqn);

/*! \brief Return the interrupt source for a state machines TX FIFO not full interrupt
 *  \ingroup hardware_pio
 *
 * \param sm State machine index (0..3)
 * \return The interrupt source number for use in \ref pio_set_irqn_source_enabled or similar
 * functions
 */
inline enum pio_interrupt_source pio_get_tx_fifo_not_full_interrupt_source(u32 sm);

/*! \brief Return the interrupt source for a state machines RX FIFO not empty interrupt
 *  \ingroup hardware_pio
 *
 * \param sm State machine index (0..3)
 * \return The interrupt source number for use in \ref pio_set_irqn_source_enabled or similar
 * functions
 */
inline enum pio_interrupt_source pio_get_rx_fifo_not_empty_interrupt_source(u32 sm);

/* ----------------> amlogic  <---------------- */
inline int pio_class_register(void);

int pio_sm_dma_put(struct pio_hw *pio, u32 sm, u32 *data, u8 len);
int pio_sm_dma_get(struct pio_hw *pio, u32 sm, u32 *data, u8 len);
u16 convert_intr(u16 intr);
void pio_get_clkdiv_param(struct pio_hw *pio, u64 clk, u64 *div_int, u64 *frac8);
#endif
