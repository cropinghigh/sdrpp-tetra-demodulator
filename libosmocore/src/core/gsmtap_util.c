/*! \file gsmtap_util.c
 * GSMTAP support code in libosmocore. */
/*
 * (C) 2010-2017 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
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

#include "osmo_config.h"

#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/gsmtap.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/select.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/byteswap.h>
#include <osmocom/core/utils.h>
#include <osmocom/gsm/protocol/gsm_04_08.h>
#include <osmocom/gsm/rsl.h>

#include <sys/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

/*! \addtogroup gsmtap
 *  @{
 *  GSMTAP utility routines. Encapsulates GSM messages over UDP.
 *
 * \file gsmtap_util.c */

/*! one gsmtap instance
 *  Until gsmtap_inst_fd() is removed from the API at some point in the future, we have to keep the first member as
 *  'int' and the second as 'struct osmo_wqueue' (this effectively makes sure that the struct member wq.bfd.fd maintains
 *  the same memory offset from the start of the struct) to ensure that inlined static 'instances' of gsmtap_inst_fd() in
 *  old binaries keep working the way they used to even with gsmtap_inst objects obtained from newer versions of libosmocore */
struct gsmtap_inst {
	int osmo_io_mode;	  /*!< Indicates whether or not to use Osmo IO mode for message output (thus enabling use of tx queues).
				   *   This field member may not be changed or moved (backwards compatibility) */
	struct osmo_wqueue wq;	  /*!< the wait queue. This field member may not be changed or moved (backwards compatibility) */

	struct osmo_io_fd *out;	  /*!< Used when osmo_io_mode is nonzero */
	int sink_fd;
};

struct _gsmtap_inst_legacy {
	int ofd_wq_mode;
	struct osmo_wqueue wq;
	struct osmo_fd sink_ofd;
};
osmo_static_assert(offsetof(struct gsmtap_inst, wq) == offsetof(struct _gsmtap_inst_legacy, wq),
		   gsmtap_inst_new_wq_offset_equals_legacy_wq_offset);

/*! Deprecated, use gsmtap_inst_fd2() instead
 *  \param[in] gti GSMTAP instance
 *  \returns file descriptor of GSMTAP instance */
int gsmtap_inst_fd(struct gsmtap_inst *gti)
{
	return gsmtap_inst_fd2(gti);
}

/*! obtain the file descriptor associated with a gsmtap instance
 *  \param[in] gti GSMTAP instance
 *  \returns file descriptor of GSMTAP instance */
int gsmtap_inst_fd2(const struct gsmtap_inst *gti)
{
	return gti->wq.bfd.fd;
}

/*! convert RSL channel number to GSMTAP channel type
 *  \param[in] rsl_chantype RSL channel type
 *  \param[in] link_id RSL link identifier
 *  \param[in] user_plane Is this voice/csd user plane (1) or signaling (0)
 *  \returns GSMTAP channel type
 */
uint8_t chantype_rsl2gsmtap2(uint8_t rsl_chantype, uint8_t link_id, bool user_plane)
{
	uint8_t ret = GSMTAP_CHANNEL_UNKNOWN;

	switch (rsl_chantype) {
	case RSL_CHAN_Bm_ACCHs:
	case RSL_CHAN_OSMO_VAMOS_Bm_ACCHs:
		if (user_plane)
			ret = GSMTAP_CHANNEL_VOICE_F;
		else
			ret = GSMTAP_CHANNEL_FACCH_F;
		break;
	case RSL_CHAN_Lm_ACCHs:
	case RSL_CHAN_OSMO_VAMOS_Lm_ACCHs:
		if (user_plane)
			ret = GSMTAP_CHANNEL_VOICE_H;
		else
			ret = GSMTAP_CHANNEL_FACCH_H;
		break;
	case RSL_CHAN_SDCCH4_ACCH:
		ret = GSMTAP_CHANNEL_SDCCH4;
		break;
	case RSL_CHAN_SDCCH8_ACCH:
		ret = GSMTAP_CHANNEL_SDCCH8;
		break;
	case RSL_CHAN_BCCH:
		ret = GSMTAP_CHANNEL_BCCH;
		break;
	case RSL_CHAN_RACH:
		ret = GSMTAP_CHANNEL_RACH;
		break;
	case RSL_CHAN_PCH_AGCH:
		/* it could also be AGCH... */
		ret = GSMTAP_CHANNEL_PCH;
		break;
	case RSL_CHAN_OSMO_PDCH:
		ret = GSMTAP_CHANNEL_PDCH;
		break;
	case RSL_CHAN_OSMO_CBCH4:
		ret = GSMTAP_CHANNEL_CBCH51;
		break;
	case RSL_CHAN_OSMO_CBCH8:
		ret = GSMTAP_CHANNEL_CBCH52;
		break;
	}

	if (link_id & 0x40)
		ret |= GSMTAP_CHANNEL_ACCH;

	return ret;
}

