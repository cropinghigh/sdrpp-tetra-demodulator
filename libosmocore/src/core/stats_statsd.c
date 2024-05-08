/*
 * (C) 2015 by sysmocom - s.f.m.c. GmbH
 * Author: Jacob Erlbeck <jerlbeck@sysmocom.de>
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

/*! \addtogroup stats
 *  @{
 *  \file stats_statsd.c */

#include "osmo_config.h"
#if !defined(EMBEDDED)

#include <osmocom/core/stats.h>

#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/rate_ctr.h>
#include <osmocom/core/stat_item.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/stats.h>

static int osmo_stats_reporter_statsd_send_counter(struct osmo_stats_reporter *srep,
	const struct rate_ctr_group *ctrg,
	const struct rate_ctr_desc *desc,
	int64_t value, int64_t delta);
static int osmo_stats_reporter_statsd_send_item(struct osmo_stats_reporter *srep,
	const struct osmo_stat_item_group *statg,
	const struct osmo_stat_item_desc *desc, int64_t value);

/*! Create a stats_reporter reporting to statsd.  This creates a stats_reporter
 *  instance which reports the related statistics data to statsd.
 *  \param[in] name Name of the to-be-created stats_reporter
 *  \returns stats_reporter on success; NULL on error */
struct osmo_stats_reporter *osmo_stats_reporter_create_statsd(const char *name)
{
	struct osmo_stats_reporter *srep;
	srep = osmo_stats_reporter_alloc(OSMO_STATS_REPORTER_STATSD, name);
	if (!srep)
		return NULL;

	srep->have_net_config = 1;

	srep->open = osmo_stats_reporter_udp_open;
	srep->close = osmo_stats_reporter_udp_close;
	srep->send_counter = osmo_stats_reporter_statsd_send_counter;
	srep->send_item = osmo_stats_reporter_statsd_send_item;

	return srep;
}

/*! Replace all illegal ':' in the stats name, but not when used as value seperator.
 *  ':' is used as seperator between the name and the value in the statsd protocol.
 *  \param[inout] buf is a null terminated string containing name, value, unit. */
static void osmo_stats_reporter_sanitize_name(char *buf)
{
	/* e.g. msc.loc_update_type:normal:1|c -> msc.loc_update_type.normal:1|c
	 * last is the seperator between name and value */
	char *last = strrchr(buf, ':');
	char *tmp = strchr(buf, ':');

	if (!last)
		return;

	while (tmp < last) {
		*tmp = '.';
		tmp = strchr(buf, ':');
	}
}

static int osmo_stats_reporter_statsd_send(struct osmo_stats_reporter *srep,
	const char *name1, const char *index1, const char *name2, int64_t value,
	const char *unit)
{
	char *buf;
	int buf_size;
	int nchars, rc = 0;
	char *fmt = NULL;
	char *prefix = srep->name_prefix;
	int old_len = msgb_length(srep->buffer);

	if (prefix) {
		if (name1)
			fmt = "%1$s.%2$s.%6$s.%3$s:%4$" PRId64 "|%5$s";
		else
			fmt = "%1$s.%2$0.0s%3$s:%4$" PRId64 "|%5$s";
	} else {
		prefix = "";
		if (name1)
			fmt = "%1$s%2$s.%6$s.%3$s:%4$" PRId64 "|%5$s";
		else
			fmt = "%1$s%2$0.0s%3$s:%4$" PRId64 "|%5$s";
	}

	if (srep->agg_enabled) {
		if (msgb_length(srep->buffer) > 0 &&
			msgb_tailroom(srep->buffer) > 0)
		{
			msgb_put_u8(srep->buffer, '\n');
		}
	}

	buf = (char *)msgb_put(srep->buffer, 0);
	buf_size = msgb_tailroom(srep->buffer);

	nchars = snprintf(buf, buf_size, fmt,
		prefix, name1, name2,
		value, unit, index1);

	if (nchars >= buf_size) {
		/* Truncated */
		/* Restore original buffer (without trailing LF) */
		msgb_trim(srep->buffer, old_len);
		/* Send it */
		rc = osmo_stats_reporter_send_buffer(srep);

		/* Try again */
		buf = (char *)msgb_put(srep->buffer, 0);
		buf_size = msgb_tailroom(srep->buffer);

		nchars = snprintf(buf, buf_size, fmt,
			prefix, name1, name2,
			value, unit, index1);

		if (nchars >= buf_size)
			return -EMSGSIZE;
	}

	if (nchars > 0) {
		osmo_stats_reporter_sanitize_name(buf);
		msgb_trim(srep->buffer, msgb_length(srep->buffer) + nchars);
	}

	if (!srep->agg_enabled)
		rc = osmo_stats_reporter_send_buffer(srep);

	return rc;
}

static int osmo_stats_reporter_statsd_send_counter(struct osmo_stats_reporter *srep,
	const struct rate_ctr_group *ctrg,
	const struct rate_ctr_desc *desc,
	int64_t value, int64_t delta)
{
	char buf_idx[64];
	const char *idx_name = buf_idx;
	const char *prefix;

	if (ctrg) {
		prefix = ctrg->desc->group_name_prefix;
		if (ctrg->name)
			idx_name = ctrg->name;
		else
			snprintf(buf_idx, sizeof(buf_idx), "%u", ctrg->idx);
	} else {
		prefix = NULL;
		buf_idx[0] = '0';
		buf_idx[1] = '\n';
	}
	return osmo_stats_reporter_statsd_send(srep, prefix, idx_name, desc->name, delta, "c");
}

static int osmo_stats_reporter_statsd_send_item(struct osmo_stats_reporter *srep,
	const struct osmo_stat_item_group *statg,
	const struct osmo_stat_item_desc *desc, int64_t value)
{
	char buf_idx[64];
	char *idx_name;
	if (statg->name)
		idx_name = statg->name;
	else {
		snprintf(buf_idx, sizeof(buf_idx), "%u", statg->idx);
		idx_name = buf_idx;
	}

	if (value < 0)
		value = 0;

	return osmo_stats_reporter_statsd_send(srep, statg->desc->group_name_prefix,
					       idx_name, desc->name, value, "g");
}
#endif /* !EMBEDDED */

/* @} */
