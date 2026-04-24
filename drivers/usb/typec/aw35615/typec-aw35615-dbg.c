// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifdef MESON_AW
#include <linux/kernel.h>
#include "TypeC_Types.h"
#include "PD_Types.h"
#include "aw35615_driver.h"
#include "platform_helpers.h"
#include "typec-aw35615-dbg.h"

void aw35615_dbg_trace(void (*trace)(struct va_format *),
			const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	pr_debug("%pV\n", &vaf);
	trace(&vaf);
	va_end(args);
}

const char *aw35615_conn_state_string(ConnectionState state)
{
	switch (state) {
	case Disabled:
		return "Disabled";
	case ErrorRecovery:
		return "ErrorRecovery";
	case Unattached:
		return "Unattached";
	case AttachWaitSink:
		return "AttachWaitSink";
	case AttachedSink:
		return "AttachedSink";
	case AttachWaitSource:
		return "AttachWaitSource";
	case AttachedSource:
		return "AttachedSource";
	case TrySource:
		return "TrySource";
	case TryWaitSink:
		return "TryWaitSink";
	case TrySink:
		return "TrySink";
	case TryWaitSource:
		return "TryWaitSource";
	case AudioAccessory:
		return "AudioAccessory";
	case DebugAccessorySource:
		return "DebugAccessorySource";
	case AttachWaitAccessory:
		return "AttachWaitAccessory";
	case PoweredAccessory:
		return "PoweredAccessory";
	case UnsupportedAccessory:
		return "UnsupportedAccessory";
	case DelayUnattached:
		return "DelayUnattached";
	case UnattachedSource:
		return "UnattachedSource";
	case DebugAccessorySink:
		return "DebugAccessorySink";
	case AttachWaitDebSink:
		return "AttachWaitDebSink";
	case AttachedDebSink:
		return "AttachedDebSink";
	case AttachWaitDebSource:
		return "AttachWaitDebSource";
	case AttachedDebSource:
		return "AttachedDebSource";
	case TryDebSource:
		return "TryDebSource";
	case TryWaitDebSink:
		return "TryWaitDebSink";
	case UnattachedDebSource:
		return "UnattachedDebSource";
	case IllegalCable:
		return "IllegalCable";
	case AttachVbusOnlyok:
		return "AttachVbusOnlyok";
	default:
		return "INVALID";
	}
}

const char *aw35615_protocol_state_string(ProtocolState_t state)
{
	switch (state) {
	case PRLDisabled:
		return "PRLDisabled";
	case PRLIdle:
		return "PRLIdle";
	case PRLReset:
		return "PRLReset";
	case PRLResetWait:
		return "PRLResetWait";
	case PRLRxWait:
		return "PRLRxWait";
	case PRLTxSendingMessage:
		return "PRLTxSendingMessage";
	case PRLTxWaitForPHYResponse:
		return "PRLTxWaitForPHYResponse";
	case PRLTxVerifyGoodCRC:
		return "PRLTxVerifyGoodCRC";
	case PRLManualRetries:
		return "PRLManualRetries";
	case PRL_BIST_Rx_Reset_Counter:
		return "PRL_BIST_Rx_Reset_Counter";
	case PRL_BIST_Rx_Test_Frame:
		return "PRL_BIST_Rx_Test_Frame";
	case PRL_BIST_Rx_Error_Count:
		return "PRL_BIST_Rx_Error_Count";
	case PRL_BIST_Rx_Inform_Policy:
		return "PRL_BIST_Rx_Inform_Policy";
	default:
		return "INVALID";
	}
}

