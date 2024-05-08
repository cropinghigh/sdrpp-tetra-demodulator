/*! \file gsm_08_58.h
 * GSM Radio Signalling Link messages on the A-bis interface.
 * 3GPP TS 08.58 version 8.6.0 Release 1999 / ETSI TS 100 596 V8.6.0 */
/*
 * (C) 2008 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#pragma once

#include <stdint.h>

#include <osmocom/core/endian.h>

/*! \addtogroup rsl
 *  @{
 * \file gsm_08_58.h */

/* Channel Number 9.3.1 */
union abis_rsl_chan_nr {
	struct {
#if OSMO_IS_BIG_ENDIAN
		uint8_t cbits:5,
			tn:3;
#elif OSMO_IS_LITTLE_ENDIAN
		uint8_t tn:3,
			cbits:5;
#endif
	} __attribute__ ((packed));
	uint8_t chan_nr;
} __attribute__ ((packed));
#define ABIS_RSL_CHAN_NR_CBITS_Bm_ACCHs	0x01
#define ABIS_RSL_CHAN_NR_CBITS_Lm_ACCHs(ss)	(0x02 + (ss))
#define ABIS_RSL_CHAN_NR_CBITS_SDCCH4_ACCH(ss)	(0x04 + (ss))
#define ABIS_RSL_CHAN_NR_CBITS_SDCCH8_ACCH(ss)	(0x08 + (ss))
#define ABIS_RSL_CHAN_NR_CBITS_BCCH		0x10
#define ABIS_RSL_CHAN_NR_CBITS_RACH		0x11
#define ABIS_RSL_CHAN_NR_CBITS_PCH_AGCH	0x12
#define ABIS_RSL_CHAN_NR_CBITS_OSMO_PDCH	0x18 /*< non-standard, for dyn TS */
#define ABIS_RSL_CHAN_NR_CBITS_OSMO_CBCH4	0x19 /*< non-standard, for CBCH/SDCCH4 */
#define ABIS_RSL_CHAN_NR_CBITS_OSMO_CBCH8	0x1a /*< non-standard, for CBCH/SDCCH8 */

/* non-standard, Osmocom specific Bm/Lm equivalents for VAMOS */
#define ABIS_RSL_CHAN_NR_CBITS_OSMO_VAMOS_Bm_ACCHs	0x1d		/*< VAMOS TCH/F */
#define ABIS_RSL_CHAN_NR_CBITS_OSMO_VAMOS_Lm_ACCHs(ss)	(0x1e + (ss))	/*< VAMOS TCH/H */

/* Link Identifier 9.3.2 */
union abis_rsl_link_id {
	struct {
#if OSMO_IS_BIG_ENDIAN
		uint8_t cbits:2,
			na:1,
			reserved:2,
			sapi:3;
#elif OSMO_IS_LITTLE_ENDIAN
		uint8_t sapi:3,
			reserved:2,
			na:1,
			cbits:2;
#endif
	} __attribute__ ((packed));
	uint8_t link_id;
} __attribute__ ((packed));
#define ABIS_RSL_LINK_ID_CBITS_FACCH_SDCCH 0x00
#define ABIS_RSL_LINK_ID_CBITS_SACCH 0x01

/*! RSL common header */
struct abis_rsl_common_hdr {
	uint8_t	msg_discr;	/*!< message discriminator (ABIS_RSL_MDISC_*) */
	uint8_t	msg_type;	/*!< message type (\ref abis_rsl_msgtype) */
	uint8_t	data[0];	/*!< actual payload data */
} __attribute__ ((packed));

/* RSL RLL header (Chapter 8.3) */
struct abis_rsl_rll_hdr {
	struct abis_rsl_common_hdr c;
	uint8_t	ie_chan;	/*!< \ref RSL_IE_CHAN_NR (tag) */
	union {
		uint8_t	chan_nr;	 /* API backward compat */
		union abis_rsl_chan_nr chan_nr_fields; /*!< RSL channel number (value) */
	};
	uint8_t	ie_link_id;	/*!< \ref RSL_IE_LINK_IDENT (tag) */
	union {
		uint8_t	link_id; /* API backward compat */
		union abis_rsl_link_id link_id_fields; /*!< RSL link identifier (value) */
	};
	uint8_t	data[0];	/*!< message payload data */
} __attribute__ ((packed));

/* RSL Dedicated Channel header (Chapter 8.3 and 8.4) */
struct abis_rsl_dchan_hdr {
	struct abis_rsl_common_hdr c;
	uint8_t	ie_chan;	/*!< \ref RSL_IE_CHAN_NR (tag) */
	union {
		uint8_t	chan_nr;	 /* API backward compat */
		union abis_rsl_chan_nr chan_nr_fields; /*!< RSL channel number (value) */
	};
	uint8_t	data[0];	/*!< message payload data */
} __attribute__ ((packed));

/* RSL Common Channel header (Chapter 8.5) */
struct abis_rsl_cchan_hdr {
	struct abis_rsl_common_hdr c;
	uint8_t	ie_chan;	/*!< \ref RSL_IE_CHAN_NR (tag) */
	union {
		uint8_t	chan_nr;	 /* API backward compat */
		union abis_rsl_chan_nr chan_nr_fields; /*!< RSL channel number (value) */
	};
	uint8_t	data[0];	/*!< message payload data */
} __attribute__ ((packed));

/* Osmocom specific IE to negotiate repeated ACCH capabilities */
struct abis_rsl_osmo_rep_acch_cap {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t dl_facch_cmd:1,
		dl_facch_all:1,
		dl_sacch:1,
		ul_sacch:1,
		rxqual:3,
		reserved:1;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved:1, rxqual:3, ul_sacch:1, dl_sacch:1, dl_facch_all:1, dl_facch_cmd:1;
#endif
} __attribute__ ((packed));

/* Osmocom specific IE to negotiate temporary overpower of ACCH channels */
struct abis_rsl_osmo_temp_ovp_acch_cap {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t overpower_db:3,
		rxqual:3,
		facch_enable:1,
		sacch_enable:1;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t sacch_enable:1, facch_enable:1, rxqual:3, overpower_db:3;
#endif
} __attribute__ ((packed));

/* Chapter 9.1 */
/* RSL Message Discriminator: RLL */
#define ABIS_RSL_MDISC_RLL		0x02
/* RSL Message Discriminator: Dedicated Channel */
#define ABIS_RSL_MDISC_DED_CHAN		0x08
/* RSL Message Discriminator: Common Channel */
#define ABIS_RSL_MDISC_COM_CHAN		0x0c
/* RSL Message Discriminator: TRX Management */
#define ABIS_RSL_MDISC_TRX		0x10
/* RSL Message Discriminator: Location Service */
#define ABIS_RSL_MDISC_LOC		0x20
/* RSL Message Discriminator: ip.access */
#define ABIS_RSL_MDISC_IPACCESS		0x7e
#define ABIS_RSL_MDISC_TRANSP		0x01

/* Check if given RSL message discriminator is transparent */
#define ABIS_RSL_MDISC_IS_TRANSP(x)	(x & 0x01)

