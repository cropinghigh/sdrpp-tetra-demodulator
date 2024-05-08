/*! \file gsmtap.h
 * gsmtap header, pseudo-header in front of the actua GSM payload.
 * GSMTAP is a generic header format for GSM protocol captures,
 * it uses the IANA-assigned UDP port number 4729 and carries
 * payload in various formats of GSM interfaces such as Um MAC
 * blocks or Um bursts.
 *
 * Example programs generating GSMTAP data are airprobe
 * (http://airprobe.org/) or OsmocomBB (http://bb.osmocom.org/)
 */

#pragma once

#include <stdint.h>

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

/* The GSMTAP format definition is maintained in libosmocore,
 * specifically the latest version can always be obtained from
 * http://cgit.osmocom.org/cgit/libosmocore/tree/include/osmocom/core/gsmtap.h
 *
 * If you want to introduce new protocol/burst/channel types or extend
 * GSMTAP in any way, please contact the GSMTAP maintainer at either the
 * public openbsc@lists.osmocom.org mailing list, or privately at
 * Harald Welte <laforge@gnumonks.org>.
 *
 * Your cooperation ensures that all projects will use the same GSMTAP
 * definitions and remain compatible with each other.
 */

#define GSMTAP_VERSION		0x02

#define GSMTAP_TYPE_UM		0x01
#define GSMTAP_TYPE_ABIS	0x02
#define GSMTAP_TYPE_UM_BURST	0x03	/* raw burst bits */
#define GSMTAP_TYPE_SIM		0x04	/* ISO 7816 smart card interface */
#define GSMTAP_TYPE_TETRA_I1		0x05	/* tetra air interface */
#define GSMTAP_TYPE_TETRA_I1_BURST	0x06	/* tetra air interface */
#define GSMTAP_TYPE_WMX_BURST	0x07    /* WiMAX burst */
#define GSMTAP_TYPE_GB_LLC	0x08 /* GPRS Gb interface: LLC */
#define GSMTAP_TYPE_GB_SNDCP	0x09 /* GPRS Gb interface: SNDCP */
#define GSMTAP_TYPE_GMR1_UM	0x0a	/* GMR-1 L2 packets */
#define GSMTAP_TYPE_UMTS_RLC_MAC	0x0b
#define GSMTAP_TYPE_UMTS_RRC	0x0c
#define GSMTAP_TYPE_LTE_RRC	0x0d	/* LTE interface */
#define GSMTAP_TYPE_LTE_MAC	0x0e	/* LTE MAC interface */ 
#define GSMTAP_TYPE_LTE_MAC_FRAMED	0x0f	/* LTE MAC with context hdr */
#define GSMTAP_TYPE_OSMOCORE_LOG	0x10	/* libosmocore logging */
#define GSMTAP_TYPE_QC_DIAG		0x11	/* Qualcomm DIAG frame */
#define GSMTAP_TYPE_LTE_NAS		0x12	/* LTE Non-Access Stratum */
#define GSMTAP_TYPE_E1T1		0x13	/* E1/T1 Lines */
#define GSMTAP_TYPE_GSM_RLP		0x14	/* GSM RLP frames as per 3GPP TS 24.022 */

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

/* sub-types for TYPE_UM_BURST */
#define GSMTAP_BURST_UNKNOWN		0x00
#define GSMTAP_BURST_FCCH		0x01
#define GSMTAP_BURST_PARTIAL_SCH	0x02
#define GSMTAP_BURST_SCH		0x03
#define GSMTAP_BURST_CTS_SCH		0x04
#define GSMTAP_BURST_COMPACT_SCH	0x05
#define GSMTAP_BURST_NORMAL		0x06
#define GSMTAP_BURST_DUMMY		0x07
#define GSMTAP_BURST_ACCESS		0x08
#define GSMTAP_BURST_NONE		0x09
/* WiMAX bursts */
#define GSMTAP_BURST_CDMA_CODE		0x10	/* WiMAX CDMA Code Attribute burst */
#define GSMTAP_BURST_FCH		0x11	/* WiMAX FCH burst */
#define GSMTAP_BURST_FFB		0x12	/* WiMAX Fast Feedback burst */
#define GSMTAP_BURST_PDU		0x13	/* WiMAX PDU burst */
#define GSMTAP_BURST_HACK		0x14	/* WiMAX HARQ ACK burst */
#define GSMTAP_BURST_PHY_ATTRIBUTES	0x15	/* WiMAX PHY Attributes burst */

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