const char *aw35615_policy_state_string(PolicyState_t state)
{
	switch (state) {
	case peDisabled:
		return "peDisabled";
	//    case FIRST_PE_ST:
	//        return "FIRST_PE_ST";
	case peErrorRecovery:
		return "peErrorRecovery";
	case peSourceHardReset:
		return "peSourceHardReset";
	case peSourceSendHardReset:
		return "peSourceSendHardReset";
	case peSourceSoftReset:
		return "peSourceSoftReset";
	case peSourceSendSoftReset:
		return "peSourceSendSoftReset";
	case peSourceStartup:
		return "peSourceStartup";
	case peSourceSendCaps:
		return "peSourceSendCaps";
	case peSourceDiscovery:
		return "peSourceDiscovery";
	case peSourceDisabled:
		return "peSourceDisabled";
	case peSourceTransitionDefault:
		return "peSourceTransitionDefault";
	case peSourceNegotiateCap:
		return "peSourceNegotiateCap";
	case peSourceCapabilityResponse:
		return "peSourceCapabilityResponse";
	case peSourceWaitNewCapabilities:
		return "peSourceWaitNewCapabilities";
	case peSourceTransitionSupply:
		return "peSourceTransitionSupply";
	case peSourceReady:
		return "peSourceReady";
	case peSourceGiveSourceCaps:
		return "peSourceGiveSourceCaps";
	case peSourceGetSinkCaps:
		return "peSourceGetSinkCaps";
	case peSourceSendPing:
		return "peSourceSendPing";
	case peSourceGotoMin:
		return "peSourceGotoMin";
	case peSourceGiveSinkCaps:
		return "peSourceGiveSinkCaps";
	case peSourceGetSourceCaps:
		return "peSourceGetSourceCaps";
	case peSourceSendDRSwap:
		return "peSourceSendDRSwap";
	case peSourceEvaluateDRSwap:
		return "peSourceEvaluateDRSwap";
	case peSourceAlertReceived:
		return "peSourceAlertReceived";
	case peSinkHardReset:
		return "peSinkHardReset";
	case peSinkSendHardReset:
		return "peSinkSendHardReset";
	case peSinkSoftReset:
		return "peSinkSoftReset";
	case peSinkSendSoftReset:
		return "peSinkSendSoftReset";
	case peSinkTransitionDefault:
		return "peSinkTransitionDefault";
	case peSinkStartup:
		return "peSinkStartup";
	case peSinkDiscovery:
		return "peSinkDiscovery";
	case peSinkWaitCaps:
		return "peSinkWaitCaps";
	case peSinkEvaluateCaps:
		return "peSinkEvaluateCaps";
	case peSinkSelectCapability:
		return "peSinkSelectCapability";
	case peSinkTransitionSink:
		return "peSinkTransitionSink";
	case peSinkReady:
		return "peSinkReady";
	case peSinkGiveSinkCap:
		return "peSinkGiveSinkCap";
	case peSinkGetSourceCap:
		return "peSinkGetSourceCap";
	case peSinkGetSinkCap:
		return "peSinkGetSinkCap";
	case peSinkGiveSourceCap:
		return "peSinkGiveSourceCap";
	case peSinkSendDRSwap:
		return "peSinkSendDRSwap";
	case peSinkAlertReceived:
		return "peSinkAlertReceived";
	case peSinkEvaluateDRSwap:
		return "peSinkEvaluateDRSwap";
	case peSourceSendVCONNSwap:
		return "peSourceSendVCONNSwap";
	case peSourceEvaluateVCONNSwap:
		return "peSourceEvaluateVCONNSwap";
	case peSinkSendVCONNSwap:
		return "peSinkSendVCONNSwap";
	case peSinkEvaluateVCONNSwap:
		return "peSinkEvaluateVCONNSwap";
	case peSourceSendPRSwap:
		return "peSourceSendPRSwap";
	case peSourceEvaluatePRSwap:
		return "peSourceEvaluatePRSwap";
	case peSinkSendPRSwap:
		return "peSinkSendPRSwap";
	case peSinkEvaluatePRSwap:
		return "peSinkEvaluatePRSwap";
	case peGetCountryCodes:
		return "peGetCountryCodes";
	case peGiveCountryCodes:
		return "peGiveCountryCodes";
	case peNotSupported:
		return "peNotSupported";
	case peGetPPSStatus:
		return "peGetPPSStatus";
	case peGivePPSStatus:
		return "peGivePPSStatus";
	case peGiveCountryInfo:
		return "peGiveCountryInfo";
	case peSourceGiveSourceCapExt:
		return "peSourceGiveSourceCapExt";
	case peGiveVdm:
		return "peGiveVdm";
	case peUfpVdmGetIdentity:
		return "peUfpVdmGetIdentity";
	//    case FIRST_VDM_STATE:
	//        return "FIRST_VDM_STATE";
	case peUfpVdmSendIdentity:
		return "peUfpVdmSendIdentity";
	case peUfpVdmGetSvids:
		return "peUfpVdmGetSvids";
	case peUfpVdmSendSvids:
		return "peUfpVdmSendSvids";
	case peUfpVdmGetModes:
		return "peUfpVdmGetModes";
	case peUfpVdmSendModes:
		return "peUfpVdmSendModes";
	case peUfpVdmEvaluateModeEntry:
		return "peUfpVdmEvaluateModeEntry";
	case peUfpVdmModeEntryNak:
		return "peUfpVdmModeEntryNak";
	case peUfpVdmModeEntryAck:
		return "peUfpVdmModeEntryAck";
	case peUfpVdmModeExit:
		return "peUfpVdmModeExit";
	case peUfpVdmModeExitNak:
		return "peUfpVdmModeExitNak";
	case peUfpVdmModeExitAck:
		return "peUfpVdmModeExitAck";
	case peUfpVdmAttentionRequest:
		return "peUfpVdmAttentionRequest";
	case peDfpUfpVdmIdentityRequest:
		return "peDfpUfpVdmIdentityRequest";
	case peDfpUfpVdmIdentityAcked:
		return "peDfpUfpVdmIdentityAcked";
	case peDfpUfpVdmIdentityNaked:
		return "peDfpUfpVdmIdentityNaked";
	case peDfpCblVdmIdentityRequest:
		return "peDfpCblVdmIdentityRequest";
	case peDfpCblVdmIdentityAcked:
		return "peDfpCblVdmIdentityAcked";
	case peDfpCblVdmIdentityNaked:
		return "peDfpCblVdmIdentityNaked";
	case peDfpVdmSvidsRequest:
		return "peDfpVdmSvidsRequest";
	case peDfpVdmSvidsAcked:
		return "peDfpVdmSvidsAcked";
	case peDfpVdmSvidsNaked:
		return "peDfpVdmSvidsNaked";
	case peDfpVdmModesRequest:
		return "peDfpVdmModesRequest";
	case peDfpVdmModesAcked:
		return "peDfpVdmModesAcked";
	case peDfpVdmModesNaked:
		return "peDfpVdmModesNaked";
	case peDfpVdmModeEntryRequest:
		return "peDfpVdmModeEntryRequest";
	case peDfpVdmModeEntryAcked:
		return "peDfpVdmModeEntryAcked";
	case peDfpVdmModeEntryNaked:
		return "peDfpVdmModeEntryNaked";
	case peDfpVdmModeExitRequest:
		return "peDfpVdmModeExitRequest";
	case peDfpVdmExitModeAcked:
		return "peDfpVdmExitModeAcked";
	case peSrcVdmIdentityRequest:
		return "peSrcVdmIdentityRequest";
	case peSrcVdmIdentityAcked:
		return "peSrcVdmIdentityAcked";
	case peSrcVdmIdentityNaked:
		return "peSrcVdmIdentityNaked";
	case peDfpVdmAttentionRequest:
		return "peDfpVdmAttentionRequest";
	case peCblReady:
		return "peCblReady";
	case peCblGetIdentity:
		return "peCblGetIdentity";
	case peCblGetIdentityNak:
		return "peCblGetIdentityNak";
	case peCblSendIdentity:
		return "peCblSendIdentity";
	case peCblGetSvids:
		return "peCblGetSvids";
	case peCblGetSvidsNak:
		return "peCblGetSvidsNak";
	case peCblSendSvids:
		return "peCblSendSvids";
	case peCblGetModes:
		return "peCblGetModes";
	case peCblGetModesNak:
		return "peCblGetModesNak";
	case peCblSendModes:
		return "peCblSendModes";
	case peCblEvaluateModeEntry:
		return "peCblEvaluateModeEntry";
	case peCblModeEntryAck:
		return "peCblModeEntryAck";
	case peCblModeEntryNak:
		return "peCblModeEntryNak";
	case peCblModeExit:
		return "peCblModeExit";
	case peCblModeExitAck:
		return "peCblModeExitAck";
	case peCblModeExitNak:
		return "peCblModeExitNak";
	case peDpRequestStatus:
		return "peDpRequestStatus";
	case peDpRequestStatusAck:
		return "peDpRequestStatusAck";
	case peDpRequestStatusNak:
		return "peDpRequestStatusNak";
	case peDpRequestConfig:
		return "peDpRequestConfig";
	case peDpRequestConfigAck:
		return "peDpRequestConfigAck";
	case peDpRequestConfigNak:
		return "peDpRequestConfigNak";
	//    case LAST_VDM_STATE:
	//        return "LAST_VDM_STATE";
	case PE_BIST_Receive_Mode:
		return "PE_BIST_Receive_Mode";
	case PE_BIST_Frame_Received:
		return "PE_BIST_Frame_Received";
	case PE_BIST_Carrier_Mode_2:
		return "PE_BIST_Carrier_Mode_2";
	case PE_BIST_Test_Data:
		return "PE_BIST_Test_Data";
	//    case LAST_PE_ST:
	//        return "LAST_PE_ST";
	case dbgGetRxPacket:
		return "dbgGetRxPacket";
	case dbgSendTxPacket:
		return "dbgSendTxPacket";
	case peSendCableReset:
		return "peSendCableReset";
	case peSendGenericCommand:
		return "peSendGenericCommand";
	case peSendGenericData:
		return "peSendGenericData";
	case peGiveSourceInfo:
		return "peGiveSourceInfo";
	case peGiveRevisonMessage:
		return "peGiveRevisonMessage";
	case peGetBatteryCap:
		return "peGetBatteryCap";
	case peGetBatteryStatus:
		return "peGetBatteryStatus";
	case peGetSinkCapExt:
		return "peGetSinkCapExt";
	case peGetManufacturerInfo:
		return "peGetManufacturerInfo";
	default:
		return "INVALID";
	}
}

