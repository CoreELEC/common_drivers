/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __TYPEC_AW35615_DBG_H
#define __TYPEC_AW35615_DBG_H

#define AW35615_MSG_MAX	500

void aw35615_dbg_trace(void (*trace)(struct va_format *),
			const char *fmt, ...);
const char *aw35615_conn_state_string(ConnectionState state);
const char *aw35615_protocol_state_string(ProtocolState_t state);
const char *aw35615_policy_state_string(PolicyState_t state);
const char *aw35615_protocol_decode_core_event(unsigned int event);
const char *aw35615_protocol_decode_reg_addr(unsigned char reg);
const char *aw35615_protocol_decode_tx_msg(char *str, unsigned char *msg,
	unsigned char size);
const char *aw35615_protocol_decode_rx_msg(char *str, unsigned char *msg,
	unsigned char size, bool is_hdr, bool have_crc);
#endif /* __TYPEC_AW35615_DBG_H */