/* sub-types for TYPE_UM */
#define GSMTAP_CHANNEL_UNKNOWN	0x00
#define GSMTAP_CHANNEL_BCCH	0x01
#define GSMTAP_CHANNEL_CCCH	0x02
#define GSMTAP_CHANNEL_RACH	0x03
#define GSMTAP_CHANNEL_AGCH	0x04
#define GSMTAP_CHANNEL_PCH	0x05
#define GSMTAP_CHANNEL_SDCCH	0x06
#define GSMTAP_CHANNEL_SDCCH4	0x07
#define GSMTAP_CHANNEL_SDCCH8	0x08
#define GSMTAP_CHANNEL_FACCH_F	0x09	/* Actually, it's FACCH/F (signaling) */
#define GSMTAP_CHANNEL_FACCH_H	0x0a	/* Actually, it's FACCH/H (signaling) */
#define GSMTAP_CHANNEL_PACCH	0x0b
#define GSMTAP_CHANNEL_CBCH52	0x0c
#define GSMTAP_CHANNEL_PDTCH	0x0d
/* for legacy reasons we use a mis-spelled name. PDCH is really the physical channel, but we use it as PDTCH */
#define GSMTAP_CHANNEL_PDCH	GSMTAP_CHANNEL_PDTCH
#define GSMTAP_CHANNEL_PTCCH	0x0e
#define GSMTAP_CHANNEL_CBCH51	0x0f
#define GSMTAP_CHANNEL_VOICE_F	0x10	/* voice codec payload (FR/EFR/AMR) */
#define GSMTAP_CHANNEL_VOICE_H	0x11	/* voice codec payload (HR/AMR) */
#define GSMTAP_CHANNEL_TCH_F	GSMTAP_CHANNEL_FACCH_F	/* We used the wrong naming in 2008 when we were young */
#define GSMTAP_CHANNEL_TCH_H	GSMTAP_CHANNEL_FACCH_H	/* We used the wrong naming in 2008 when we were young */

/* GPRS Coding Scheme CS1..4 */
#define GSMTAP_GPRS_CS_BASE	0x20
#define GSMTAP_GPRS_CS(N)	(GSMTAP_GPRS_CS_BASE + N)
/* (E) GPRS Coding Scheme MCS0..9 */
#define GSMTAP_GPRS_MCS_BASE	0x30
#define GSMTAP_GPRS_MCS(N)	(GSMTAP_GPRS_MCS_BASE + N)

#define GSMTAP_CHANNEL_ACCH	0x80

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

/* sub-types for GSMTAP_TYPE_SIM */
#define GSMTAP_SIM_APDU		0x00 /* APDU data (complete APDU) */
#define GSMTAP_SIM_ATR		0x01 /* card ATR data */
#define GSMTAP_SIM_PPS_REQ		0x02 /* PPS request data */
#define GSMTAP_SIM_PPS_RSP		0x03 /* PPS response data */
#define GSMTAP_SIM_TPDU_HDR		0x04 /* TPDU command header */
#define GSMTAP_SIM_TPDU_CMD		0x05 /* TPDU command body */
#define GSMTAP_SIM_TPDU_RSP		0x06 /* TPDU response body */
#define GSMTAP_SIM_TPDU_SW		0x07 /* TPDU response trailer */

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

/* sub-types for TYPE_TETRA_AIR */
#define GSMTAP_TETRA_BSCH	0x01
#define GSMTAP_TETRA_AACH	0x02
#define GSMTAP_TETRA_SCH_HU	0x03
#define GSMTAP_TETRA_SCH_HD	0x04
#define GSMTAP_TETRA_SCH_F	0x05
#define GSMTAP_TETRA_BNCH	0x06
#define GSMTAP_TETRA_STCH	0x07
#define GSMTAP_TETRA_TCH_F	0x08
#define GSMTAP_TETRA_DMO_SCH_S	0x09
#define GSMTAP_TETRA_DMO_SCH_H	0x0a
#define GSMTAP_TETRA_DMO_SCH_F	0x0b
#define GSMTAP_TETRA_DMO_STCH	0x0c
#define GSMTAP_TETRA_DMO_TCH	0x0d

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