const char *aw35615_protocol_decode_core_event(unsigned int event)
{
	switch (event) {
	case CC1_ORIENT:
		return "CC1_ORIENT";
	case CC2_ORIENT:
		return "CC2_ORIENT";
	case CC_AUDIO_ORIENT:
		return "CC_AUDIO_ORIENT";
	case CC_NO_ORIENT:
		return "CC_NO_ORIENT";
	case CC_AUDIO_OPEN:
		return "CC_AUDIO_OPEN";
	case CC_ORIENT_ALL:
		return "CC_ORIENT_ALL";
	case PD_NEW_CONTRACT:
		return "PD_NEW_CONTRACT";
	case PD_NO_CONTRACT:
		return "PD_NO_CONTRACT";
//	case PD_CONTRACT_ALL:
//		return "PD_CONTRACT_ALL";
	case PD_STATE_CHANGED:
		return "PD_STATE_CHANGED";
	case ACC_UNSUPPORTED:
		return "ACC_UNSUPPORTED";
	case BIST_DISABLED:
		return "BIST_DISABLED";
	case BIST_ENABLED:
		return "BIST_ENABLED";
//	case BIST_ALL:
//		return "BIST_ALL";
	case ALERT_EVENT:
		return "ALERT_EVENT";
	case POWER_ROLE:
		return "POWER_ROLE";
	case DATA_ROLE:
		return "DATA_ROLE";
	case AUDIO_ACC:
		return "AUDIO_ACC";
	case CUSTOM_SRC:
		return "CUSTOM_SRC";
	case VBUS_OK:
		return "VBUS_OK";
	case VBUS_DIS:
		return "VBUS_DIS";
	case WATERPROOFING:
		return "WATERPROOFING";
	case CC1_AND_CC2:
		return "CC1_AND_CC2";
	case LPD_NOTICE:
		return "LPD_NOTICE";
	case EVENT_ALL:
		return "EVENT_ALL";
	default:
		return "INVALID";
	}
}