/* RSL Message Type (Chapter 9.1) */
enum abis_rsl_msgtype {
	/* Radio Link Layer Management */
	RSL_MT_DATA_REQ			= 0x01,
	RSL_MT_DATA_IND,
	RSL_MT_ERROR_IND,
	RSL_MT_EST_REQ,
	RSL_MT_EST_CONF,
	RSL_MT_EST_IND,
	RSL_MT_REL_REQ,
	RSL_MT_REL_CONF,
	RSL_MT_REL_IND,
	RSL_MT_UNIT_DATA_REQ,
	RSL_MT_UNIT_DATA_IND,		/* 0x0b */
	RSL_MT_SUSP_REQ,		/* non-standard elements */
	RSL_MT_SUSP_CONF,
	RSL_MT_RES_REQ,
	RSL_MT_RECON_REQ,		/* 0x0f */

	/* Common Channel Management / TRX Management */
	RSL_MT_BCCH_INFO			= 0x11,
	RSL_MT_CCCH_LOAD_IND,
	RSL_MT_CHAN_RQD,
	RSL_MT_DELETE_IND,
	RSL_MT_PAGING_CMD,
	RSL_MT_IMMEDIATE_ASSIGN_CMD,
	RSL_MT_SMS_BC_REQ,
	RSL_MT_CHAN_CONF,		/* non-standard element */
	/* empty */
	RSL_MT_RF_RES_IND			= 0x19,
	RSL_MT_SACCH_FILL,
	RSL_MT_OVERLOAD,
	RSL_MT_ERROR_REPORT,
	RSL_MT_SMS_BC_CMD,
	RSL_MT_CBCH_LOAD_IND,
	RSL_MT_NOT_CMD,			/* 0x1f */

	/* Dedicate Channel Management */
	RSL_MT_CHAN_ACTIV			= 0x21,
	RSL_MT_CHAN_ACTIV_ACK,
	RSL_MT_CHAN_ACTIV_NACK,
	RSL_MT_CONN_FAIL,
	RSL_MT_DEACTIVATE_SACCH,
	RSL_MT_ENCR_CMD,
	RSL_MT_HANDO_DET,
	RSL_MT_MEAS_RES,
	RSL_MT_MODE_MODIFY_REQ,
	RSL_MT_MODE_MODIFY_ACK,
	RSL_MT_MODE_MODIFY_NACK,
	RSL_MT_PHY_CONTEXT_REQ,
	RSL_MT_PHY_CONTEXT_CONF,
	RSL_MT_RF_CHAN_REL,
	RSL_MT_MS_POWER_CONTROL,
	RSL_MT_BS_POWER_CONTROL,		/* 0x30 */
	RSL_MT_PREPROC_CONFIG,
	RSL_MT_PREPROC_MEAS_RES,
	RSL_MT_RF_CHAN_REL_ACK,
	RSL_MT_SACCH_INFO_MODIFY,
	RSL_MT_TALKER_DET,
	RSL_MT_LISTENER_DET,
	RSL_MT_REMOTE_CODEC_CONF_REP,
	RSL_MT_RTD_REP,
	RSL_MT_PRE_HANDO_NOTIF,
	RSL_MT_MR_CODEC_MOD_REQ,
	RSL_MT_MR_CODEC_MOD_ACK,
	RSL_MT_MR_CODEC_MOD_NACK,
	RSL_MT_MR_CODEC_MOD_PER,
	RSL_MT_TFO_REP,
	RSL_MT_TFO_MOD_REQ,		/* 0x3f */
	RSL_MT_LOCATION_INFO		= 0x41,

	/* ip.access specific RSL message types */
	RSL_MT_IPAC_DIR_RETR_ENQ	= 0x40,
	RSL_MT_IPAC_PDCH_ACT		= 0x48,
	RSL_MT_IPAC_PDCH_ACT_ACK,
	RSL_MT_IPAC_PDCH_ACT_NACK,
	RSL_MT_IPAC_PDCH_DEACT		= 0x4b,
	RSL_MT_IPAC_PDCH_DEACT_ACK,
	RSL_MT_IPAC_PDCH_DEACT_NACK,
	RSL_MT_IPAC_CONNECT_MUX		= 0x50,
	RSL_MT_IPAC_CONNECT_MUX_ACK,
	RSL_MT_IPAC_CONNECT_MUX_NACK,
	RSL_MT_IPAC_BIND_MUX		= 0x53,
	RSL_MT_IPAC_BIND_MUX_ACK,
	RSL_MT_IPAC_BIND_MUX_NACK,
	RSL_MT_IPAC_DISC_MUX		= 0x56,
	RSL_MT_IPAC_DISC_MUX_ACK,
	RSL_MT_IPAC_DISC_MUX_NACK,
	RSL_MT_IPAC_MEAS_PREPROC_DFT 	= 0x60,		/*Extented Common Channel Management */
	RSL_MT_IPAC_HO_CAN_ENQ 		= 0x61,
	RSL_MT_IPAC_HO_CAN_RES 		= 0x62,
	RSL_MT_IPAC_CRCX		= 0x70,		/* Bind to local BTS RTP port */
	RSL_MT_IPAC_CRCX_ACK,
	RSL_MT_IPAC_CRCX_NACK,
	RSL_MT_IPAC_MDCX		= 0x73,
	RSL_MT_IPAC_MDCX_ACK,
	RSL_MT_IPAC_MDCX_NACK,
	RSL_MT_IPAC_DLCX_IND		= 0x76,
	RSL_MT_IPAC_DLCX		= 0x77,
	RSL_MT_IPAC_DLCX_ACK,
	RSL_MT_IPAC_DLCX_NACK,

	RSL_MT_OSMO_ETWS_CMD		= 0x7f,
};

/*! Siemens vendor-specific RSL message types */
enum abis_rsl_msgtype_siemens {
	RSL_MT_SIEMENS_MRPCI		= 0x41,
	RSL_MT_SIEMENS_INTRAC_HO_COND_IND = 0x42,
	RSL_MT_SIEMENS_INTERC_HO_COND_IND = 0x43,
	RSL_MT_SIEMENS_FORCED_HO_REQ	= 0x44,
	RSL_MT_SIEMENS_PREF_AREA_REQ	= 0x45,
	RSL_MT_SIEMENS_PREF_AREA	= 0x46,
	RSL_MT_SIEMENS_START_TRACE	= 0x47,
	RSL_MT_SIEMENS_START_TRACE_ACK	= 0x48,
	RSL_MT_SIEMENS_STOP_TRACE	= 0x49,
	RSL_MT_SIEMENS_TRMR		= 0x4a,
	RSL_MT_SIEMENS_HO_FAIL_IND	= 0x4b,
	RSL_MT_SIEMENS_STOP_TRACE_ACK	= 0x4c,
	RSL_MT_SIEMENS_UPLF		= 0x4d,
	RSL_MT_SIEMENS_UPLB		= 0x4e,
	RSL_MT_SIEMENS_SET_SYS_INFO_10	= 0x4f,
	RSL_MT_SIEMENS_MODIF_COND_IND	= 0x50,
};

