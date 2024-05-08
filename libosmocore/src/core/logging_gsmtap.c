/*! \file logging_gsmtap.c
 *  libosmocore log output encapsulated in GSMTAP.
 *
 *  Encapsulating the log output inside GSMTAP frames allows us to
 *  observer protocol traces (of Um, Abis, A or any other interface in
 *  the Osmocom world) with synchronous interspersed log messages.
 */
/*
 * (C) 2016 by Harald Welte <laforge@gnumonks.org>
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

/*! \addtogroup logging
 *  @{
 * \file logging_gsmtap.c */

#include "osmo_config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/gsmtap.h>
#include <osmocom/core/gsmtap_util.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/byteswap.h>
#include <osmocom/core/thread.h>

#define	GSMTAP_LOG_MAX_SIZE 4096

static __thread uint32_t logging_gsmtap_tid;

static void _gsmtap_raw_output(struct log_target *target, int subsys,
			       unsigned int level, const char *file,
			       int line, int cont, const char *format,
			       va_list ap)
{
	struct msgb *msg;
	struct gsmtap_hdr *gh;
	struct gsmtap_osmocore_log_hdr *golh;
	const char *subsys_name = log_category_name(subsys);
	struct timeval tv;
	int rc;
	const char *file_basename;
	unsigned int _level;

	/* get timestamp ASAP */
	osmo_gettimeofday(&tv, NULL);

	msg = msgb_alloc(sizeof(*gh)+sizeof(*golh)+GSMTAP_LOG_MAX_SIZE,
			 "GSMTAP logging");

	/* GSMTAP header */
	gh = (struct gsmtap_hdr *) msgb_put(msg, sizeof(*gh));
	memset(gh, 0, sizeof(*gh));
	gh->version = GSMTAP_VERSION;
	gh->hdr_len = sizeof(*gh)/4;
	gh->type = GSMTAP_TYPE_OSMOCORE_LOG;

	/* Logging header */
	golh = (struct gsmtap_osmocore_log_hdr *) msgb_put(msg, sizeof(*golh));
	OSMO_STRLCPY_ARRAY(golh->proc_name, target->tgt_gsmtap.ident);
	if (logging_gsmtap_tid == 0)
		osmo_store32be((uint32_t)osmo_gettid(), &logging_gsmtap_tid);
	golh->pid = logging_gsmtap_tid;
	if (subsys_name)
		OSMO_STRLCPY_ARRAY(golh->subsys, subsys_name + 1);
	else
		golh->subsys[0] = '\0';

	/* strip all leading path elements from file, if any. */
	file_basename = strrchr(file, '/');
	file = (file_basename && file_basename[1])? file_basename + 1 : file;
	OSMO_STRLCPY_ARRAY(golh->src_file.name, file);
	golh->src_file.line_nr = osmo_htonl(line);
	golh->level = level;
	/* we always store the timestamp in the message, irrespective
	 * of hat prrint_[ext_]timestamp say */
	golh->ts.sec = osmo_htonl(tv.tv_sec);
	golh->ts.usec = osmo_htonl(tv.tv_usec);

	rc = vsnprintf((char *) msg->tail, msgb_tailroom(msg), format, ap);
	if (rc < 0) {
		msgb_free(msg);
		return;
	} else if (rc >= msgb_tailroom(msg)) {
		/* If the output was truncated, vsnprintf() returns the
		 * number of characters which would have been written
		 * if enough space had been available (excluding '\0'). */
		rc = msgb_tailroom(msg);
		msg->tail[rc - 1]  = '\0';
	}
	msgb_put(msg, rc);

	/* Ensure that any error occurring when sending the log message doesn't cause infinite recursion */
	_level = target->loglevel;
	target->loglevel = UINT8_MAX;
	rc = gsmtap_sendmsg(target->tgt_gsmtap.gsmtap_inst, msg);
	target->loglevel = _level;
	if (rc)
		msgb_free(msg);
}

/*! Create a new logging target for GSMTAP logging
 *  \param[in] host remote host to send the logs to
 *  \param[in] port remote port to send the logs to
 *  \param[in] ident string identifier
 *  \param[in] ofd_wq_mode register osmo_wqueue (1) or not (0)
 *  \param[in] add_sink add GSMTAP sink or not
 *  \returns Log target in case of success, NULL in case of error
 */
struct log_target *log_target_create_gsmtap(const char *host, uint16_t port,
					    const char *ident,
					    bool ofd_wq_mode,
					    bool add_sink)
{
	struct log_target *target;
	struct gsmtap_inst *gti;

	target = log_target_create();
	if (!target)
		return NULL;

	gti = gsmtap_source_init(host, port, ofd_wq_mode);
	if (!gti) {
		log_target_destroy(target);
		return NULL;
	}

	if (add_sink)
		gsmtap_source_add_sink(gti);

	target->tgt_gsmtap.gsmtap_inst = gti;
	target->tgt_gsmtap.ident = talloc_strdup(target, ident);
	target->tgt_gsmtap.hostname = talloc_strdup(target, host);

	target->type = LOG_TGT_TYPE_GSMTAP;
	target->raw_output = _gsmtap_raw_output;

	return target;
}

/* @} */