const char *aw35615_protocol_decode_reg_addr(unsigned char reg)
{
	switch (reg) {
	case regDeviceID:
		return "regDeviceID";
	case regSwitches0:
		return "regSwitches0";
	case regSwitches1:
		return "regSwitches1";
	case regMeasure:
		return "regMeasure";
	case regSlice:
		return "regSlice";
	case regControl0:
		return "regControl0";
	case regControl1:
		return "regControl1";
	case regControl2:
		return "regControl2";
	case regControl3:
		return "regControl3";
	case regMask:
		return "regMask";
	case regPower:
		return "regPower";
	case regReset:
		return "regReset";
	case regOCPreg:
		return "regOCPreg";
	case regMaska:
		return "regMaska";
	case regMaskb:
		return "regMaskb";
	case regControl4:
		return "regControl4";
	case regControl5:
		return "regControl5";
	case regControl7:
		return "regControl7";
	case regControl8:
		return "regControl8";
	case regControl9:
		return "regControl9";
	case regVdl:
		return "regVdl";
	case regVdh:
		return "regVdh";
	case regStatus0a:
		return "regStatus0a";
	case regStatus1a:
		return "regStatus1a";
	case regInterrupta:
		return "regInterrupta";
	case regInterruptb:
		return "regInterruptb";
	case regStatus0:
		return "regStatus0";
	case regStatus1:
		return "regStatus1";
	case regInterrupt:
		return "regInterrupt";
	case regFIFO:
		return "regFIFO";
	default:
		return "unknown";
	}
}