/*! Ericsson vendor-specific RSL message types */
enum abis_rsl_msgtype_ericsson {
	RSL_MT_ERICSSON_IMM_ASS_SENT	= 0x10,
};

/*! RSL Information Element Identifiers (Chapter 9.3) */
enum abis_rsl_ie {
	RSL_IE_CHAN_NR			= 0x01,
	RSL_IE_LINK_IDENT,
	RSL_IE_ACT_TYPE,
	RSL_IE_BS_POWER,
	RSL_IE_CHAN_IDENT,
	RSL_IE_CHAN_MODE,
	RSL_IE_ENCR_INFO,
	RSL_IE_FRAME_NUMBER,
	RSL_IE_HANDO_REF,
	RSL_IE_L1_INFO,
	RSL_IE_L3_INFO,
	RSL_IE_MS_IDENTITY,
	RSL_IE_MS_POWER,
	RSL_IE_PAGING_GROUP,
	RSL_IE_PAGING_LOAD,
	RSL_IE_PYHS_CONTEXT		= 0x10,
	RSL_IE_ACCESS_DELAY,
	RSL_IE_RACH_LOAD,
	RSL_IE_REQ_REFERENCE,
	RSL_IE_RELEASE_MODE,
	RSL_IE_RESOURCE_INFO,
	RSL_IE_RLM_CAUSE,
	RSL_IE_STARTNG_TIME,
	RSL_IE_TIMING_ADVANCE,
	RSL_IE_UPLINK_MEAS,
	RSL_IE_CAUSE,
	RSL_IE_MEAS_RES_NR,
	RSL_IE_MSG_ID,
	/* reserved */
	RSL_IE_SYSINFO_TYPE		= 0x1e,
	RSL_IE_MS_POWER_PARAM,
	RSL_IE_BS_POWER_PARAM,
	RSL_IE_PREPROC_PARAM,
	RSL_IE_PREPROC_MEAS,
	RSL_IE_IMM_ASS_INFO,		/* Phase 1 (3.6.0), later Full below */
	RSL_IE_SMSCB_INFO		= 0x24,
	RSL_IE_MS_TIMING_OFFSET,
	RSL_IE_ERR_MSG,
	RSL_IE_FULL_BCCH_INFO,
	RSL_IE_CHAN_NEEDED,
	RSL_IE_CB_CMD_TYPE,
	RSL_IE_SMSCB_MSG,
	RSL_IE_FULL_IMM_ASS_INFO,
	RSL_IE_SACCH_INFO,
	RSL_IE_CBCH_LOAD_INFO,
	RSL_IE_SMSCB_CHAN_INDICATOR,
	RSL_IE_GROUP_CALL_REF,
	RSL_IE_CHAN_DESC		= 0x30,
	RSL_IE_NCH_DRX_INFO,
	RSL_IE_CMD_INDICATOR,
	RSL_IE_EMLPP_PRIO,
	RSL_IE_UIC,
	RSL_IE_MAIN_CHAN_REF,
	RSL_IE_MR_CONFIG,
	RSL_IE_MR_CONTROL,
	RSL_IE_SUP_CODEC_TYPES,
	RSL_IE_CODEC_CONFIG,
	RSL_IE_RTD,
	RSL_IE_TFO_STATUS,
	RSL_IE_LLP_APDU,
	/* Siemens vendor-specific */
	RSL_IE_SIEMENS_MRPCI		= 0x40,
	RSL_IE_SIEMENS_PREF_AREA_TYPE	= 0x43,
	RSL_IE_SIEMENS_ININ_CELL_HO_PAR	= 0x45,
	RSL_IE_SIEMENS_TRACE_REF_NR	= 0x46,
	RSL_IE_SIEMENS_INT_TRACE_IDX	= 0x47,
	RSL_IE_SIEMENS_L2_HDR_INFO	= 0x48,
	RSL_IE_SIEMENS_HIGHEST_RATE	= 0x4e,
	RSL_IE_SIEMENS_SUGGESTED_RATE	= 0x4f,

	/* Osmocom specific */
	RSL_IE_OSMO_REP_ACCH_CAP	= 0x60,
	RSL_IE_OSMO_TRAINING_SEQUENCE	= 0x61,
	RSL_IE_OSMO_TEMP_OVP_ACCH_CAP	= 0x62,
	RSL_IE_OSMO_OSMUX_CID		= 0x63,

	/* ip.access */
	RSL_IE_IPAC_SRTP_CONFIG	= 0xe0,
	RSL_IE_IPAC_PROXY_UDP	= 0xe1,
	RSL_IE_IPAC_BSCMPL_TOUT	= 0xe2,
	RSL_IE_IPAC_REMOTE_IP	= 0xf0,
	RSL_IE_IPAC_REMOTE_PORT	= 0xf1,
	RSL_IE_IPAC_RTP_PAYLOAD	= 0xf2,
	RSL_IE_IPAC_LOCAL_PORT	= 0xf3,
	RSL_IE_IPAC_SPEECH_MODE	= 0xf4,
	RSL_IE_IPAC_LOCAL_IP	= 0xf5,
	RSL_IE_IPAC_CONN_STAT	= 0xf6,
	RSL_IE_IPAC_HO_C_PARMS	= 0xf7,
	RSL_IE_IPAC_CONN_ID	= 0xf8,
	RSL_IE_IPAC_RTP_CSD_FMT	= 0xf9,
	RSL_IE_IPAC_RTP_JIT_BUF	= 0xfa,
	RSL_IE_IPAC_RTP_COMPR	= 0xfb,
	RSL_IE_IPAC_RTP_PAYLOAD2 = 0xfc,
	RSL_IE_IPAC_RTP_MPLEX	= 0xfd,
	RSL_IE_IPAC_RTP_MPLEX_ID = 0xfe,
};

/* Ericsson specific IEs, clash with above partially, so they're not
 * part of the enum */
#define RSL_IE_ERIC_PAGING_GROUP	0x0e
#define RSL_IE_ERIC_INST_NR		0x48
#define RSL_IE_ERIC_PGSL_TIMERS		0x49
#define RSL_IE_ERIC_REPEAT_DL_FACCH	0x4a
#define RSL_IE_ERIC_POWER_INFO		0xf0
#define RSL_IE_ERIC_MOBILE_ID		0xf1
#define RSL_IE_ERIC_BCCH_MAPPING	0xf2
#define RSL_IE_ERIC_PACKET_PAG_IND	0xf3
#define RSL_IE_ERIC_CNTR_CTRL		0xf4
#define RSL_IE_ERIC_CNTR_CTRL_ACK	0xf5
#define RSL_IE_ERIC_CNTR_REPORT		0xf6
#define RSL_IE_ERIC_ICP_CONN		0xf7
#define RSL_IE_ERIC_EMR_SUPPORT		0xf8
#define RSL_IE_ERIC_EGPRS_REQ_REF	0xf9
#define RSL_IE_ERIC_VGCS_REL		0xfa
#define RSL_IE_ERIC_REP_PER_NCH		0xfb
#define RSL_IE_ERIC_NY2			0xfc
#define RSL_IE_ERIC_T3115		0xfd
#define RSL_IE_ERIC_ACTIVATE_FLAG	0xfe
#define RSL_IE_ERIC_FULL_NCH_INFO	0xff