/* sub-types for TYPE_GMR1_UM */
#define GSMTAP_GMR1_UNKNOWN	0x00
#define GSMTAP_GMR1_BCCH	0x01
#define GSMTAP_GMR1_CCCH	0x02	/* either AGCH or PCH */
#define GSMTAP_GMR1_PCH		0x03
#define GSMTAP_GMR1_AGCH	0x04
#define GSMTAP_GMR1_BACH	0x05
#define GSMTAP_GMR1_RACH	0x06
#define GSMTAP_GMR1_CBCH	0x07
#define GSMTAP_GMR1_SDCCH	0x08
#define GSMTAP_GMR1_TACCH	0x09
#define GSMTAP_GMR1_GBCH	0x0a

#define GSMTAP_GMR1_SACCH	0x01	/* to be combined with _TCH{6,9}   */
#define GSMTAP_GMR1_FACCH	0x02	/* to be combines with _TCH{3,6,9} */
#define GSMTAP_GMR1_DKAB	0x03	/* to be combined with _TCH3 */
#define GSMTAP_GMR1_TCH3	0x10
#define GSMTAP_GMR1_TCH6	0x14
#define GSMTAP_GMR1_TCH9	0x18

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */

#define GSMTAP_UMTS_CH_PCCH	0x01
#define GSMTAP_UMTS_CH_CCCH	0x02
#define GSMTAP_UMTS_CH_DCCH	0x03

/* sub-types for TYPE_LTE_RRC */
#define GSMTAP_LTE_CH_BCCH	0x01
#define GSMTAP_LTE_CH_CCCH	0x02
#define GSMTAP_LTE_CH_DCCH	0x03
#define GSMTAP_LTE_CH_MCCH	0x04
#define GSMTAP_LTE_CH_PCCH	0x05
#define GSMTAP_LTE_CH_DTCH	0x06
#define GSMTAP_LTE_CH_MTCH	0x07

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */
/* sub-types for TYPE_E1T1 */
#define GSMTAP_E1T1_LAPD	0x01	/* Q.921 LAPD */
#define GSMTAP_E1T1_FR		0x02	/* Frame Relay */
#define GSMTAP_E1T1_RAW		0x03	/* raw/transparent B-channel */
#define GSMTAP_E1T1_TRAU16	0x04	/* 16k TRAU frames; sub-slot 0-3 */
#define GSMTAP_E1T1_TRAU8	0x05	/* 8k TRAU frames; sub-slot 0-7 */
#define GSMTAP_E1T1_V5EF	0x06	/* V5 Envelope Function */
#define GSMTAP_E1T1_X75		0x07	/* X.75 B-channel data */
#define GSMTAP_E1T1_V120	0x08	/* V.120 B-channel data */
#define GSMTAP_E1T1_V110	0x09	/* V.110 B-channel data */
#define GSMTAP_E1T1_H221	0x0a	/* H.221 B-channel data */
#define GSMTAP_E1T1_PPP		0x0b	/* PPP */

/* flags for the ARFCN */
#define GSMTAP_ARFCN_F_PCS	0x8000
#define GSMTAP_ARFCN_F_UPLINK	0x4000
#define GSMTAP_ARFCN_MASK	0x3fff

/* IANA-assigned well-known UDP port for GSMTAP messages */
#define GSMTAP_UDP_PORT			4729