static int aw35615_protocol_decode_tx_token(char *str, int *str_pos, unsigned char *msg)
{
//	*str_pos += sprintf(str + *str_pos, "TT-");
	switch (*msg) {
	case TXON:
		*str_pos += sprintf(str + *str_pos, "TXON");
		break;
	case SYNC1_TOKEN:
		*str_pos += sprintf(str + *str_pos, "SOP1");
		break;
	case SYNC2_TOKEN:
		*str_pos += sprintf(str + *str_pos, "SOP2");
		break;
	case SYNC3_TOKEN:
		*str_pos += sprintf(str + *str_pos, "SOP3");
		break;
	case RESET1:
		*str_pos += sprintf(str + *str_pos, "RESET1");
		break;
	case RESET2:
		*str_pos += sprintf(str + *str_pos, "RESET2");
		break;
//		case PACKSYM:
//			*str_pos = sprintf(str, "PACKSYM");
//			break;
	case JAM_CRC:
		*str_pos += sprintf(str + *str_pos, "JAM_CRC");
		break;
	case EOP:
		*str_pos += sprintf(str + *str_pos, "EOP");
		break;
	case TXOFF:
		*str_pos += sprintf(str + *str_pos, "TXOFF");
		break;
	default:
		if (*msg & PACKSYM)
			*str_pos += sprintf(str + *str_pos, "PACKSYM-n:%x", *msg & 0x1f);
		else
			*str_pos += sprintf(str + *str_pos, "UNKNOWN");
		break;
	}

	return 1;
}