/* IPAC MEAS_PREPROC AVERAGING METHOD */
enum {
	IPAC_UNWEIGHTED_AVE = 0,
	IPAC_WEIGHTED_AVE,
	IPAC_MEDIAN_AVE,
	/* EWMA is an Osmocom specific extension */
	IPAC_OSMO_EWMA_AVE,
};

/* IPAC MEAS_PREPROC AVERAGING PARAM ID */
enum {
	IPAC_RXLEV_AVE = 0,
	IPAC_RXQUAL_AVE,
	IPAC_MS_BTS_DIS_AVE
};

/* IPAC MEAS_PREPROC HO CAUSES */
enum {
	IPAC_HO_RQD_CAUSE_L_RXLEV_UL_H = 0x01,
	IPAC_HO_RQD_CAUSE_L_RXLEV_DL_H,
	IPAC_HO_RQD_CAUSE_L_RXQUAL_UL_H,
	IPAC_HO_RQD_CAUSE_L_RXQUAL_DL_H,
	IPAC_HO_RQD_CAUSE_RXLEV_UL_IH,
	IPAC_HO_RQD_CAUSE_RXLEV_DL_IH,
	IPAC_HO_RQD_CAUSE_MAX_MS_RANGE,
	IPAC_HO_RQD_CAUSE_POWER_BUDGET,
	IPAC_HO_RQD_CAUSE_ENQUIRY,
	IPAC_HO_RQD_CAUSE_ENQUIRY_FAILED,
	IPAC_HO_RQD_CAUSE_NORMAL3G,
	IPAC_HO_RQD_CAUSE_EMERGENCY3G,
	IPAC_HO_RQD_CAUSE_SERVICE_PREFERRED3G,
	IPAC_HO_RQD_CAUSE_O_M_SHUTDOWN,
	IPAC_HO_RQD_CAUSE_QUALITY_PROMOTION,
	IPAC_HO_RQD_CAUSE_LOAD_PROMOTION,
	IPAC_HO_RQD_CAUSE_LOAD_DEMOTION,
	IPAC_HO_RQD_CAUSE_MAX,
};

/* Chapter 9.3.1 */
#define RSL_CHAN_NR_MASK	0xf8
#define RSL_CHAN_NR_1		0x08	/*< bit to add for 2nd,... lchan */
#define RSL_CHAN_Bm_ACCHs	0x08
#define RSL_CHAN_Lm_ACCHs	0x10	/* .. 0x18 */
#define RSL_CHAN_SDCCH4_ACCH	0x20	/* .. 0x38 */
#define RSL_CHAN_SDCCH8_ACCH	0x40	/* ...0x78 */
#define RSL_CHAN_BCCH		0x80
#define RSL_CHAN_RACH		0x88
#define RSL_CHAN_PCH_AGCH	0x90
#define RSL_CHAN_OSMO_PDCH	0xc0	/*< non-standard, for dyn TS */
#define RSL_CHAN_OSMO_CBCH4	0xc8	/*< non-standard, for CBCH/SDCCH4 */
#define RSL_CHAN_OSMO_CBCH8	0xd0	/*< non-standard, for CBCH/SDCCH8 */

/* non-standard, Osmocom specific Bm/Lm equivalents for VAMOS */
#define RSL_CHAN_OSMO_VAMOS_Bm_ACCHs	0xe8	/* VAMOS TCH/F */
#define RSL_CHAN_OSMO_VAMOS_Lm_ACCHs	0xf0	/* VAMOS TCH/H */
#define RSL_CHAN_OSMO_VAMOS_MASK	0xe0	/* VAMOS TCH/{F,H} */

/* Chapter 9.3.3 */
#define RSL_ACT_TYPE_INITIAL	0x00
#define RSL_ACT_TYPE_REACT	0x80
#define RSL_ACT_INTRA_IMM_ASS	0x00
#define RSL_ACT_INTRA_NORM_ASS	0x01
#define RSL_ACT_INTER_ASYNC	0x02
#define RSL_ACT_INTER_SYNC	0x03
#define RSL_ACT_SECOND_ADD	0x04
#define RSL_ACT_SECOND_MULTI	0x05
#define RSL_ACT_OSMO_PDCH	0x0f	/*< non-standard, for dyn TS */