/*! convert RSL channel number to GSMTAP channel type
 *  \param[in] rsl_chantype RSL channel type
 *  \param[in] link_id RSL link identifier
 *  \returns GSMTAP channel type
 */
uint8_t chantype_rsl2gsmtap(uint8_t rsl_chantype, uint8_t link_id)
{
	return chantype_rsl2gsmtap2(rsl_chantype, link_id, false);
}

/*! convert GSMTAP channel type to RSL channel number + Link ID
 *  \param[in] gsmtap_chantype GSMTAP channel type
 *  \param[out] rsl_chantype RSL channel mumber
 *  \param[out] link_id RSL link identifier
 */
void chantype_gsmtap2rsl(uint8_t gsmtap_chantype, uint8_t *rsl_chantype,
                         uint8_t *link_id)
{
	switch (gsmtap_chantype & ~GSMTAP_CHANNEL_ACCH & 0xff) {
	case GSMTAP_CHANNEL_FACCH_F:
	case GSMTAP_CHANNEL_VOICE_F: // TCH/F
		*rsl_chantype = RSL_CHAN_Bm_ACCHs;
		break;
	case GSMTAP_CHANNEL_FACCH_H:
	case GSMTAP_CHANNEL_VOICE_H: // TCH/H
		*rsl_chantype = RSL_CHAN_Lm_ACCHs;
		break;
	case GSMTAP_CHANNEL_SDCCH4: // SDCCH/4
		*rsl_chantype = RSL_CHAN_SDCCH4_ACCH;
		break;
	case GSMTAP_CHANNEL_SDCCH8: // SDCCH/8
		*rsl_chantype = RSL_CHAN_SDCCH8_ACCH;
		break;
	case GSMTAP_CHANNEL_BCCH: // BCCH
		*rsl_chantype = RSL_CHAN_BCCH;
		break;
	case GSMTAP_CHANNEL_RACH: // RACH
		*rsl_chantype = RSL_CHAN_RACH;
		break;
	case GSMTAP_CHANNEL_PCH: // PCH
	case GSMTAP_CHANNEL_AGCH: // AGCH
		*rsl_chantype = RSL_CHAN_PCH_AGCH;
		break;
	case GSMTAP_CHANNEL_PDCH:
		*rsl_chantype = RSL_CHAN_OSMO_PDCH;
		break;
	}

	*link_id = gsmtap_chantype & GSMTAP_CHANNEL_ACCH ? 0x40 : 0x00;
}

/*! create an arbitrary type GSMTAP message
 *  \param[in] type The GSMTAP_TYPE_xxx constant of the message to create
 *  \param[in] arfcn GSM ARFCN (Channel Number)
 *  \param[in] ts GSM time slot
 *  \param[in] chan_type Channel Type
 *  \param[in] ss Sub-slot
 *  \param[in] fn GSM Frame Number
 *  \param[in] signal_dbm Signal Strength (dBm)
 *  \param[in] snr Signal/Noise Ratio (SNR)
 *  \param[in] data Pointer to data buffer
 *  \param[in] len Length of \ref data
 *  \return dynamically allocated message buffer containing data
 *
 * This function will allocate a new msgb and fill it with a GSMTAP
 * header containing the information
 */