static inline const char *aw35615_decode_message_type(unsigned char mt,
	unsigned char ndo, unsigned char is_ext)
{
	switch (ndo) {
	case 0:
		switch (mt) {
		case 1:
			return "GoodCRC";
		case 2:
			return "GotoMin";
		case 3:
			return "Accept";
		case 4:
			return "Reject";
		case 5:
			return "Ping";
		case 6:
			return "PS_RDY";
		case 7:
			return "Get_Source_Cap";
		case 8:
			return "Get_Sink_Cap";
		case 9:
			return "DR_Swap";
		case 10:
			return "PR_Swap";
		case 11:
			return "VCONN_Swap";
		case 12:
			return "Wait";
		case 13:
			return "Soft_Reset";
		case 14:
			return "Data_Reset";
		case 15:
			return "Data_Reset_Complete";
		case 16:
			return "Not_Supported";
		case 17:
			return "Get_Source_Cap_Extended";
		case 18:
			return "Get_Status";
		case 19:
			return "FR_Swap";
		case 20:
			return "Get_PPS_Status";
		case 21:
			return "Get_Country_Codes";
		case 22:
			return "Get_Sink_Cap_Extended";
		case 23:
			return "Get_Source_Info";
		case 24:
			return "Get_Revision";
		default:
			return "Reserved";
		}
	default:
		switch (is_ext) {
		case 0:
			switch (mt) {
			case 1:
				return "Source_Capabilities";
			case 2:
				return "Request";
			case 3:
				return "BIST";
			case 4:
				return "Sink_Capabilities";
			case 5:
				return "Battery_Status";
			case 6:
				return "Alert";
			case 7:
				return "Get_Country_Info";
			case 8:
				return "Enter_USB";
			case 9:
				return "EPR_Request";
			case 10:
				return "EPR_Mode";
			case 11:
				return "Source_Info";
			case 12:
				return "Revision";
			case 15:
				return "Vendor_Defined";
			default:
				return "Reserved";
			}
		case 1:
			switch (mt) {
			case 1:
				return "Source_Capabilities_Extended";
			case 2:
				return "Status";
			case 3:
				return "Get_Battery_Cap";
			case 4:
				return "Get_Battery_Status";
			case 5:
				return "Battery_Capabilities";
			case 6:
				return "Get_Manufacturer_Info";
			case 7:
				return "Manufacturer_Info";
			case 8:
				return "Security_Request";
			case 9:
				return "Security_Response";
			case 10:
				return "Firmware_Update_Request";
			case 11:
				return "Firmware_Update_Response";
			case 12:
				return "PPS_Status";
			case 13:
				return "Country_Info";
			case 14:
				return "Country_Codes";
			case 15:
				return "Sink_Capabilities_Extended";
			case 16:
				return "Extended_Control";
			case 17:
				return "EPR_Source_Capabilities";
			case 18:
				return "EPR_Sink_Capabilities";
			case 30:
				return "Vendor_Defined_Extended";
			default:
				return "Reserved";
			}
		default:
			return "UNKNOWN";
		}
	}
}

static int aw35615_protocol_decode_msg_header(char *str, int *str_pos,
	unsigned char *msg, unsigned char *is_ext, unsigned char *num_dobjs)
{
	sopMainHeader_t *txheader = (sopMainHeader_t *)msg;

	*str_pos += sprintf(str + *str_pos, "HDR-%s:pdr:%x,sr:%x,ppr:%x,mid:%x,ndo:%x,e:%x",
			aw35615_decode_message_type(txheader->MessageType,
				txheader->NumDataObjects, txheader->Extended),
			txheader->PortDataRole, txheader->SpecRevision,
			txheader->PortPowerRole, txheader->MessageID, txheader->NumDataObjects,
			txheader->Extended);

	*num_dobjs = txheader->NumDataObjects;
	*is_ext = txheader->Extended;

	return 2;
}

static int aw35615_protocol_decode_ext_header(char *str, int *str_pos, unsigned char *msg)
{
	ExtHeader_t *ext_header = (ExtHeader_t *)msg;

	*str_pos += sprintf(str + *str_pos, "EXTHDR ds:%x,rc:%x,cn:%x,c-ed:%x",
					ext_header->DataSize, ext_header->ReqChunk,
					ext_header->ChunkNum, ext_header->Chunked);

	return 2;
}