/*! RSL Channel Mode IF (Chapter 9.3.6) */
struct rsl_ie_chan_mode {
	uint8_t dtx_dtu;
	uint8_t spd_ind;
	uint8_t chan_rt;
	uint8_t chan_rate;
} __attribute__ ((packed));
#define RSL_CMOD_DTXu		0x01	/* uplink */
#define RSL_CMOD_DTXd		0x02	/* downlink */
enum rsl_cmod_spd {
	RSL_CMOD_SPD_SPEECH	= 0x01,
	RSL_CMOD_SPD_DATA	= 0x02,
	RSL_CMOD_SPD_SIGN	= 0x03,
};
/*! Channel rate and type */
enum rsl_cmod_crt {
	RSL_CMOD_CRT_SDCCH		= 0x01,
	RSL_CMOD_CRT_TCH_Bm		= 0x08,	/* full-rate */
	RSL_CMOD_CRT_TCH_Lm		= 0x09,	/* half-rate */
	RSL_CMOD_CRT_TCH_BI_Bm		= 0x0a,	/* full-rate: bi-directional (multislot) */
	RSL_CMOD_CRT_TCH_UNI_Bm		= 0x1a,	/* full-rate: uni-directional (multislot) */
	RSL_CMOD_CRT_TCH_GROUP_Bm	= 0x18,	/* full-rate: group call channel */
	RSL_CMOD_CRT_TCH_GROUP_Lm	= 0x19,	/* half-rate: group call channel */
	RSL_CMOD_CRT_TCH_BCAST_Bm	= 0x28,	/* full-rate: broadcast call channel */
	RSL_CMOD_CRT_TCH_BCAST_Lm	= 0x29,	/* half-rate: broadcast call channel */
	RSL_CMOD_CRT_OSMO_TCH_VAMOS_Bm	= 0x88,	/* full-rate in VAMOS mode */
	RSL_CMOD_CRT_OSMO_TCH_VAMOS_Lm	= 0x89,	/* half-rate in VAMOS mode */
};
/*! Speech */
enum rsl_cmod_sp {
	RSL_CMOD_SP_GSM1	= 0x01,
	RSL_CMOD_SP_GSM2	= 0x11,
	RSL_CMOD_SP_GSM3	= 0x21,
	RSL_CMOD_SP_GSM4	= 0x31,
	RSL_CMOD_SP_GSM5	= 0x09,
	RSL_CMOD_SP_GSM6	= 0x0d,
};
/*! Non-transparent data */
enum rsl_cmod_csd_nt {
	RSL_CMOD_CSD_NTA_43k5_14k5	= 0x61,	/* asymmetric 43.5 kbit/s (DL) + 14.5 kbit/s (UL) */
	RSL_CMOD_CSD_NTA_29k0_14k5	= 0x62,	/* asymmetric 29.0 kbit/s (DL) + 14.5 kbit/s (UL) */
	RSL_CMOD_CSD_NTA_43k5_29k0	= 0x63,	/* asymmetric 43.5 kbit/s (DL) + 29.0 kbit/s (UL) */
	RSL_CMOD_CSD_NTA_14k5_43k5	= 0x69,	/* asymmetric 14.5 kbit/s (DL) + 43.5 kbit/s (UL) */
	RSL_CMOD_CSD_NTA_14k5_29k0	= 0x6a,	/* asymmetric 14.5 kbit/s (DL) + 29.0 kbit/s (UL) */
	RSL_CMOD_CSD_NTA_29k0_43k5	= 0x6b,	/* asymmetric 29.0 kbit/s (DL) + 43.5 kbit/s (UL) */
	RSL_CMOD_CSD_NT_43k5		= 0x74,
	RSL_CMOD_CSD_NT_28k8		= 0x71,
	RSL_CMOD_CSD_NT_14k5		= 0x58,
	RSL_CMOD_CSD_NT_12k0		= 0x50,
	RSL_CMOD_CSD_NT_6k0		= 0x51,
};
/* legacy #defines with wrong name */
#define RSL_CMOD_SP_NT_14k5	RSL_CMOD_CSD_NT_14k5
#define RSL_CMOD_SP_NT_12k0	RSL_CMOD_CSD_NT_12k0
#define RSL_CMOD_SP_NT_6k0	RSL_CMOD_CSD_NT_6k0
#define RSL_CMOD_CSD_T_32000	RSL_CMOD_CSD_T_32k0
#define RSL_CMOD_CSD_T_29000	RSL_CMOD_CSD_T_29k0
#define RSL_CMOD_CSD_T_14400	RSL_CMOD_CSD_T_14k4
#define RSL_CMOD_CSD_T_9600	RSL_CMOD_CSD_T_9k6
#define RSL_CMOD_CSD_T_4800	RSL_CMOD_CSD_T_4k8
#define RSL_CMOD_CSD_T_2400	RSL_CMOD_CSD_T_2k4
#define RSL_CMOD_CSD_T_1200	RSL_CMOD_CSD_T_1k2
/*! Transparent data */
enum rsl_cmod_csd_t {
	RSL_CMOD_CSD_T_32k0	= 0x38,
	RSL_CMOD_CSD_T_29k0	= 0x39,
	RSL_CMOD_CSD_T_14k4	= 0x18,
	RSL_CMOD_CSD_T_9k6	= 0x10,
	RSL_CMOD_CSD_T_4k8	= 0x11,
	RSL_CMOD_CSD_T_2k4	= 0x12,
	RSL_CMOD_CSD_T_1k2	= 0x13,
	RSL_CMOD_CSD_T_600	= 0x14,
	RSL_CMOD_CSD_T_1200_75	= 0x15,
};

/*! RSL Channel Identification IE (Chapter 9.3.5) */
struct rsl_ie_chan_ident {
	/* GSM 04.08 10.5.2.5 */
	struct {
		uint8_t iei;
		uint8_t chan_nr;	/* enc_chan_nr */
		uint8_t oct3;
		uint8_t oct4;
	} chan_desc;
#if 0	/* spec says we need this but Abissim doesn't use it */
	struct {
		uint8_t tag;
		uint8_t len;
	} mobile_alloc;
#endif
} __attribute__ ((packed));

/* Chapter 9.3.22 */
#define RLL_CAUSE_T200_EXPIRED		0x01
#define RLL_CAUSE_REEST_REQ		0x02
#define RLL_CAUSE_UNSOL_UA_RESP		0x03
#define RLL_CAUSE_UNSOL_DM_RESP		0x04
#define RLL_CAUSE_UNSOL_DM_RESP_MF	0x05
#define RLL_CAUSE_UNSOL_SPRV_RESP	0x06
#define RLL_CAUSE_SEQ_ERR		0x07
#define RLL_CAUSE_UFRM_INC_PARAM	0x08
#define RLL_CAUSE_SFRM_INC_PARAM	0x09
#define RLL_CAUSE_IFRM_INC_MBITS	0x0a
#define RLL_CAUSE_IFRM_INC_LEN		0x0b
#define RLL_CAUSE_FRM_UNIMPL		0x0c
#define RLL_CAUSE_SABM_MF		0x0d
#define RLL_CAUSE_SABM_INFO_NOTALL	0x0e

/* Chapter 9.3.26 */
#define RSL_ERRCLS_NORMAL		0x00
#define RSL_ERRCLS_RESOURCE_UNAVAIL	0x20
#define RSL_ERRCLS_SERVICE_UNAVAIL	0x30
#define RSL_ERRCLS_SERVICE_UNIMPL	0x40
#define RSL_ERRCLS_INVAL_MSG		0x50
#define RSL_ERRCLS_PROTO_ERROR		0x60
#define RSL_ERRCLS_INTERWORKING		0x70

/* normal event */
#define RSL_ERR_RADIO_IF_FAIL		0x00
#define RSL_ERR_RADIO_LINK_FAIL		0x01
#define RSL_ERR_HANDOVER_ACC_FAIL	0x02
#define RSL_ERR_TALKER_ACC_FAIL		0x03
#define RSL_ERR_OM_INTERVENTION		0x07
#define RSL_ERR_NORMAL_UNSPEC		0x0f
#define RSL_ERR_T_MSRFPCI_EXP		0x18
/* resource unavailable */
#define RSL_ERR_EQUIPMENT_FAIL		0x20
#define RSL_ERR_RR_UNAVAIL		0x21
#define RSL_ERR_TERR_CH_FAIL		0x22
#define RSL_ERR_CCCH_OVERLOAD		0x23
#define RSL_ERR_ACCH_OVERLOAD		0x24
#define RSL_ERR_PROCESSOR_OVERLOAD	0x25
#define RSL_ERR_BTS_NOT_EQUIPPED	0x27
#define RSL_ERR_REMOTE_TRANSC_FAIL	0x28
#define RSL_ERR_NOTIFICATION_OVERFL	0x29
#define RSL_ERR_RES_UNAVAIL		0x2f
/* service or option not available */
#define RSL_ERR_TRANSC_UNAVAIL		0x30
#define RSL_ERR_SERV_OPT_UNAVAIL	0x3f
/* service or option not implemented */
#define RSL_ERR_ENCR_UNIMPL		0x40
#define RSL_ERR_SERV_OPT_UNIMPL		0x4f
/* invalid message */
#define RSL_ERR_RCH_ALR_ACTV_ALLOC	0x50
#define RSL_ERR_INVALID_MESSAGE		0x5f
/* protocol error */
#define RSL_ERR_MSG_DISCR		0x60
#define RSL_ERR_MSG_TYPE		0x61
#define RSL_ERR_MSG_SEQ			0x62
#define RSL_ERR_IE_ERROR		0x63
#define RSL_ERR_MAND_IE_ERROR		0x64
#define RSL_ERR_OPT_IE_ERROR		0x65
#define RSL_ERR_IE_NONEXIST		0x66
#define RSL_ERR_IE_LENGTH		0x67
#define RSL_ERR_IE_CONTENT		0x68
#define RSL_ERR_PROTO			0x6f
/* interworking */
#define RSL_ERR_INTERWORKING		0x7f