/* UMTS RRC message types */
enum {
	GSMTAP_RRC_SUB_DL_DCCH_Message = 0,
	GSMTAP_RRC_SUB_UL_DCCH_Message,
	GSMTAP_RRC_SUB_DL_CCCH_Message,
	GSMTAP_RRC_SUB_UL_CCCH_Message,
	GSMTAP_RRC_SUB_PCCH_Message,
	GSMTAP_RRC_SUB_DL_SHCCH_Message,
	GSMTAP_RRC_SUB_UL_SHCCH_Message,
	GSMTAP_RRC_SUB_BCCH_FACH_Message,
	GSMTAP_RRC_SUB_BCCH_BCH_Message,
	GSMTAP_RRC_SUB_MCCH_Message,
	GSMTAP_RRC_SUB_MSCH_Message,
	GSMTAP_RRC_SUB_HandoverToUTRANCommand,
	GSMTAP_RRC_SUB_InterRATHandoverInfo,
	GSMTAP_RRC_SUB_SystemInformation_BCH,
	GSMTAP_RRC_SUB_System_Information_Container,
	GSMTAP_RRC_SUB_UE_RadioAccessCapabilityInfo,
	GSMTAP_RRC_SUB_MasterInformationBlock,
	GSMTAP_RRC_SUB_SysInfoType1,
	GSMTAP_RRC_SUB_SysInfoType2,
	GSMTAP_RRC_SUB_SysInfoType3,
	GSMTAP_RRC_SUB_SysInfoType4,
	GSMTAP_RRC_SUB_SysInfoType5,
	GSMTAP_RRC_SUB_SysInfoType5bis,
	GSMTAP_RRC_SUB_SysInfoType6,
	GSMTAP_RRC_SUB_SysInfoType7,
	GSMTAP_RRC_SUB_SysInfoType8,
	GSMTAP_RRC_SUB_SysInfoType9,
	GSMTAP_RRC_SUB_SysInfoType10,
	GSMTAP_RRC_SUB_SysInfoType11,
	GSMTAP_RRC_SUB_SysInfoType11bis,
	GSMTAP_RRC_SUB_SysInfoType12,
	GSMTAP_RRC_SUB_SysInfoType13,
	GSMTAP_RRC_SUB_SysInfoType13_1,
	GSMTAP_RRC_SUB_SysInfoType13_2,
	GSMTAP_RRC_SUB_SysInfoType13_3,
	GSMTAP_RRC_SUB_SysInfoType13_4,
	GSMTAP_RRC_SUB_SysInfoType14,
	GSMTAP_RRC_SUB_SysInfoType15,
	GSMTAP_RRC_SUB_SysInfoType15bis,
	GSMTAP_RRC_SUB_SysInfoType15_1,
	GSMTAP_RRC_SUB_SysInfoType15_1bis,
	GSMTAP_RRC_SUB_SysInfoType15_2,
	GSMTAP_RRC_SUB_SysInfoType15_2bis,
	GSMTAP_RRC_SUB_SysInfoType15_2ter,
	GSMTAP_RRC_SUB_SysInfoType15_3,
	GSMTAP_RRC_SUB_SysInfoType15_3bis,
	GSMTAP_RRC_SUB_SysInfoType15_4,
	GSMTAP_RRC_SUB_SysInfoType15_5,
	GSMTAP_RRC_SUB_SysInfoType15_6,
	GSMTAP_RRC_SUB_SysInfoType15_7,
	GSMTAP_RRC_SUB_SysInfoType15_8,
	GSMTAP_RRC_SUB_SysInfoType16,
	GSMTAP_RRC_SUB_SysInfoType17,
	GSMTAP_RRC_SUB_SysInfoType18,
	GSMTAP_RRC_SUB_SysInfoType19,
	GSMTAP_RRC_SUB_SysInfoType20,
	GSMTAP_RRC_SUB_SysInfoType21,
	GSMTAP_RRC_SUB_SysInfoType22,
	GSMTAP_RRC_SUB_SysInfoTypeSB1,
	GSMTAP_RRC_SUB_SysInfoTypeSB2,
	GSMTAP_RRC_SUB_ToTargetRNC_Container,
	GSMTAP_RRC_SUB_TargetRNC_ToSourceRNC_Container,

	GSMTAP_RRC_SUB_MAX
};

/* LTE RRC message types */
enum {
	GSMTAP_LTE_RRC_SUB_DL_CCCH_Message = 0,
	GSMTAP_LTE_RRC_SUB_DL_DCCH_Message,
	GSMTAP_LTE_RRC_SUB_UL_CCCH_Message,
	GSMTAP_LTE_RRC_SUB_UL_DCCH_Message,
	GSMTAP_LTE_RRC_SUB_BCCH_BCH_Message,
	GSMTAP_LTE_RRC_SUB_BCCH_DL_SCH_Message,
	GSMTAP_LTE_RRC_SUB_PCCH_Message,
	GSMTAP_LTE_RRC_SUB_MCCH_Message,
	GSMTAP_LTE_RRC_SUB_BCCH_BCH_Message_MBMS,
	GSMTAP_LTE_RRC_SUB_BCCH_DL_SCH_Message_BR,
	GSMTAP_LTE_RRC_SUB_BCCH_DL_SCH_Message_MBMS,
	GSMTAP_LTE_RRC_SUB_SC_MCCH_Message,
	GSMTAP_LTE_RRC_SUB_SBCCH_SL_BCH_Message,
	GSMTAP_LTE_RRC_SUB_SBCCH_SL_BCH_Message_V2X,
	GSMTAP_LTE_RRC_SUB_DL_CCCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_DL_DCCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_UL_CCCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_UL_DCCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_BCCH_BCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_BCCH_BCH_Message_TDD_NB,
	GSMTAP_LTE_RRC_SUB_BCCH_DL_SCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_PCCH_Message_NB,
	GSMTAP_LTE_RRC_SUB_SC_MCCH_Message_NB,