struct msgb *gsmtap_makemsg_ex(uint8_t type, uint16_t arfcn, uint8_t ts, uint8_t chan_type,
			    uint8_t ss, uint32_t fn, int8_t signal_dbm,
			    int8_t snr, const uint8_t *data, unsigned int len)
{
	struct msgb *msg;
	struct gsmtap_hdr *gh;
	uint8_t *dst;

	msg = msgb_alloc(sizeof(*gh) + len, "gsmtap_tx");
	if (!msg)
		return NULL;

	gh = (struct gsmtap_hdr *) msgb_put(msg, sizeof(*gh));

	gh->version = GSMTAP_VERSION;
	gh->hdr_len = sizeof(*gh)/4;
	gh->type = type;
	gh->timeslot = ts;
	gh->sub_slot = ss;
	gh->arfcn = osmo_htons(arfcn);
	gh->snr_db = snr;
	gh->signal_dbm = signal_dbm;
	gh->frame_number = osmo_htonl(fn);
	gh->sub_type = chan_type;
	gh->antenna_nr = 0;

	dst = msgb_put(msg, len);
	memcpy(dst, data, len);

	return msg;
}

/*! create L1/L2 data and put it into GSMTAP
 *  \param[in] arfcn GSM ARFCN (Channel Number)
 *  \param[in] ts GSM time slot
 *  \param[in] chan_type Channel Type
 *  \param[in] ss Sub-slot
 *  \param[in] fn GSM Frame Number
 *  \param[in] signal_dbm Signal Strength (dBm)
 *  \param[in] snr Signal/Noise Ratio (SNR)
 *  \param[in] data Pointer to data buffer
 *  \param[in] len Length of \ref data
 *  \return message buffer or NULL in case of error
 *
 * This function will allocate a new msgb and fill it with a GSMTAP
 * header containing the information
 */
struct msgb *gsmtap_makemsg(uint16_t arfcn, uint8_t ts, uint8_t chan_type,
			    uint8_t ss, uint32_t fn, int8_t signal_dbm,
			    int8_t snr, const uint8_t *data, unsigned int len)
{
	return gsmtap_makemsg_ex(GSMTAP_TYPE_UM, arfcn, ts, chan_type,
		ss, fn, signal_dbm, snr, data, len);
}

#ifdef HAVE_SYS_SOCKET_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <osmocom/core/osmo_io.h>

/*! Create a new (sending) GSMTAP source socket
 *  \param[in] host host name or IP address in string format
 *  \param[in] port UDP port number in host byte order
 *  \return file descriptor of the new socket
 *
 * Opens a GSMTAP source (sending) socket, connect it to host/port and
 * return resulting fd.  If \a host is NULL, the destination address
 * will be localhost.  If \a port is 0, the default \ref
 * GSMTAP_UDP_PORT will be used.
 * */
int gsmtap_source_init_fd(const char *host, uint16_t port)
{
	if (port == 0)
		port = GSMTAP_UDP_PORT;
	if (host == NULL)
		host = "localhost";

	return osmo_sock_init(AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, host, port,
				OSMO_SOCK_F_CONNECT);
}

/*! Create a new (sending) GSMTAP source socket
 *  \param[in] local_host local host name or IP address in string format
 *  \param[in] local_port local UDP port number in host byte order
 *  \param[in] rem_host remote host name or IP address in string format
 *  \param[in] rem_port remote UDP port number in host byte order
 *  \return file descriptor of the new socket
 *
 * Opens a GSMTAP source (sending) socket, connect it to remote host/port,
 * bind to local host/port and return resulting fd.
 * If \a local_host is NULL, the default address is used.
 * If \a local_port is 0, than random unused port will be selected by OS.
 * If \a rem_host is NULL, the destination address will be localhost.
 * If \a rem_port is 0, the default \ref GSMTAP_UDP_PORT will be used.
 */
int gsmtap_source_init_fd2(const char *local_host, uint16_t local_port, const char *rem_host, uint16_t rem_port)
{
	if (!local_host)
		return gsmtap_source_init_fd(rem_host, rem_port);

	return osmo_sock_init2(AF_UNSPEC, SOCK_DGRAM, IPPROTO_UDP, local_host, local_port,
			       rem_host ? rem_host : "localhost", rem_port ? rem_port : GSMTAP_UDP_PORT,
			       OSMO_SOCK_F_BIND | OSMO_SOCK_F_CONNECT);
}