/* Chapter 9.3.30 */
#define RSL_SYSTEM_INFO_8	0x00
#define RSL_SYSTEM_INFO_1	0x01
#define RSL_SYSTEM_INFO_2	0x02
#define RSL_SYSTEM_INFO_3	0x03
#define RSL_SYSTEM_INFO_4	0x04
#define RSL_SYSTEM_INFO_5	0x05
#define RSL_SYSTEM_INFO_6	0x06
#define RSL_SYSTEM_INFO_7	0x07
#define RSL_SYSTEM_INFO_16	0x08
#define RSL_SYSTEM_INFO_17	0x09
#define RSL_SYSTEM_INFO_2bis	0x0a
#define RSL_SYSTEM_INFO_2ter	0x0b
#define RSL_SYSTEM_INFO_5bis	0x0d
#define RSL_SYSTEM_INFO_5ter	0x0e
#define RSL_SYSTEM_INFO_10	0x0f
#define RSL_EXT_MEAS_ORDER	0x47
#define RSL_MEAS_INFO		0x48
#define RSL_SYSTEM_INFO_13	0x28
#define RSL_ERIC_SYSTEM_INFO_13	0x0C
#define RSL_SYSTEM_INFO_2quater	0x29
#define RSL_SYSTEM_INFO_9	0x2a
#define RSL_SYSTEM_INFO_18	0x2b
#define RSL_SYSTEM_INFO_19	0x2c
#define RSL_SYSTEM_INFO_20	0x2d

/* Chapter 9.3.40 */
#define RSL_CHANNEED_ANY	0x00
#define RSL_CHANNEED_SDCCH	0x01
#define RSL_CHANNEED_TCH_F	0x02
#define RSL_CHANNEED_TCH_ForH	0x03

/*! RSL Cell Broadcast Command (Chapter 9.3.41) */
struct rsl_ie_cb_cmd_type {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t last_block:2;
	uint8_t spare:1;
	uint8_t def_bcast:1;
	uint8_t command:4;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t command:4, def_bcast:1, spare:1, last_block:2;
#endif
} __attribute__ ((packed));
/* ->command */
#define RSL_CB_CMD_TYPE_NORMAL		0x00
#define RSL_CB_CMD_TYPE_SCHEDULE	0x08
#define RSL_CB_CMD_TYPE_DEFAULT		0x0e
#define RSL_CB_CMD_TYPE_NULL		0x0f
/* ->def_bcast */
#define RSL_CB_CMD_DEFBCAST_NORMAL	0
#define RSL_CB_CMD_DEFBCAST_NULL	1
/* ->last_block */
#define RSL_CB_CMD_LASTBLOCK_4		0
#define RSL_CB_CMD_LASTBLOCK_1		1
#define RSL_CB_CMD_LASTBLOCK_2		2
#define RSL_CB_CMD_LASTBLOCK_3		3

/*! NCH DRX Information (Chapter 9.3.47) */
struct rsl_ie_nch_drx_info {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t nln:2;
	uint8_t emlpp_priority:3;
	uint8_t nln_status:1;
	uint8_t spare:2;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t spare:2, nln_status:1, emlpp_priority:3, nln:2;
#endif
} __attribute__ ((packed));

/*! Command Indicator (Chapter 9.3.48) */
#define RSL_CMD_INDICATOR_START	0x00
#define RSL_CMD_INDICATOR_STOP	0x01

/* Chapter 3.3.2.3 Brocast control channel */
/* CCCH-CONF, NC is not combined */
#define RSL_BCCH_CCCH_CONF_1_NC	0x00
#define RSL_BCCH_CCCH_CONF_1_C	0x01
#define RSL_BCCH_CCCH_CONF_2_NC	0x02
#define RSL_BCCH_CCCH_CONF_3_NC	0x04
#define RSL_BCCH_CCCH_CONF_4_NC	0x06

/* BS-PA-MFRMS */
#define RSL_BS_PA_MFRMS_2	0x00
#define RSL_BS_PA_MFRMS_3	0x01
#define RSL_BS_PA_MFRMS_4	0x02
#define RSL_BS_PA_MFRMS_5	0x03
#define RSL_BS_PA_MFRMS_6	0x04
#define RSL_BS_PA_MFRMS_7	0x05
#define RSL_BS_PA_MFRMS_8	0x06
#define RSL_BS_PA_MFRMS_9	0x07

/* RSL_IE_IPAC_RTP_PAYLOAD[2] */
enum rsl_ipac_rtp_payload {
	RSL_IPAC_RTP_GSM	= 1,
	RSL_IPAC_RTP_EFR,
	RSL_IPAC_RTP_AMR,
	RSL_IPAC_RTP_CSD,
	RSL_IPAC_RTP_MUX,
};

/* RSL_IE_IPAC_SPEECH_MODE, lower four bits */
enum rsl_ipac_speech_mode_s {
	RSL_IPAC_SPEECH_GSM_FR = 0,	/* GSM FR (Type 1, FS) */
	RSL_IPAC_SPEECH_GSM_EFR = 1,	/* GSM EFR (Type 2, FS) */
	RSL_IPAC_SPEECH_GSM_AMR_FR = 2,	/* GSM AMR/FR (Type 3, FS) */
	RSL_IPAC_SPEECH_GSM_HR = 3,	/* GSM HR (Type 1, HS) */
	RSL_IPAC_SPEECH_GSM_AMR_HR = 5,	/* GSM AMR/hr (Type 3, HS) */
	RSL_IPAC_SPEECH_AS_RTP = 0xf,	/* As specified by RTP Payload IE */
};
/* RSL_IE_IPAC_SPEECH_MODE, upper four bits */
enum rsl_ipac_speech_mode_m {
	RSL_IPAC_SPEECH_M_RXTX = 0,	/* Send and Receive */
	RSL_IPAC_SPEECH_M_RX = 1,	/* Receive only */
	RSL_IPAC_SPEECH_M_TX = 2,	/* Send only */
};

/* RSL_IE_IPAC_RTP_CSD_FMT, lower four bits */
enum rsl_ipac_rtp_csd_format_d {
	RSL_IPAC_RTP_CSD_EXT_TRAU = 0,	/*!< TRAU-like RTP format, without leading zero-bits */
	RSL_IPAC_RTP_CSD_NON_TRAU = 1,	/*!< packed 16k (252/288 bit) / 8k (126 bit) in RTP */
	RSL_IPAC_RTP_CSD_TRAU_BTS = 2,	/*!< TRAU in BTS; V.110 in RTP/CLEARMODE */
	RSL_IPAC_RTP_CSD_IWF_FREE = 3,	/*!< unknown proprietary IWF-free BTS-BTS data */
};
/* RSL_IE_IPAC_RTP_CSD_FMT, upper four bits */
enum rsl_ipac_rtp_csd_format_ir {
	RSL_IPAC_RTP_CSD_IR_8k = 0,
	RSL_IPAC_RTP_CSD_IR_16k = 1,
	RSL_IPAC_RTP_CSD_IR_32k = 2,
	RSL_IPAC_RTP_CSD_IR_64k = 3,
};

