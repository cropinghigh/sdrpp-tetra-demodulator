#pragma once

#include <stdint.h>
#include <osmocom/core/write_queue.h>
#include <osmocom/core/select.h>

/*! \defgroup gsmtap GSMTAP
 *  @{
 * \file gsmtap_util.h */

uint8_t chantype_rsl2gsmtap2(uint8_t rsl_chantype, uint8_t rsl_link_id, bool user_plane);

uint8_t chantype_rsl2gsmtap(uint8_t rsl_chantype, uint8_t rsl_link_id)
	OSMO_DEPRECATED("Use chantype_rsl2gsmtap2() instead");

void chantype_gsmtap2rsl(uint8_t gsmtap_chantype, uint8_t *rsl_chantype, uint8_t *link_id);

struct msgb *gsmtap_makemsg_ex(uint8_t type, uint16_t arfcn, uint8_t ts, uint8_t chan_type,
			    uint8_t ss, uint32_t fn, int8_t signal_dbm,
			    int8_t snr, const uint8_t *data, unsigned int len);

struct msgb *gsmtap_makemsg(uint16_t arfcn, uint8_t ts, uint8_t chan_type,
			    uint8_t ss, uint32_t fn, int8_t signal_dbm,
			    int8_t snr, const uint8_t *data, unsigned int len);

/*! one gsmtap instance */
struct gsmtap_inst;

int gsmtap_inst_fd(struct gsmtap_inst *gti)
	OSMO_DEPRECATED("Use gsmtap_inst_fd2() instead");

int gsmtap_inst_fd2(const struct gsmtap_inst *gti);

int gsmtap_source_init_fd(const char *host, uint16_t port);
int gsmtap_source_init_fd2(const char *local_host, uint16_t local_port, const char *rem_host, uint16_t rem_port);

int gsmtap_source_add_sink_fd(int gsmtap_fd);

struct gsmtap_inst *gsmtap_source_init(const char *host, uint16_t port,
					int ofd_wq_mode);
struct gsmtap_inst *gsmtap_source_init2(const char *local_host, uint16_t local_port,
					const char *rem_host, uint16_t rem_port, int ofd_wq_mode);

void gsmtap_source_free(struct gsmtap_inst *gti);

int gsmtap_source_add_sink(struct gsmtap_inst *gti);

int gsmtap_sendmsg(struct gsmtap_inst *gti, struct msgb *msg);
int gsmtap_sendmsg_free(struct gsmtap_inst *gti, struct msgb *msg);

int gsmtap_send_ex(struct gsmtap_inst *gti, uint8_t type, uint16_t arfcn, uint8_t ts,
		uint8_t chan_type, uint8_t ss, uint32_t fn,
		int8_t signal_dbm, int8_t snr, const uint8_t *data,
		unsigned int len);

int gsmtap_send(struct gsmtap_inst *gti, uint16_t arfcn, uint8_t ts,
		uint8_t chan_type, uint8_t ss, uint32_t fn,
		int8_t signal_dbm, int8_t snr, const uint8_t *data,
		unsigned int len);

extern const struct value_string gsmtap_gsm_channel_names[];
extern const struct value_string gsmtap_type_names[];

/*! @} */