/*! Add a local sink to an existing GSMTAP source and return fd
 *  \param[in] gsmtap_fd file descriptor of the gsmtap socket
 *  \returns file descriptor of locally bound receive socket
 *
 *  In case the GSMTAP socket is connected to a local destination
 *  IP/port, this function creates a corresponding receiving socket
 *  bound to that destination IP + port.
 *
 *  In case the gsmtap socket is not connected to a local IP/port, or
 *  creation of the receiving socket fails, a negative error code is
 *  returned.
 */
int gsmtap_source_add_sink_fd(int gsmtap_fd)
{
	struct sockaddr_storage ss;
	socklen_t ss_len = sizeof(ss);
	int rc;

	rc = getpeername(gsmtap_fd, (struct sockaddr *)&ss, &ss_len);
	if (rc < 0)
		return rc;

	if (osmo_sockaddr_is_local((struct sockaddr *)&ss, ss_len) == 1) {
		rc = osmo_sock_init_sa((struct sockaddr *)&ss, SOCK_DGRAM,
				       IPPROTO_UDP,
				       OSMO_SOCK_F_BIND |
				       OSMO_SOCK_F_UDP_REUSEADDR);
		if (rc >= 0)
			return rc;
	}

	return -ENODEV;
}

/*! Send a \ref msgb through a GSMTAP source
 *  \param[in] gti GSMTAP instance
 *  \param[in] msg message buffer
 *  \return 0 in case of success; negative in case of error
 * NOTE: in case of nonzero return value, the *caller* must free the msg!
 * (This enables the caller to attempt re-sending the message.)
 * If 0 is returned, the msgb was freed by this function.
 */
int gsmtap_sendmsg(struct gsmtap_inst *gti, struct msgb *msg)
{
	if (!gti)
		return -ENODEV;

	if (gti->osmo_io_mode)
		return osmo_iofd_write_msgb(gti->out, msg);
	else {
		/* try immediate send and return error if any */
		int rc;

		rc = write(gsmtap_inst_fd2(gti), msg->data, msg->len);
		if (rc < 0) {
			return rc;
		} else if (rc >= msg->len) {
			msgb_free(msg);
			return 0;
		} else {
			/* short write */
			return -EIO;
		}
	}
}

/*! Send a \ref msgb through a GSMTAP source; free the message even if tx queue full.
 *  \param[in] gti GSMTAP instance
 *  \param[in] msg message buffer; always freed, caller must not reference it later.
 *  \return 0 in case of success; negative in case of error
 */
int gsmtap_sendmsg_free(struct gsmtap_inst *gti, struct msgb *msg)
{
	int rc;
	rc = gsmtap_sendmsg(gti, msg);
	if (rc < 0)
		msgb_free(msg);
	return rc;
}

/*! send an arbitrary type through GSMTAP.
 *  See \ref gsmtap_makemsg_ex for arguments
 */
int gsmtap_send_ex(struct gsmtap_inst *gti, uint8_t type, uint16_t arfcn, uint8_t ts,
		uint8_t chan_type, uint8_t ss, uint32_t fn,
		int8_t signal_dbm, int8_t snr, const uint8_t *data,
		unsigned int len)
{
	struct msgb *msg;
	int rc;

	if (!gti)
		return -ENODEV;

	msg = gsmtap_makemsg_ex(type, arfcn, ts, chan_type, ss, fn, signal_dbm,
			     snr, data, len);
	if (!msg)
		return -ENOMEM;

	rc = gsmtap_sendmsg(gti, msg);
	if (rc)
		msgb_free(msg);
	return rc;
}

/*! send a message from L1/L2 through GSMTAP.
 *  See \ref gsmtap_makemsg for arguments
 */
int gsmtap_send(struct gsmtap_inst *gti, uint16_t arfcn, uint8_t ts,
		uint8_t chan_type, uint8_t ss, uint32_t fn,
		int8_t signal_dbm, int8_t snr, const uint8_t *data,
		unsigned int len)
{
	return gsmtap_send_ex(gti, GSMTAP_TYPE_UM, arfcn, ts, chan_type, ss, fn,
		signal_dbm, snr, data, len);
}