	GSMTAP_LTE_RRC_SUB_MAX
};

/* LTE NAS message types */
enum {
	GSMTAP_LTE_NAS_PLAIN = 0,
	GSMTAP_LTE_NAS_SEC_HEADER,

	GSMTAP_LTE_NAS_SUB_MAX
};

/* ====== DO NOT MAKE UNAPPROVED MODIFICATIONS HERE ===== */
/*! Structure of the GSMTAP pseudo-header */
struct gsmtap_hdr {
	uint8_t version;	/*!< version, set to 0x01 currently */
	uint8_t hdr_len;	/*!< length in number of 32bit words */
	uint8_t type;		/*!< see GSMTAP_TYPE_* */
	uint8_t timeslot;	/*!< timeslot (0..7 on Um) */

	uint16_t arfcn;		/*!< ARFCN (frequency) */
	int8_t signal_dbm;	/*!< signal level in dBm */
	int8_t snr_db;		/*!< signal/noise ratio in dB */

	uint32_t frame_number;	/*!< GSM Frame Number (FN) */

	uint8_t sub_type;	/*!< Type of burst/channel, see above */
	uint8_t antenna_nr;	/*!< Antenna Number */
	uint8_t sub_slot;	/*!< sub-slot within timeslot */
	uint8_t res;		/*!< reserved for future use (RFU) */

} __attribute__((packed));

/*! Structure of the GSMTAP libosmocore logging header */
struct gsmtap_osmocore_log_hdr {
	struct {
		uint32_t sec;
		uint32_t usec;
	} ts;
	char proc_name[16];	/*!< name of process */
	uint32_t pid;		/*!< process ID */
	uint8_t level;		/*!< logging level */
	uint8_t _pad[3];
	/* TODO: color */
	char subsys[16];	/*!< logging sub-system */
	struct {
		char name[32];	/*!< source file name */
		uint32_t line_nr;/*!< line number */
	} src_file;
} __attribute__((packed));

/*! First byte of type==GSMTAP_TYPE_UM sub_type==GSMTAP_CHANNEL_VOICE payload */
enum gsmtap_um_voice_type {
	/*! 1 byte TOC + 112 bits (14 octets) = 15 octets payload;
	 *  Reference is RFC5993 Section 5.2.1 + 3GPP TS 46.030 Annex B */
	GSMTAP_UM_VOICE_HR,
	/*! 33 payload bytes; Reference is RFC3551 Section 4.5.8.1 */
	GSMTAP_UM_VOICE_FR,
	/*! 31 payload bytes; Reference is RFC3551 Section 4.5.9 + ETSI TS 101 318 */
	GSMTAP_UM_VOICE_EFR,
	/*! 1 byte TOC + 5..31 bytes = 6..32 bytes payload; RFC4867 octet-aligned */
	GSMTAP_UM_VOICE_AMR,
	/* TODO: Revisit the types below; their usage; ... */
	GSMTAP_UM_VOICE_AMR_SID_BAD,
	GSMTAP_UM_VOICE_AMR_ONSET,
	GSMTAP_UM_VOICE_AMR_RATSCCH,
	GSMTAP_UM_VOICE_AMR_SID_UPDATE_INH,
	GSMTAP_UM_VOICE_AMR_SID_FIRST_P1,
	GSMTAP_UM_VOICE_AMR_SID_FIRST_P2,
	GSMTAP_UM_VOICE_AMR_SID_FIRST_INH,
	GSMTAP_UM_VOICE_AMR_RATSCCH_MARKER,
	GSMTAP_UM_VOICE_AMR_RATSCCH_DATA,
};
