/* SPDX-License-Identifier: (GPL-2.0-only OR MIT) */
/*
 * Copyright (C) 2023 Amlogic, Inc. All rights reserved
 */

#ifndef __AML_PIO_DMA_H
#define __AML_PIO_DMA_H

// =============================================================================
// Register    : PIO SM FIFO0
// Description : PIO SM FIFO0 control register
#define PIO_SM_CFG_FIFO0_OFFSET _u(0x00000150)
#define PIO_SM_CFG_FIFO0_BITS   _u(0x00000003)
#define PIO_SM_CFG_FIFO0_RESET  _u(0x00000003)

// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO0_DMA
// Description : Enable dma mode if 1 or disable (apb mode) when 0.
#define PIO_SM_CFG_FIFO0_DMA_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO0_DMA_BITS   _u(0x00000004)
#define PIO_SM_CFG_FIFO0_DMA_MSB    _u(2)
#define PIO_SM_CFG_FIFO0_DMA_LSB    _u(2)
#define PIO_SM_CFG_FIFO0_DMA_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO0_RXEN
// Description : Enable rx-fifo request dma if 1 or disable when 0.
#define PIO_SM_CFG_FIFO0_RXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO0_RXEN_BITS   _u(0x00000002)
#define PIO_SM_CFG_FIFO0_RXEN_MSB    _u(1)
#define PIO_SM_CFG_FIFO0_RXEN_LSB    _u(1)
#define PIO_SM_CFG_FIFO0_RXEN_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO0_TXEN
// Description : Enable tx-fifo request dma if 1 or disable when 0.
//               of 0. Clock dividers are free-running, so once started, their
#define PIO_SM_CFG_FIFO0_TXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO0_TXEN_BITS   _u(0x00000001)
#define PIO_SM_CFG_FIFO0_TXEN_MSB    _u(0)
#define PIO_SM_CFG_FIFO0_TXEN_LSB    _u(0)
#define PIO_SM_CFG_FIFO0_TXEN_ACCESS "SC"

// =============================================================================
// Register    : PIO SM FIFO1
// Description : PIO SM FIFO1 control register
#define PIO_SM_CFG_FIFO1_OFFSET _u(0x00000154)
#define PIO_SM_CFG_FIFO1_BITS   _u(0x00000003)
#define PIO_SM_CFG_FIFO1_RESET  _u(0x00000003)

// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO1_DMA
// Description : Enable dma mode if 1 or disable (apb mode) when 0.
#define PIO_SM_CFG_FIFO1_DMA_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO1_DMA_BITS   _u(0x00000004)
#define PIO_SM_CFG_FIFO1_DMA_MSB    _u(2)
#define PIO_SM_CFG_FIFO1_DMA_LSB    _u(2)
#define PIO_SM_CFG_FIFO1_DMA_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO1_RXEN
// Description : Enable rx-fifo request dma if 1 or disable when 0.
#define PIO_SM_CFG_FIFO1_RXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO1_RXEN_BITS   _u(0x00000002)
#define PIO_SM_CFG_FIFO1_RXEN_MSB    _u(1)
#define PIO_SM_CFG_FIFO1_RXEN_LSB    _u(1)
#define PIO_SM_CFG_FIFO1_RXEN_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO1_TXEN
// Description : Enable tx-fifo request dma if 1 or disable when 0.
//               of 0. Clock dividers are free-running, so once started, their
#define PIO_SM_CFG_FIFO1_TXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO1_TXEN_BITS   _u(0x00000001)
#define PIO_SM_CFG_FIFO1_TXEN_MSB    _u(0)
#define PIO_SM_CFG_FIFO1_TXEN_LSB    _u(0)
#define PIO_SM_CFG_FIFO1_TXEN_ACCESS "SC"

// =============================================================================
// Register    : PIO SM FIFO2
// Description : PIO SM FIFO2 control register
#define PIO_SM_CFG_FIFO2_OFFSET _u(0x00000158)
#define PIO_SM_CFG_FIFO2_BITS   _u(0x00000003)
#define PIO_SM_CFG_FIFO2_RESET  _u(0x00000003)

// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO2_DMA
// Description : Enable dma mode if 1 or disable (apb mode) when 0.
#define PIO_SM_CFG_FIFO2_DMA_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO2_DMA_BITS   _u(0x00000004)
#define PIO_SM_CFG_FIFO2_DMA_MSB    _u(2)
#define PIO_SM_CFG_FIFO2_DMA_LSB    _u(2)
#define PIO_SM_CFG_FIFO2_DMA_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO2_RXEN
// Description : Enable rx-fifo request dma if 1 or disable when 0.
#define PIO_SM_CFG_FIFO2_RXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO2_RXEN_BITS   _u(0x00000002)
#define PIO_SM_CFG_FIFO2_RXEN_MSB    _u(1)
#define PIO_SM_CFG_FIFO2_RXEN_LSB    _u(1)
#define PIO_SM_CFG_FIFO2_RXEN_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO2_TXEN
// Description : Enable tx-fifo request dma if 1 or disable when 0.
//               of 0. Clock dividers are free-running, so once started, their
#define PIO_SM_CFG_FIFO2_TXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO2_TXEN_BITS   _u(0x00000001)
#define PIO_SM_CFG_FIFO2_TXEN_MSB    _u(0)
#define PIO_SM_CFG_FIFO2_TXEN_LSB    _u(0)
#define PIO_SM_CFG_FIFO2_TXEN_ACCESS "SC"

// =============================================================================
// Register    : PIO SM FIFO3
// Description : PIO SM FIFO3 control register
#define PIO_SM_CFG_FIFO3_OFFSET _u(0x00000158)
#define PIO_SM_CFG_FIFO3_BITS   _u(0x00000003)
#define PIO_SM_CFG_FIFO3_RESET  _u(0x00000003)

// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO3_DMA
// Description : Enable dma mode if 1 or disable (apb mode) when 0.
#define PIO_SM_CFG_FIFO3_DMA_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO3_DMA_BITS   _u(0x00000004)
#define PIO_SM_CFG_FIFO3_DMA_MSB    _u(2)
#define PIO_SM_CFG_FIFO3_DMA_LSB    _u(2)
#define PIO_SM_CFG_FIFO3_DMA_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO3_RXEN
// Description : Enable rx-fifo request dma if 1 or disable when 0.
#define PIO_SM_CFG_FIFO3_RXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO3_RXEN_BITS   _u(0x00000002)
#define PIO_SM_CFG_FIFO3_RXEN_MSB    _u(1)
#define PIO_SM_CFG_FIFO3_RXEN_LSB    _u(1)
#define PIO_SM_CFG_FIFO3_RXEN_ACCESS "SC"
// -----------------------------------------------------------------------------
// Field       : PIO_SM_CFG_FIFO3_TXEN
// Description : Enable tx-fifo request dma if 1 or disable when 0.
//               of 0. Clock dividers are free-running, so once started, their
#define PIO_SM_CFG_FIFO3_TXEN_RESET  _u(0x0)
#define PIO_SM_CFG_FIFO3_TXEN_BITS   _u(0x00000001)
#define PIO_SM_CFG_FIFO3_TXEN_MSB    _u(0)
#define PIO_SM_CFG_FIFO3_TXEN_LSB    _u(0)
#define PIO_SM_CFG_FIFO3_TXEN_ACCESS "SC"

#endif