/*! Add a local sink to an existing GSMTAP source and return fd
 *  \param[in] gti existing GSMTAP source
 *  \returns file descriptor of locally bound receive socket
 *
 *  In case the GSMTAP socket is connected to a local destination
 *  IP/port, this function creates a corresponding receiving socket
 *  bound to that destination IP + port.
 *
 *  In case the gsmtap socket is not connected to a local IP/port, or
 *  creation of the receiving socket fails, a negative error code is
 *  returned.
 *
 *  The file descriptor of the receiving socket is automatically added
 *  to the libosmocore select() handling.
 */
int gsmtap_source_add_sink(struct gsmtap_inst *gti)
{
	return gti->sink_fd = gsmtap_source_add_sink_fd(gsmtap_inst_fd2(gti));
}

/* Registered in Osmo IO as a no-op to set the write callback. */
static void gsmtap_ops_noop_cb(struct osmo_io_fd *iofd, int res, struct msgb *msg)
{
}

static struct osmo_io_ops gsmtap_ops = { .write_cb = gsmtap_ops_noop_cb };

/*! Open GSMTAP source socket, connect and register osmo_fd
 *  \param[in] local_host IP address in string format
 *  \param[in] local_port UDP port number in host byte order
 *  \param[in] rem_host host name or IP address in string format
 *  \param[in] rem_port UDP port number in host byte order
 *  \param[in] ofd_wq_mode Register \ref osmo_wqueue (1) or not (0)
 *  \return callee-allocated \ref gsmtap_inst
 *
 * Open GSMTAP source (sending) socket, connect it to remote host/port,
 * bind it local host/port,
 * allocate 'struct gsmtap_inst' and optionally osmo_fd/osmo_wqueue
 * registration.
 */
struct gsmtap_inst *gsmtap_source_init2(const char *local_host, uint16_t local_port,
					const char *rem_host, uint16_t rem_port, int ofd_wq_mode)
{
	struct gsmtap_inst *gti;
	int fd;

	fd = gsmtap_source_init_fd2(local_host, local_port, rem_host, rem_port);
	if (fd < 0)
		return NULL;

	gti = talloc_zero(NULL, struct gsmtap_inst);
	gti->osmo_io_mode = ofd_wq_mode;
	/* Still using the wq member for its 'fd' field only, since we are keeping it for now, anyways  */
	gti->wq.bfd.fd = fd;
	gti->sink_fd = -1;

	if (ofd_wq_mode) {
		gti->out = osmo_iofd_setup(gti, gti->wq.bfd.fd, "gsmtap_inst.io_fd", OSMO_IO_FD_MODE_READ_WRITE, &gsmtap_ops, NULL);
		if (gti->out == NULL)
			goto err_cleanup;
		if (osmo_iofd_register(gti->out, gti->wq.bfd.fd) < 0)
			goto err_cleanup;

		/* osmo write queue previously used was set up with value of 64 */
		osmo_iofd_set_txqueue_max_length(gti->out, 64);
	}

	return gti;

err_cleanup:
	talloc_free(gti);
	close(fd);
	return NULL;
}

/*! Open GSMTAP source socket, connect and register osmo_fd
 *  \param[in] host host name or IP address in string format
 *  \param[in] port UDP port number in host byte order
 *  \param[in] ofd_wq_mode Register \ref osmo_wqueue (1) or not (0)
 *  \return callee-allocated \ref gsmtap_inst
 *
 * Open GSMTAP source (sending) socket, connect it to host/port,
 * allocate 'struct gsmtap_inst' and optionally osmo_fd/osmo_wqueue
 * registration.
 */
struct gsmtap_inst *gsmtap_source_init(const char *host, uint16_t port,
					int ofd_wq_mode)
{
	return gsmtap_source_init2(NULL, 0, host, port, ofd_wq_mode);
}

void gsmtap_source_free(struct gsmtap_inst *gti)
{
	if (!gti)
		return;

	if (gti->osmo_io_mode) {
		osmo_iofd_free(gti->out);

		if (gti->sink_fd != -1) {
			close(gti->sink_fd);
			gti->sink_fd = -1;
		}

	}

	talloc_free(gti);
}

#endif /* HAVE_SYS_SOCKET_H */

