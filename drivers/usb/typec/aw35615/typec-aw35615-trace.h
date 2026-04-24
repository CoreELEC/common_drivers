/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifdef MESON_AW
#undef TRACE_SYSTEM
#define TRACE_SYSTEM typec-aw35615

/*
 * The TRACE_SYSTEM_VAR defaults to TRACE_SYSTEM, but must be a
 * legitimate C variable. It is not exported to user space.
 */
#undef TRACE_SYSTEM_VAR
#define TRACE_SYSTEM_VAR typec_aw35615

#if !defined(__TYPEC_AW35615_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __TYPEC_AW35615_TRACE_H

#include <linux/tracepoint.h>
#include "Port.h"
#include "modules/dpm.h"
#include "TypeC_Types.h"
#include "PD_Types.h"
#include "typec-aw35615-dbg.h"

DECLARE_EVENT_CLASS(aw35615_log_msg,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf),
	TP_STRUCT__entry(__vstring(msg, vaf->fmt, vaf->va)),
	TP_fast_assign(
		__assign_vstr(msg, vaf->fmt, vaf->va);
	),
	TP_printk("%s", __get_str(msg))
);

DEFINE_EVENT(aw35615_log_msg, aw35615_queue_sm,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_pe_state_change,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_tc_state_change,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_protocol_state_change,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_irq,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_cc_termination,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_vbus,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DEFINE_EVENT(aw35615_log_msg, aw35615_dp_platform_update,
	TP_PROTO(struct va_format *vaf),
	TP_ARGS(vaf)
);

DECLARE_EVENT_CLASS(aw35615_debounced_cc_state,
	TP_PROTO(Port_t *port),
	TP_ARGS(port),
	TP_STRUCT__entry(
		__field(CCOrientation, ccpin)
		__field(CCTermType, cctermpddebounce)
		__field(CCTermType, cctermccdebounce)
	),
	TP_fast_assign(
		__entry->ccpin = port->CCPin;
		__entry->cctermpddebounce = port->CCTermPDDebounce;
		__entry->cctermccdebounce = port->CCTermCCDebounce;
	),
	TP_printk("ccpin:%d pd-de:%d cc-de:%d",
		__entry->ccpin,
		__entry->cctermpddebounce,
		__entry->cctermccdebounce)
);

DEFINE_EVENT(aw35615_debounced_cc_state, aw35615_cc_debounced_state,
	TP_PROTO(Port_t *port),
	TP_ARGS(port)
);

DEFINE_EVENT(aw35615_debounced_cc_state, aw35615_cc_state,
	TP_PROTO(Port_t *port),
	TP_ARGS(port)
);

DECLARE_EVENT_CLASS(aw35615_sm_state,
	TP_PROTO(Port_t *port),
	TP_ARGS(port),
	TP_STRUCT__entry(
		__field(bool, smenabled)
		__field(bool, tcidle)
		__field(bool, peidle)
		__field(ConnectionState, connstate)
		__field(unsigned char, typecsubstate)
		__field(bool, usbpdactive)
		__field(ProtocolState_t, protocolstate)
		__field(PolicyState_t, policystate)
		__field(unsigned char, policysubindex)
	),
	TP_fast_assign(
		__entry->smenabled = port->SMEnabled;
		__entry->tcidle = port->TCIdle;
		__entry->peidle = port->PEIdle;
		__entry->connstate = port->ConnState;
		__entry->typecsubstate = port->TypeCSubState;
		__entry->usbpdactive = port->USBPDActive;
		__entry->protocolstate = port->ProtocolState;
		__entry->policystate = port->PolicyState;
		__entry->policysubindex = port->PolicySubIndex;
	),
	TP_printk("sm state e:%d tc_i:%d pe_i:%d conn_s:%s-%u pd_act:%d proto_s:%s poli_s:%s-%u",
		__entry->smenabled,
		__entry->tcidle,
		__entry->peidle,
		aw35615_conn_state_string(__entry->connstate),
		__entry->typecsubstate,
		__entry->usbpdactive,
		aw35615_protocol_state_string(__entry->protocolstate),
		aw35615_policy_state_string(__entry->policystate),
		__entry->policysubindex)
);

DEFINE_EVENT(aw35615_sm_state, aw35615_sm_loop_head,
	 TP_PROTO(Port_t *port),
	 TP_ARGS(port)
);

DEFINE_EVENT(aw35615_sm_state, aw35615_sm_loop_tail,
	 TP_PROTO(Port_t *port),
	 TP_ARGS(port)
);

DECLARE_EVENT_CLASS(aw35615_timer_state,
	TP_PROTO(struct TimerObj *obj),
	TP_ARGS(obj),
	TP_STRUCT__entry(
		__field(const char *, name)
		__field(unsigned long long, starttime)
		__field(unsigned int, period)
		__field(unsigned char, disablecount)
	),
	TP_fast_assign(
		__entry->name = obj->name;
		__entry->starttime = obj->starttime_;
		__entry->period = obj->period_;
		__entry->disablecount = obj->disablecount_;
	),
	TP_printk("timer %s s:%llu p:%u d:%u",
		__entry->name,
		__entry->starttime,
		__entry->period,
		__entry->disablecount)
);

DEFINE_EVENT(aw35615_timer_state, aw35615_timer_start,
	 TP_PROTO(struct TimerObj *obj),
	 TP_ARGS(obj)
);

DEFINE_EVENT(aw35615_timer_state, aw35615_timer_restart,
	 TP_PROTO(struct TimerObj *obj),
	 TP_ARGS(obj)
);

DEFINE_EVENT(aw35615_timer_state, aw35615_timer_disable,
	 TP_PROTO(struct TimerObj *obj),
	 TP_ARGS(obj)
);

DECLARE_EVENT_CLASS(aw35615_timer_state_ret,
	TP_PROTO(struct TimerObj *obj, bool ret),
	TP_ARGS(obj, ret),
	TP_STRUCT__entry(
		__field(const char *, name)
		__field(unsigned long long, starttime)
		__field(unsigned int, period)
		__field(unsigned char, disablecount)
		__field(bool, ret)
	),
	TP_fast_assign(
		__entry->name = obj->name;
		__entry->starttime = obj->starttime_;
		__entry->period = obj->period_;
		__entry->disablecount = obj->disablecount_;
		__entry->ret = ret;
	),
	TP_printk("timer %s s:%llu p:%u d:%u ret:%d",
		__entry->name,
		__entry->starttime,
		__entry->period,
		__entry->disablecount,
		__entry->ret)
);

DEFINE_EVENT(aw35615_timer_state_ret, aw35615_timer_disabled,
	 TP_PROTO(struct TimerObj *obj, bool ret),
	 TP_ARGS(obj, ret)
);

DEFINE_EVENT(aw35615_timer_state_ret, aw35615_timer_expired,
	 TP_PROTO(struct TimerObj *obj, bool ret),
	 TP_ARGS(obj, ret)
);

DECLARE_EVENT_CLASS(aw35615_proto_state,
	TP_PROTO(Port_t *port, unsigned char status),
	TP_ARGS(port, status),
	TP_STRUCT__entry(
		__field(PDTxStatus_t, pdtxstatus)
		__field(unsigned char, status)
		__field(bool, protocol_msg_rx)
		__field(bool, protocol_msg_rx_pending)
	),
	TP_fast_assign(
		__entry->pdtxstatus = port->PDTxStatus;
		__entry->status = status;
		__entry->protocol_msg_rx = port->ProtocolMsgRx;
		__entry->protocol_msg_rx_pending = port->ProtocolMsgRxPending;
	),
	TP_printk("tx_s:%d stat:%d, rx:%d rx_p:%d",
		__entry->pdtxstatus,
		__entry->status,
		__entry->protocol_msg_rx,
		__entry->protocol_msg_rx_pending
	)
);

DEFINE_EVENT(aw35615_proto_state, aw35615_protocol_requested_tx_state,
	 TP_PROTO(Port_t *port, unsigned char status),
	 TP_ARGS(port, status)
);

DEFINE_EVENT(aw35615_proto_state, aw35615_protocol_processed_tx_state,
	 TP_PROTO(Port_t *port, unsigned char status),
	 TP_ARGS(port, status)
);

DEFINE_EVENT(aw35615_proto_state, aw35615_protocol_received_rx_state,
	 TP_PROTO(Port_t *port, unsigned char status),
	 TP_ARGS(port, status)
);

DEFINE_EVENT(aw35615_proto_state, aw35615_protocol_processed_rx_state,
	 TP_PROTO(Port_t *port, unsigned char status),
	 TP_ARGS(port, status)
);

DEFINE_EVENT(aw35615_proto_state, aw35615_protocol_state_reset,
	 TP_PROTO(Port_t *port, unsigned char status),
	 TP_ARGS(port, status)
);

DECLARE_EVENT_CLASS(aw35615_proto_tx,
	TP_PROTO(Port_t *port),
	TP_ARGS(port),
	TP_STRUCT__entry(
		__array(unsigned char, protocoltxbuffer, AW_PROTOCOL_BUFFER_SIZE)
		__field(unsigned char, protocoltxbytes)
	),
	TP_fast_assign(
		memcpy(__entry->protocoltxbuffer, port->ProtocolTxBuffer,
			sizeof(__entry->protocoltxbuffer));
		__entry->protocoltxbytes = port->ProtocolTxBytes;
	),
	TP_printk("tx:%s",
		aw35615_protocol_decode_tx_msg(__get_buf(AW35615_MSG_MAX),
			__entry->protocoltxbuffer, __entry->protocoltxbytes))
);

DEFINE_EVENT(aw35615_proto_tx, aw35615_protocol_transmit_message,
	 TP_PROTO(Port_t *port),
	 TP_ARGS(port)
);

DEFINE_EVENT(aw35615_proto_tx, aw35615_protocol_send_goodcrc,
	 TP_PROTO(Port_t *port),
	 TP_ARGS(port)
);

DEFINE_EVENT(aw35615_proto_tx, aw35615_protocol_send_cablereset,
	 TP_PROTO(Port_t *port),
	 TP_ARGS(port)
);

DECLARE_EVENT_CLASS(aw35615_proto_rx,
	TP_PROTO(unsigned char *msg, unsigned int msg_l, bool is_hdr, bool have_crc),
	TP_ARGS(msg, msg_l, is_hdr, have_crc),
	TP_STRUCT__entry(
		__array(unsigned char, protocolrxbuffer, AW_PROTOCOL_BUFFER_SIZE)
		__field(unsigned char, protocolrxbytes)
		__field(bool, is_hdr)
		__field(bool, have_crc)
	),
	TP_fast_assign(
		memcpy(__entry->protocolrxbuffer, msg, msg_l);
		__entry->protocolrxbytes = msg_l;
		__entry->is_hdr = is_hdr;
		__entry->have_crc = have_crc;
	),
	TP_printk("rx:%s",
		aw35615_protocol_decode_rx_msg(__get_buf(AW35615_MSG_MAX),
			__entry->protocolrxbuffer, __entry->protocolrxbytes,
			__entry->is_hdr, __entry->have_crc))
);

DEFINE_EVENT(aw35615_proto_rx, aw35615_protocol_getrxpacket,
	 TP_PROTO(unsigned char *msg, unsigned int msg_l, bool is_hdr, bool have_crc),
	 TP_ARGS(msg, msg_l, is_hdr, have_crc)
);

DECLARE_EVENT_CLASS(aw35615_i2c_xfer,
	TP_PROTO(unsigned char reg_addr, unsigned char length, unsigned char *buf),
	TP_ARGS(reg_addr, length, buf),
	TP_STRUCT__entry(
		__field(unsigned char, reg_addr)
		__field(unsigned char, length)
		__dynamic_array(unsigned char, buf, length)
	),
	TP_fast_assign(
		__entry->reg_addr = reg_addr;
		__entry->length = length;
		memcpy(__get_dynamic_array(buf), buf, length);
	),
	TP_printk("%s, val %s.", aw35615_protocol_decode_reg_addr(__entry->reg_addr),
		__print_dynamic_array(buf, sizeof(unsigned char)))
);

DEFINE_EVENT(aw35615_i2c_xfer, aw35615_device_read,
	 TP_PROTO(unsigned char reg_addr, unsigned char length, unsigned char *buf),
	 TP_ARGS(reg_addr, length, buf)
);

DEFINE_EVENT(aw35615_i2c_xfer, aw35615_device_write,
	 TP_PROTO(unsigned char reg_addr, unsigned char length, unsigned char *buf),
	 TP_ARGS(reg_addr, length, buf)
);

TRACE_EVENT(aw35615_timer_remaining,
	TP_PROTO(struct TimerObj *obj, unsigned int remaining),
	TP_ARGS(obj, remaining),
	TP_STRUCT__entry(
		__field(const char *, name)
		__field(unsigned long long, starttime)
		__field(unsigned int, period)
		__field(unsigned char, disablecount)
		__field(unsigned int, remaining)
	),
	TP_fast_assign(
		   __entry->name = obj->name;
		   __entry->starttime = obj->starttime_;
		   __entry->period = obj->period_;
		   __entry->disablecount = obj->disablecount_;
		   __entry->remaining = remaining;
	),
	TP_printk("timer %s s:%llu p:%u d:%u remmain:%u",
		__entry->name,
		__entry->starttime,
		__entry->period,
		__entry->disablecount,
		__entry->remaining)
);

TRACE_EVENT(aw35615_core_event,
	TP_PROTO(unsigned int event, unsigned char port_i),
	TP_ARGS(event, port_i),
	TP_STRUCT__entry(
		__field(unsigned int, event)
		__field(unsigned char, port_i)
	),
	TP_fast_assign(
		   __entry->event = event;
		   __entry->port_i = port_i;
	),
	TP_printk("port %d event %s",
		__entry->port_i,
		aw35615_protocol_decode_core_event(__entry->event))
);

#endif /* __TYPEC_AW35615_TRACE_H */

/* this part must be outside header guard */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE typec-aw35615-trace

#include <trace/define_trace.h>
#endif