/* Siemens vendor-specific RSL extensions */
struct rsl_mrpci {
	uint8_t power_class:3,
		 vgcs_capable:1,
		 vbs_capable:1,
		 gsm_phase:2;
} __attribute__ ((packed));

enum rsl_mrpci_pwrclass {
	RSL_MRPCI_PWRC_1	= 0,
	RSL_MRPCI_PWRC_2	= 1,
	RSL_MRPCI_PWRC_3	= 2,
	RSL_MRPCI_PWRC_4	= 3,
	RSL_MRPCI_PWRC_5	= 4,
};
enum rsl_mrpci_phase {
	RSL_MRPCI_PHASE_1	= 0,
	/* reserved */
	RSL_MRPCI_PHASE_2	= 2,
	RSL_MRPCI_PHASE_2PLUS	= 3,
};

/* 9.3.20 Release Mode */
enum rsl_rel_mode {
	RSL_REL_NORMAL		= 0,
	RSL_REL_LOCAL_END	= 1,
};

/*! ip.access specific embedded information elements */
enum rsl_ipac_embedded_ie {
	RSL_IPAC_EIE_RXLEV		= 0x00,
	RSL_IPAC_EIE_RXQUAL		= 0x01,
	RSL_IPAC_EIE_FREQ_ERR		= 0x02,
	RSL_IPAC_EIE_TIMING_ERR		= 0x03,
	RSL_IPAC_EIE_MEAS_AVG_CFG	= 0x04,
	RSL_IPAC_EIE_BS_PWR_CTL		= 0x05,
	RSL_IPAC_EIE_MS_PWR_CTL		= 0x06,
	RSL_IPAC_EIE_HANDO_THRESH	= 0x07,
	RSL_IPAC_EIE_NCELL_DEFAULTS	= 0x08,
	RSL_IPAC_EIE_NCELL_LIST		= 0x09,
	RSL_IPAC_EIE_PC_THRESH_COMP	= 0x0a,
	RSL_IPAC_EIE_HO_THRESH_COMP	= 0x0b,
	RSL_IPAC_EIE_HO_CAUSE		= 0x0c,
	RSL_IPAC_EIE_HO_CANDIDATES	= 0x0d,
	RSL_IPAC_EIE_NCELL_BA_CHG_LIST	= 0x0e,
	RSL_IPAC_EIE_NUM_OF_MS		= 0x10,
	RSL_IPAC_EIE_HO_CAND_EXT	= 0x11,
	RSL_IPAC_EIE_NCELL_DEF_EXT	= 0x12,
	RSL_IPAC_EIE_NCELL_LIST_EXT	= 0x13,
	RSL_IPAC_EIE_MASTER_KEY		= 0x14,
	RSL_IPAC_EIE_MASTER_SALT	= 0x15,
	/* additional IPAC measurement pre-processing related IEI */
	RSL_IPAC_EIE_MEAS_TRANS_RES	= 0x16,
	RSL_IPAC_EIE_3G_HO_PARAM	= 0x17,
	RSL_IPAC_EIE_3G_NCELL_LIST	= 0x18,
	RSL_IPAC_EIE_SDCCH_CTL_PARAM	= 0x1a,
	RSL_IPAC_EIE_AMR_CONV_THRESH 	= 0x1b,

	/* Osmocom specific extensions: */
	RSL_IPAC_EIE_OSMO_MEAS_AVG_CFG	= 0xf0,
	RSL_IPAC_EIE_OSMO_MS_PWR_CTL	= 0xf1,
	RSL_IPAC_EIE_OSMO_PC_THRESH_COMP = 0xf2,

};

/* Value of TLV IE RSL_IPAC_EIE_MEAS_AVG_CFG */
struct ipac_preproc_ave_cfg {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t h_reqave:5,
		param_id:2,
		reserved:1;
	uint8_t h_reqt:5,
		ave_method:3;
	uint8_t params[0];
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved:1, param_id:2, h_reqave:5;
	uint8_t ave_method:3, h_reqt:5;
	uint8_t params[0];
#endif
}__attribute__ ((packed));


struct osmo_preproc_ave_cfg_field {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t h_reqave:5,
		ave_enabled:1,
		reserved:2;
	uint8_t h_reqt:5,
		ave_method:3;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved:2, ave_enabled:1, h_reqave:5;
	uint8_t ave_method:3, h_reqt:5;
#endif
}__attribute__ ((packed));
/* Value of TLV IE RSL_IPAC_EIE_OSMO_MEAS_AVG_CFG: */
struct osmo_preproc_ave_cfg {
	struct osmo_preproc_ave_cfg_field ci_fr;
	struct osmo_preproc_ave_cfg_field ci_hr;
	struct osmo_preproc_ave_cfg_field ci_amr_fr;
	struct osmo_preproc_ave_cfg_field ci_amr_hr;
	struct osmo_preproc_ave_cfg_field ci_sdcch;
	struct osmo_preproc_ave_cfg_field ci_gprs;
	uint8_t params[0]; /* Contains params for each above, appended one after the other */
}__attribute__ ((packed));

/*! MS/BS Power Control Thresholds (RSL_IPAC_EIE_MS_PWR_CTL) */
struct ipac_preproc_pc_thresh {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t l_rxlev:6, reserved_l_rxlev:2;
	uint8_t u_rxlev:6, reserved_u_rxlev:2;
	uint8_t u_rxqual:3, reserved_u_rxqual:1,
		l_rxqual:3, reserved_l_rxqual:1;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved_l_rxlev:2, l_rxlev:6;
	uint8_t reserved_u_rxlev:2, u_rxlev:6;
	uint8_t reserved_l_rxqual:1, l_rxqual:3, reserved_u_rxqual:1, u_rxqual:3;
#endif
}__attribute__ ((packed));

/*! Osmocom extension for: MS/BS Power Control Thresholds (RSL_IPAC_EIE_OSMO_MS_PWR_CTL) */
struct osmo_preproc_pc_thresh {
	/* Carrier-to-Interference (C/I), in dB: */
	int8_t l_ci_fr; int8_t u_ci_fr; /* FR/EFR */
	int8_t l_ci_hr; int8_t u_ci_hr; /* HR */
	int8_t l_ci_amr_fr; int8_t u_ci_amr_fr; /* AMR FR */
	int8_t l_ci_amr_hr; int8_t u_ci_amr_hr; /* AMR HR */
	int8_t l_ci_sdcch; int8_t u_ci_sdcch; /* SDCCH */
	int8_t l_ci_gprs; int8_t u_ci_gprs; /* GPRS */
}__attribute__ ((packed));