const struct value_string gsmtap_gsm_channel_names[] = {
	{ GSMTAP_CHANNEL_UNKNOWN,	"UNKNOWN" },
	{ GSMTAP_CHANNEL_BCCH,		"BCCH" },
	{ GSMTAP_CHANNEL_CCCH,		"CCCH" },
	{ GSMTAP_CHANNEL_RACH,		"RACH" },
	{ GSMTAP_CHANNEL_AGCH,		"AGCH" },
	{ GSMTAP_CHANNEL_PCH,		"PCH" },
	{ GSMTAP_CHANNEL_SDCCH,		"SDCCH" },
	{ GSMTAP_CHANNEL_SDCCH4,	"SDCCH/4" },
	{ GSMTAP_CHANNEL_SDCCH8,	"SDCCH/8" },
	{ GSMTAP_CHANNEL_FACCH_F,	"FACCH/F" },
	{ GSMTAP_CHANNEL_FACCH_H,	"FACCH/H" },
	{ GSMTAP_CHANNEL_PACCH,		"PACCH" },
	{ GSMTAP_CHANNEL_CBCH52,	"CBCH" },
	{ GSMTAP_CHANNEL_PDCH,		"PDCH" } ,
	{ GSMTAP_CHANNEL_PTCCH,		"PTTCH" },
	{ GSMTAP_CHANNEL_CBCH51,	"CBCH" },
	{ GSMTAP_CHANNEL_ACCH | GSMTAP_CHANNEL_SDCCH, "LSACCH" },
	{ GSMTAP_CHANNEL_ACCH | GSMTAP_CHANNEL_SDCCH4, "SACCH/4" },
	{ GSMTAP_CHANNEL_ACCH | GSMTAP_CHANNEL_SDCCH8, "SACCH/8" },
	{ GSMTAP_CHANNEL_ACCH | GSMTAP_CHANNEL_FACCH_F, "SACCH/F" },
	{ GSMTAP_CHANNEL_ACCH | GSMTAP_CHANNEL_FACCH_H, "SACCH/H" },
	{ GSMTAP_CHANNEL_VOICE_F,	"TCH/F" },
	{ GSMTAP_CHANNEL_VOICE_H,	"TCH/H" },
	{ 0, NULL }
};

/* for debugging */
const struct value_string gsmtap_type_names[] = {
	{ GSMTAP_TYPE_UM,		"GSM Um (MS<->BTS)" },
	{ GSMTAP_TYPE_ABIS,		"GSM Abis (BTS<->BSC)" },
	{ GSMTAP_TYPE_UM_BURST,		"GSM Um burst (MS<->BTS)" },
	{ GSMTAP_TYPE_SIM,		"SIM Card" },
	{ GSMTAP_TYPE_TETRA_I1,		"TETRA V+D"  },
	{ GSMTAP_TYPE_TETRA_I1_BURST,	"TETRA bursts" },
	{ GSMTAP_TYPE_WMX_BURST,	"WiMAX burst" },
	{ GSMTAP_TYPE_GB_LLC,		"GPRS Gb LLC" },
	{ GSMTAP_TYPE_GB_SNDCP,		"GPRS Gb SNDCP" },
	{ GSMTAP_TYPE_GMR1_UM,		"GMR-1 air interfeace (MES-MS<->GTS)"},
	{ GSMTAP_TYPE_UMTS_RLC_MAC,	"UMTS RLC/MAC" },
	{ GSMTAP_TYPE_UMTS_RRC,		"UMTS RRC" },
	{ GSMTAP_TYPE_LTE_RRC,		"LTE RRC" },
	{ GSMTAP_TYPE_LTE_MAC,		"LTE MAC" },
	{ GSMTAP_TYPE_LTE_MAC_FRAMED,	"LTE MAC with context hdr" },
	{ GSMTAP_TYPE_OSMOCORE_LOG,	"libosmocore logging" },
	{ GSMTAP_TYPE_QC_DIAG,		"Qualcomm DIAG" },
	{ GSMTAP_TYPE_LTE_NAS,		"LTE Non-Access Stratum" },
	{ GSMTAP_TYPE_E1T1,		"E1/T1 lines" },
	{ GSMTAP_TYPE_GSM_RLP,		"GSM Radio Link Protocol" },
	{ 0, NULL }
};

/*! @} */
