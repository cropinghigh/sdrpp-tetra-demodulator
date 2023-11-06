#ifndef TETRA_COMMON_H
#define TETRA_COMMON_H

#include <stdint.h>
#include "tetra_mac_pdu.h"
#include <stdbool.h>
#include <osmocom/core/linuxlist.h>

#include "tetra_common.h"
#include "tetra_mac_pdu.h"

#ifdef DEBUG
#define DEBUGP(x, args...)	printf(x, ## args)
#else
#define DEBUGP(x, args...)	do { } while (0)
#endif

#define TETRA_SYM_PER_TS	255
#define TETRA_BITS_PER_TS	(TETRA_SYM_PER_TS*2)

/* Chapter 22.2.x */
enum tetra_log_chan {
	TETRA_LC_UNKNOWN,
	/* TMA SAP */
	TETRA_LC_SCH_F,
	TETRA_LC_SCH_HD,
	TETRA_LC_SCH_HU,
	TETRA_LC_STCH,
	TETRA_LC_SCH_P8_F,
	TETRA_LC_SCH_P8_HD,
	TETRA_LC_SCH_P8_HU,

	TETRA_LC_AACH,
	TETRA_LC_TCH,
	TETRA_LC_BSCH,
	TETRA_LC_BNCH,

	/* FIXME: QAM */
};

uint32_t bits_to_uint(const uint8_t *bits, unsigned int len);

#include "tetra_tdma.h"
struct tetra_phy_state {
	struct tetra_tdma_time time;
};
extern struct tetra_phy_state t_phy_state;

struct tetra_display_state {
	int curr_hyperframe;//
	int curr_multiframe;//
	int curr_frame;//
	int timeslot_content[4]; //0-other, 1-NORM1, 2-NORM2, 3-SYNC//
	int dl_usage;//
	int ul_usage;//
	char access1_code;//
	char access2_code;//
	int access1;//
	int access2;//
	int dl_freq;//
	int ul_freq;//
	int mcc;//
	int mnc;//
	int cc;//
	bool last_crc_fail;//
	bool advanced_link;
	bool air_encryption;
	bool sndcp_data;
	bool circuit_data;
	bool voice_service;
	bool normal_mode;
	bool migration_supported;
	bool never_minimum_mode;
	bool priority_cell;
	bool dereg_mandatory;
	bool reg_mandatory;
};

struct tetra_mac_state {
	struct llist_head voice_channels;
	struct {
		int is_traffic;
		bool blk1_stolen;
		bool blk2_stolen;
	} cur_burst;
	struct tetra_si_decoded last_sid;

	struct tetra_crypto_state *tcs; /* contains all state relevant to encryption */

	char *dumpdir;	/* Where to save traffic channel dump */
	int ssi;	/* SSI */
	int tsn;	/* Timeslon number */
	int usage_marker; /* Usage marker (if addressed)*/
	int addr_type;
	
	struct tetra_display_state *t_display_st;
	bool codec_first_pass;
	
	void (*put_voice_data)(void* ctx, int count, int16_t* data);
	void* put_voice_data_ctx;
	int last_frame;
	int curr_active_timeslot;
};

extern struct tetra_display_state t_display_state;

void tetra_mac_state_init(struct tetra_mac_state *tms);

#define TETRA_CRC_OK	0x1d0f

uint32_t tetra_dl_carrier_hz(uint8_t band, uint16_t carrier, uint8_t offset);
uint32_t tetra_ul_carrier_hz(uint8_t band, uint16_t carrier, uint8_t offset,
			     uint8_t duplex, uint8_t reverse);

const char *tetra_get_lchan_name(enum tetra_log_chan lchan);
const char *tetra_get_sap_name(uint8_t sap);
#endif