/*! Handover Thresholds */
struct ipac_preproc_ho_thresh {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t l_rxlev_ul_h:6,
		reserved_l_rxlev_ul:2;
	uint8_t l_rxlev_dl_h:6,
		reserved_l_rxlev_dl:2;
	uint8_t rxlev_ul_ih:6,
		reserved_rxlev_ul:2;
	uint8_t rxlev_dl_ih:6,
		reserved_rxlev_dl:2;
	uint8_t l_rxqual_ul_h:3,
		reserved_rxlqual_ul:1,
		l_rxqual_dl_h:3,
		reserved_rxqual_dl:1;
	uint8_t ms_range_max:6,
		reserved_ms_range:2;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved_l_rxlev_ul:2, l_rxlev_ul_h:6;
	uint8_t reserved_l_rxlev_dl:2, l_rxlev_dl_h:6;
	uint8_t reserved_rxlev_ul:2, rxlev_ul_ih:6;
	uint8_t reserved_rxlev_dl:2, rxlev_dl_ih:6;
	uint8_t reserved_rxqual_dl:1, l_rxqual_dl_h:3, reserved_rxlqual_ul:1, l_rxqual_ul_h:3;
	uint8_t reserved_ms_range:2, ms_range_max:6;
#endif
}__attribute__ ((packed));

/*! PC Threshold Comparators (RSL_IPAC_EIE_PC_THRESH_COMP) */
struct ipac_preproc_pc_comp {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t p1:5, reserved_p1:3;
	uint8_t n1:5, reserved_n1:3;
	uint8_t p2:5, reserved_p2:3;
	uint8_t n2:5, reserved_n2:3;
	uint8_t p3:5, reserved_p3:3;
	uint8_t n3:5, reserved_n3:3;
	uint8_t p4:5, reserved_p4:3;
	uint8_t n4:5, reserved_n4:3;
	uint8_t pc_interval:5, reserved_pc:3;
	uint8_t red_step_size:4, inc_step_size:4;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved_p1:3, p1:5;
	uint8_t reserved_n1:3, n1:5;
	uint8_t reserved_p2:3, p2:5;
	uint8_t reserved_n2:3, n2:5;
	uint8_t reserved_p3:3, p3:5;
	uint8_t reserved_n3:3, n3:5;
	uint8_t reserved_p4:3, p4:5;
	uint8_t reserved_n4:3, n4:5;
	uint8_t reserved_pc:3, pc_interval:5;
	uint8_t inc_step_size:4, red_step_size:4;
#endif
}__attribute__ ((packed));

/*! Osmocom extension for: PC Threshold Comparators (RSL_IPAC_EIE_OSMO_PC_THRESH_COMP) */
struct ipac_preproc_pc_comp_field {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t lower_p:5, reserved_lower_p:3;
	uint8_t lower_n:5, reserved_lower_n:3;
	uint8_t upper_p:5, reserved_upper_p:3;
	uint8_t upper_n:5, reserved_upper_n:3;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved_lower_p:3, lower_p:5;
	uint8_t reserved_lower_n:3, lower_n:5;
	uint8_t reserved_upper_p:3, upper_p:5;
	uint8_t reserved_upper_n:3, upper_n:5;
#endif
}__attribute__ ((packed));
struct osmo_preproc_pc_comp {
	/* Used for Carrier-to-Interference (C/I), in dB: */
	struct ipac_preproc_pc_comp_field ci_fr;
	struct ipac_preproc_pc_comp_field ci_hr;
	struct ipac_preproc_pc_comp_field ci_amr_fr;
	struct ipac_preproc_pc_comp_field ci_amr_hr;
	struct ipac_preproc_pc_comp_field ci_sdcch;
	struct ipac_preproc_pc_comp_field ci_gprs;
}__attribute__ ((packed));

/*! HO Threshold Comparators */
struct ipac_preproc_ho_comp {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t p5:5,
		reserved_p5:3;
	uint8_t n5:5,
		reserved_n5:3;
	uint8_t p6:5,
		reserved_p6:3;
	uint8_t n6:5,
		reserved_n6:3;
	uint8_t p7:5,
		reserved_p7:3;
	uint8_t n7:5,
		reserved_n7:3;
	uint8_t p8:5,
		reserved_p8:3;
	uint8_t n8:5,
		reserved_n8:3;
	uint8_t ho_interval:5,
		reserved_ho:3;
	uint8_t reserved;

#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved_p5:3, p5:5;
	uint8_t reserved_n5:3, n5:5;
	uint8_t reserved_p6:3, p6:5;
	uint8_t reserved_n6:3, n6:5;
	uint8_t reserved_p7:3, p7:5;
	uint8_t reserved_n7:3, n7:5;
	uint8_t reserved_p8:3, p8:5;
	uint8_t reserved_n8:3, n8:5;
	uint8_t reserved_ho:3, ho_interval:5;
	uint8_t reserved;
#endif
}__attribute__ ((packed));

struct ipac_preproc_ho_candidates {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t bsic:6,
		reserved0:2;
	uint8_t bcch_freq:5,
		ba_used:1,
		s:1,
		reserved1:1;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved0:2, bsic:6;
	uint8_t reserved1:1, s:1, ba_used:1, bcch_freq:5;
#endif
}__attribute__ ((packed));

struct ipac_preproc_ncell_dflts {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t rxlev_min_def:6,
		reserved_rxlev_min_def:2;
	uint8_t ho_margin_def:5,
		reserved_ho_margin_def:3;
	uint8_t ms_txpwr_max_def:5,
		reserved_ms_txpwr_max_def:3;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved_rxlev_min_def:2, rxlev_min_def:6;
	uint8_t reserved_ho_margin_def:3, ho_margin_def:5;
	uint8_t reserved_ms_txpwr_max_def:3, ms_txpwr_max_def:5;
#endif
}__attribute__ ((packed));

struct ipac_preproc_ho_ctl_param {
#if OSMO_IS_LITTLE_ENDIAN
	uint8_t sdcch_ho_gsm:1,
		sdcch_ho_umts:1,
		reserved:6;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
	uint8_t reserved:6, sdcch_ho_umts:1, sdcch_ho_gsm:1;
#endif
}__attribute__ ((packed));

struct ipac_preproc_cfg {
	uint8_t meas_rep_mode;
	uint32_t meas_mode_flags;
	struct ipac_preproc_ave_cfg ms_ave_cfg[3];
	struct ipac_preproc_ave_cfg ave_cfg;
	struct ipac_preproc_ho_thresh ho_thresh;
	struct ipac_preproc_ho_comp ho_comp;
	struct ipac_preproc_ncell_dflts ncell_dflts;
	struct ipac_preproc_ho_ctl_param ho_ctl_param;
};

struct rsl_l1_info {
#if OSMO_IS_LITTLE_ENDIAN
		uint8_t reserved:1,
			srr_sro:1,
			fpc_epc:1,
			ms_pwr:5;
		uint8_t ta;
#elif OSMO_IS_BIG_ENDIAN
/* auto-generated from the little endian part above (libosmocore/contrib/struct_endianness.py) */
		uint8_t ms_pwr:5, fpc_epc:1, srr_sro:1, reserved:1;
		uint8_t ta;
#endif
} __attribute__ ((packed));

/*! @} */