static int aw35615_protocol_decode_msg_do(char *str, int *str_pos, unsigned char *msg,
	unsigned char num_dos, bool is_ext)
{
	int i = 0;
	int bytes = 0;

	if (!num_dos)
		return bytes;

	if (!is_ext) {
		*str_pos += sprintf(str + *str_pos, "||DO");
		i = num_dos * 4 - 1;
		bytes = num_dos * 4;

		while (i >= 0) {
//			unsigned char data_i;

//			*str_pos += sprintf(str + *str_pos, "|");
//			for (data_i = 0; data_i < 4; data_i++) {
//				*str_pos += sprintf(str + *str_pos, "0x");
//				*str_pos += sprintf(str + *str_pos, "%02x", msg[i++]);
//			}
//			*str_pos += sprintf(str + *str_pos, "|");
//		}
//		*str_pos += sprintf(str + *str_pos, "|");
			if (i % 4 == 3) {
				*str_pos += sprintf(str + *str_pos, "|");
				*str_pos += sprintf(str + *str_pos, "0x");
			}

			*str_pos += sprintf(str + *str_pos, "%02x", msg[i]);

			if (!(i % 4))
				*str_pos += sprintf(str + *str_pos, "|");

			i--;
		}

	} else {
		unsigned char ext_header_len;

		ext_header_len = aw35615_protocol_decode_ext_header(str, str_pos, msg);
		i = num_dos * 4 - 1;
		bytes = num_dos * 4;

		*str_pos += sprintf(str + *str_pos, "||DO:");

		/* if Chunked bit is set, ext dos are 4 bytes aligned.
		 * But the ext_header resides in the first ext_header_len bytes.
		 */
		while (i >= ext_header_len)
			*str_pos += sprintf(str + *str_pos, "%02x", msg[i--]);

		*str_pos += sprintf(str + *str_pos, "||");
	}

	return bytes;
}

const char *aw35615_protocol_decode_tx_msg(char *str, unsigned char *msg, unsigned char size)
{
	int ret = 0;
	int i = 0;
	unsigned char is_ext = 0;
	unsigned char num_dobjs = 0;

	while (i < 5)
		i += aw35615_protocol_decode_tx_token(str, &ret, msg + i);

	i += aw35615_protocol_decode_msg_header(str, &ret, msg + i, &is_ext, &num_dobjs);

	i += aw35615_protocol_decode_msg_do(str, &ret, msg + i, num_dobjs, is_ext);

	/* dos are not 4 bytes aligned? */
	WARN_ON(i > size);

	while (i < size)
		i += aw35615_protocol_decode_tx_token(str, &ret, msg + i);

	WARN_ON(ret >= AW35615_MSG_MAX);

	return str;
}

static int aw35615_protocol_decode_rx_token(char *str, int *str_pos, unsigned char *msg)
{
	unsigned char masked_sop = *msg & 0xE0;

	switch (masked_sop) {
	case SOP_CODE_SOP:
		*str_pos += sprintf(str + *str_pos, "RT_SOP");
		break;
	case SOP_CODE_SOP1:
		*str_pos += sprintf(str + *str_pos, "RT_SOP1");
		break;
	case SOP_CODE_SOP2:
		*str_pos += sprintf(str + *str_pos, "RT_SOP2");
		break;
	case SOP_CODE_SOP1_DEBUG:
		*str_pos += sprintf(str + *str_pos, "RT_SOP1_D");
		break;
	case SOP_CODE_SOP2_DEBUG:
		*str_pos += sprintf(str + *str_pos, "RT_SOP2_D");
		break;
	default:
		*str_pos += sprintf(str + *str_pos, "RT_SOP_ERR");
		break;
	}

	return 1;
}

const char *aw35615_protocol_decode_rx_msg(char *str, unsigned char *msg, unsigned char size,
	bool is_hdr, bool have_crc)
{
	int ret = 0;
	int i = 0, crc_i = 0;
	static unsigned char is_ext;
	static unsigned char num_dobjs;

	if (is_hdr) {
		i += aw35615_protocol_decode_rx_token(str, &ret, msg + i);
		i += aw35615_protocol_decode_msg_header(str, &ret, msg + i, &is_ext, &num_dobjs);
	} else if (have_crc) {
		ret += sprintf(str + ret, "crc:");
		while (crc_i++ < 4)
			ret += sprintf(str + ret, "%02x", msg[i++]);
	} else {
		i += aw35615_protocol_decode_msg_do(str, &ret, msg + i, num_dobjs, is_ext);
		is_ext = 0;
		num_dobjs = 0;

		ret += sprintf(str + ret, "crc:");
		while (crc_i++ < 4)
			ret += sprintf(str + ret, "%02x", msg[i++]);
	}

	/* dos are not 4 bytes aligned? */
	WARN_ON(i > size);

	WARN_ON(ret >= AW35615_MSG_MAX);

	return str;
}
#endif
