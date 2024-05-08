/*! \file stats.c */
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
 *
 * This module implements periodic reporting of statistics / counters.
 * It supports the notion of multiple \ref osmo_stats_reporter objects
 * which independently of each other can report statistics at different
 * configurable intervals to different destinations.
 *
 * In order to use this facility, you have to call \ref
 * osmo_stats_init() once at application start-up and then create one or
 * more \ref osmo_stats_reporter, either using the direct API functions
 * or by using the optional VTY bindings:
 *
 * - reporting to any of the libosmocore log targets
 *   \ref osmo_stats_reporter_create_log() creates a new stats_reporter
 *   which reports to the libosmcoore \ref logging subsystem.
 *
 * - reporting to statsd (a front-end proxy for the Graphite/Carbon
 *   metrics server
 *   \ref osmo_stats_reporter_create_statsd() creates a new stats_reporter
 *   which reports via UDP to statsd.
 *
 * You can either use the above API functions directly to create \ref
 * osmo_stats_reporter instances, or you can use the VTY support
 * contained in libosmovty.  See the "stats" configuration node
 * installed by osmo_stats_vty_Add_cmds().
 *
 * An \ref osmo_stats_reporter reports statistics on all of the following
 * libosmocore internal counter/statistics objects:
 * - \ref osmo_counter
 * - \ref rate_ctr
 * - \ref osmo_stat_item
 *
 * You do not need to do anything in particular to expose a given
 * counter or stat_item, they are all exported automatically via any
 * \ref osmo_stats_reporter.  If you have multiple \ref
 * osmo_stats_reporter, they will each report all counters/stat_items.
 *
 * \file stats.c */

#include "osmo_config.h"
#if !defined(EMBEDDED)

#include <osmocom/core/byteswap.h>
#include <osmocom/core/stats.h>

#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/rate_ctr.h>
#include <osmocom/core/stat_item.h>
#include <osmocom/core/select.h>
#include <osmocom/core/counter.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/stats_tcp.h>

#ifdef HAVE_SYSTEMTAP
/* include the generated probes header and put markers in code */
#include "probes.h"
#define TRACE(probe) probe
#define TRACE_ENABLED(probe) probe ## _ENABLED()
#else
/* Wrap the probe to allow it to be removed when no systemtap available */
#define TRACE(probe)
#define TRACE_ENABLED(probe) (0)
#endif /* HAVE_SYSTEMTAP */

#include <stat_item_internal.h>

#define STATS_DEFAULT_INTERVAL 5 /* secs */
#define STATS_DEFAULT_BUFLEN 256

LLIST_HEAD(osmo_stats_reporter_list);
static void *osmo_stats_ctx = NULL;
static int is_initialised = 0;

static struct osmo_stats_config s_stats_config = {
	.interval = STATS_DEFAULT_INTERVAL,
};
struct osmo_stats_config *osmo_stats_config = &s_stats_config;

static struct osmo_fd osmo_stats_timer = { .fd = -1 };

static int osmo_stats_reporter_log_send_counter(struct osmo_stats_reporter *srep,
	const struct rate_ctr_group *ctrg,
	const struct rate_ctr_desc *desc,
	int64_t value, int64_t delta);
static int osmo_stats_reporter_log_send_item(struct osmo_stats_reporter *srep,
	const struct osmo_stat_item_group *statg,
	const struct osmo_stat_item_desc *desc, int64_t value);

static int update_srep_config(struct osmo_stats_reporter *srep)
{
	int rc = 0;

	if (srep->running) {
		if (srep->close)
			rc = srep->close(srep);
		srep->running = 0;
	}

	if (!srep->enabled)
		return rc;

	if (srep->open)
		rc = srep->open(srep);
	else
		rc = 0;

	if (rc < 0)
		srep->enabled = 0;
	else
		srep->running = 1;

	srep->force_single_flush = 1;

	return rc;
}

static int osmo_stats_timer_cb(struct osmo_fd *ofd, unsigned int what)
{
	uint64_t expire_count;
	int rc;

	/* check that the timer has actually expired */
	if (!(what & OSMO_FD_READ))
		return 0;

	/* read from timerfd: number of expirations of periodic timer */
	rc = read(ofd->fd, (void *) &expire_count, sizeof(expire_count));
	if (rc < 0 && errno == EAGAIN)
		return 0;
	OSMO_ASSERT(rc == sizeof(expire_count));

	if (expire_count > 1)
		LOGP(DLSTATS, LOGL_NOTICE, "Stats timer expire_count=%" PRIu64 ": We missed %" PRIu64 " timers\n",
			expire_count, expire_count-1);

	if (!llist_empty(&osmo_stats_reporter_list))
		osmo_stats_report();

	return 0;
}

static int start_timer(void)
{
	int rc;
	int interval = osmo_stats_config->interval;

	if (!is_initialised)
		return -ESRCH;

	struct timespec ts_first = {.tv_sec=0, .tv_nsec=1000};
	struct timespec ts_interval = {.tv_sec=interval, .tv_nsec=0};

	rc = osmo_timerfd_setup(&osmo_stats_timer, osmo_stats_timer_cb, NULL);
	if (rc < 0)
		LOGP(DLSTATS, LOGL_ERROR, "Failed to setup the timer with error code %d (fd=%d)\n",
		     rc, osmo_stats_timer.fd);

        if (interval == 0) {
		rc = osmo_timerfd_disable(&osmo_stats_timer);
		if (rc < 0)
			LOGP(DLSTATS, LOGL_ERROR, "Failed to disable the timer with error code %d (fd=%d)\n",
			     rc, osmo_stats_timer.fd);
        } else {

		rc = osmo_timerfd_schedule(&osmo_stats_timer, &ts_first, &ts_interval);
		if (rc < 0)
			LOGP(DLSTATS, LOGL_ERROR, "Failed to schedule the timer with error code %d (fd=%d, interval %d sec)\n",
			rc, osmo_stats_timer.fd, interval);

		LOGP(DLSTATS, LOGL_INFO, "Stats timer started with interval %d sec\n", interval);
	}

	return 0;
}

struct osmo_stats_reporter *osmo_stats_reporter_alloc(enum osmo_stats_reporter_type type,
	const char *name)
{
	struct osmo_stats_reporter *srep;
	srep = talloc_zero(osmo_stats_ctx, struct osmo_stats_reporter);
	if (!srep)
		return NULL;

	srep->type = type;
	if (name)
		srep->name = talloc_strdup(srep, name);
	srep->fd = -1;

	llist_add_tail(&srep->list, &osmo_stats_reporter_list);

	return srep;
}

/*! Destroy a given stats_reporter. Takes care of first disabling it.
 *  \param[in] srep stats_reporter that shall be disabled + destroyed */
void osmo_stats_reporter_free(struct osmo_stats_reporter *srep)
{
	osmo_stats_reporter_disable(srep);
	llist_del(&srep->list);
	talloc_free(srep);
}

/*! Initialize the stats reporting module; call this once in your program.
 *  \param[in] ctx Talloc context from which stats related memory is allocated */
void osmo_stats_init(void *ctx)
{
	osmo_stats_ctx = ctx;
	is_initialised = 1;
	start_timer();

	/* Make sure that the tcp-stats interval timer also runs at its
	 * preconfigured rate. The vty might change this setting later. */
	osmo_stats_tcp_set_interval(osmo_tcp_stats_config->interval);
}

/*! Find a stats_reporter of given \a type and \a name.
 *  \param[in] type Type of stats_reporter to find
 *  \param[in] name Name of stats_reporter to find
 *  \returns stats_reporter matching \a type and \a name; NULL otherwise */
struct osmo_stats_reporter *osmo_stats_reporter_find(enum osmo_stats_reporter_type type,
	const char *name)
{
	struct osmo_stats_reporter *srep;
	llist_for_each_entry(srep, &osmo_stats_reporter_list, list) {
		if (srep->type != type)
			continue;
		if (srep->name != name) {
			if (name == NULL || srep->name == NULL ||
				strcmp(name, srep->name) != 0)
				continue;
		}
		return srep;
	}
	return NULL;
}

#ifdef HAVE_SYS_SOCKET_H

/*! Set the remote (IP) address of a given stats_reporter.
 *  \param[in] srep stats_reporter whose remote address is to be set
 *  \param[in] addr String representation of remote IPv4 address
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_set_remote_addr(struct osmo_stats_reporter *srep, const char *addr)
{
	int rc;
	struct sockaddr_in *sock_addr = (struct sockaddr_in *)&srep->dest_addr;
	struct in_addr inaddr;

	if (!srep->have_net_config)
		return -ENOTSUP;

	OSMO_ASSERT(addr != NULL);

	rc = inet_pton(AF_INET, addr, &inaddr);
	if (rc <= 0)
		return -EINVAL;

	sock_addr->sin_addr = inaddr;
	sock_addr->sin_family = AF_INET;
	srep->dest_addr_len = sizeof(*sock_addr);

	talloc_free(srep->dest_addr_str);
	srep->dest_addr_str = talloc_strdup(srep, addr);

	return update_srep_config(srep);
}

/*! Set the remote (UDP) port of a given stats_reporter
 *  \param[in] srep stats_reporter whose remote address is to be set
 *  \param[in] port UDP port of remote statsd to which we report
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_set_remote_port(struct osmo_stats_reporter *srep, int port)
{
	struct sockaddr_in *sock_addr = (struct sockaddr_in *)&srep->dest_addr;

	if (!srep->have_net_config)
		return -ENOTSUP;

	srep->dest_port = port;
	sock_addr->sin_port = osmo_htons(port);

	return update_srep_config(srep);
}

/*! Set the local (IP) address of a given stats_reporter.
 *  \param[in] srep stats_reporter whose remote address is to be set
 *  \param[in] addr String representation of local IP address
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_set_local_addr(struct osmo_stats_reporter *srep, const char *addr)
{
	int rc;
	struct sockaddr_in *sock_addr = (struct sockaddr_in *)&srep->bind_addr;
	struct in_addr inaddr;

	if (!srep->have_net_config)
		return -ENOTSUP;

	if (addr) {
		rc = inet_pton(AF_INET, addr, &inaddr);
		if (rc <= 0)
			return -EINVAL;
	} else {
		inaddr.s_addr = INADDR_ANY;
	}

	sock_addr->sin_addr = inaddr;
	sock_addr->sin_family = AF_INET;
	srep->bind_addr_len = addr ? sizeof(*sock_addr) : 0;

	talloc_free(srep->bind_addr_str);
	srep->bind_addr_str = addr ? talloc_strdup(srep, addr) : NULL;

	return update_srep_config(srep);
}

/*! Set the maximum transmission unit of a given stats_reporter.
 *  \param[in] srep stats_reporter whose remote address is to be set
 *  \param[in] mtu Maximum Transmission Unit of \a srep
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_set_mtu(struct osmo_stats_reporter *srep, int mtu)
{
	if (!srep->have_net_config)
		return -ENOTSUP;

	if (mtu < 0)
		return -EINVAL;

	srep->mtu = mtu;

	return update_srep_config(srep);
}
#endif /* HAVE_SYS_SOCKETS_H */

int osmo_stats_reporter_set_max_class(struct osmo_stats_reporter *srep,
	enum osmo_stats_class class_id)
{
	if (class_id == OSMO_STATS_CLASS_UNKNOWN)
		return -EINVAL;

	srep->max_class = class_id;

	return 0;
}

/*! Set the reporting interval (common for all reporters)
 *  \param[in] interval Reporting interval in seconds
 *  \returns 0 on success; negative on error */
int osmo_stats_set_interval(int interval)
{
	if (interval < 0)
		return -EINVAL;

	osmo_stats_config->interval = interval;
	if (is_initialised)
		start_timer();

	return 0;
}

/*! Set the regular flush period for a given stats_reporter
 *
 * Send all stats even if they have not changed (i.e. force the flush)
 * every N-th reporting interval. Set to 0 to disable regular flush,
 * set to 1 to flush every time, set to 2 to flush every 2nd time, etc.
 *  \param[in] srep stats_reporter to set flush period for
 *  \param[in] period Reporting interval in seconds
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_set_flush_period(struct osmo_stats_reporter *srep, unsigned int period)
{
	srep->flush_period = period;
	srep->flush_period_counter = 0;
	/* force the flush now if it's not disabled by period=0 */
	if (period > 0)
		srep->force_single_flush = 1;

	return 0;
}

/*! Set the name prefix of a given stats_reporter.
 *  \param[in] srep stats_reporter whose name prefix is to be set
 *  \param[in] prefix Name prefix to pre-pend for any reported value
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_set_name_prefix(struct osmo_stats_reporter *srep, const char *prefix)
{
	talloc_free(srep->name_prefix);
	srep->name_prefix = prefix && strlen(prefix) > 0 ?
		talloc_strdup(srep, prefix) : NULL;

	return update_srep_config(srep);
}


/*! Enable the given stats_reporter.
 *  \param[in] srep stats_reporter who is to be enabled
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_enable(struct osmo_stats_reporter *srep)
{
	srep->enabled = 1;

	return update_srep_config(srep);
}

/*! Disable the given stats_reporter.
 *  \param[in] srep stats_reporter who is to be disabled
 *  \returns 0 on success; negative on error */
int osmo_stats_reporter_disable(struct osmo_stats_reporter *srep)
{
	srep->enabled = 0;

	return update_srep_config(srep);
}

/*** i/o helper functions ***/

#ifdef HAVE_SYS_SOCKET_H

/*! Open the UDP socket for given stats_reporter.
 *  \param[in] srep stats_reporter whose UDP socket is to be opened
 *  ]returns 0 on success; negative otherwise */
int osmo_stats_reporter_udp_open(struct osmo_stats_reporter *srep)
{
	int sock;
	int rc;
	int buffer_size = STATS_DEFAULT_BUFLEN;

	if (srep->fd != -1 && srep->close)
		 srep->close(srep);

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == -1)
		return -errno;

#if defined(__APPLE__) && !defined(MSG_NOSIGNAL)
	{
		static int val = 1;

		rc = setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, (void*)&val, sizeof(val));
		goto failed;
	}
#endif
	if (srep->bind_addr_len > 0) {
		rc = bind(sock, &srep->bind_addr, srep->bind_addr_len);
		if (rc == -1)
			goto failed;
	}

	srep->fd = sock;

	if (srep->mtu > 0) {
		buffer_size = srep->mtu - 20 /* IP */ - 8 /* UDP */;
		srep->agg_enabled = 1;
	}

	srep->buffer = msgb_alloc(buffer_size, "stats buffer");
	if (!srep->buffer)
		goto failed;

	return 0;

failed:
	rc = -errno;
	close(sock);

	return rc;
}

/*! Closee the UDP socket for given stats_reporter.
 *  \param[in] srep stats_reporter whose UDP socket is to be closed
 *  ]returns 0 on success; negative otherwise */
int osmo_stats_reporter_udp_close(struct osmo_stats_reporter *srep)
{
	int rc;
	if (srep->fd == -1)
		return -EBADF;

	osmo_stats_reporter_send_buffer(srep);

	rc = close(srep->fd);
	srep->fd = -1;
	msgb_free(srep->buffer);
	srep->buffer = NULL;
	return rc == -1 ? -errno : 0;
}

/*! Send given date to given stats_reporter.
 *  \param[in] srep stats_reporter whose UDP socket is to be opened
 *  \param[in] data string data to be sent
 *  \param[in] data_len Length of \a data in bytes
 *  \returns number of bytes on success; negative otherwise */
int osmo_stats_reporter_send(struct osmo_stats_reporter *srep, const char *data,
	int data_len)
{
	int rc;

	rc = sendto(srep->fd, data, data_len,
#ifdef MSG_NOSIGNAL
		MSG_NOSIGNAL |
#endif
		MSG_DONTWAIT,
		&srep->dest_addr, srep->dest_addr_len);

	if (rc == -1)
		rc = -errno;

	return rc;
}

/*! Send current accumulated buffer to given stats_reporter.
 *  \param[in] srep stats_reporter whose UDP socket is to be opened
 *  \returns number of bytes on success; negative otherwise */
int osmo_stats_reporter_send_buffer(struct osmo_stats_reporter *srep)
{
	int rc;

	if (!srep->buffer || msgb_length(srep->buffer) == 0)
		return 0;

	rc = osmo_stats_reporter_send(srep,
		(const char *)msgb_data(srep->buffer), msgb_length(srep->buffer));

	msgb_trim(srep->buffer, 0);

	return rc;
}
#endif /* HAVE_SYS_SOCKET_H */

/*** log reporter ***/

/*! Create a stats_reporter that logs via libosmocore logging.
 *  A stats_reporter created via this function will simply print the statistics
 *  via the libosmocore logging framework, using DLSTATS subsystem and LOGL_INFO
 *  priority.  The configuration of the libosmocore log targets define where this
 *  information will end up (ignored, text file, stderr, syslog, ...).
 *  \param[in] name Name of the to-be-created stats_reporter
 *  \returns stats_reporter on success; NULL on error */
struct osmo_stats_reporter *osmo_stats_reporter_create_log(const char *name)
{
	struct osmo_stats_reporter *srep;
	srep = osmo_stats_reporter_alloc(OSMO_STATS_REPORTER_LOG, name);
	if (!srep)
		return NULL;

	srep->have_net_config = 0;

	srep->send_counter = osmo_stats_reporter_log_send_counter;
	srep->send_item = osmo_stats_reporter_log_send_item;

	return srep;
}

static int osmo_stats_reporter_log_send(struct osmo_stats_reporter *srep,
	const char *type,
	const char *name1, unsigned int index1, const char *name2, int value,
	const char *unit)
{
	LOGP(DLSTATS, LOGL_INFO,
		"stats t=%s p=%s g=%s i=%u n=%s v=%d u=%s\n",
		type, srep->name_prefix ? srep->name_prefix : "",
		name1 ? name1 : "", index1,
		name2, value, unit ? unit : "");

	return 0;
}


static int osmo_stats_reporter_log_send_counter(struct osmo_stats_reporter *srep,
	const struct rate_ctr_group *ctrg,
	const struct rate_ctr_desc *desc,
	int64_t value, int64_t delta)
{
	if (ctrg)
		return osmo_stats_reporter_log_send(srep, "c",
			ctrg->desc->group_name_prefix,
			ctrg->idx,
			desc->name, value, NULL);
	else
		return osmo_stats_reporter_log_send(srep, "c",
			NULL, 0,
			desc->name, value, NULL);
}

static int osmo_stats_reporter_log_send_item(struct osmo_stats_reporter *srep,
	const struct osmo_stat_item_group *statg,
	const struct osmo_stat_item_desc *desc, int64_t value)
{
	return osmo_stats_reporter_log_send(srep, "i",
		statg->desc->group_name_prefix, statg->idx,
		desc->name, value, desc->unit);
}

/*** helper for reporting ***/

static int osmo_stats_reporter_check_config(struct osmo_stats_reporter *srep,
	unsigned int index, int class_id)
{
	if (class_id == OSMO_STATS_CLASS_UNKNOWN)
		class_id = index != 0 ?
			OSMO_STATS_CLASS_SUBSCRIBER : OSMO_STATS_CLASS_GLOBAL;

	return class_id <= srep->max_class;
}

/*** generic rate counter support ***/

static int osmo_stats_reporter_send_counter(struct osmo_stats_reporter *srep,
	const struct rate_ctr_group *ctrg,
	const struct rate_ctr_desc *desc,
	int64_t value, int64_t delta)
{
	if (!srep->send_counter)
		return 0;

	return srep->send_counter(srep, ctrg, desc, value, delta);
}

static int rate_ctr_handler(
	struct rate_ctr_group *ctrg, struct rate_ctr *ctr,
	const struct rate_ctr_desc *desc, void *sctx_)
{
	struct osmo_stats_reporter *srep;
	int64_t delta = rate_ctr_difference(ctr);

	llist_for_each_entry(srep, &osmo_stats_reporter_list, list) {
		if (!srep->running)
			continue;

		if (delta == 0 && !srep->force_single_flush)
			continue;

		if (!osmo_stats_reporter_check_config(srep,
			       ctrg->idx, ctrg->desc->class_id))
			continue;

		osmo_stats_reporter_send_counter(srep, ctrg, desc,
			ctr->current, delta);

		/* TODO: handle result (log?, inc counter(!)?) or remove it */
	}

	return 0;
}

static int rate_ctr_group_handler(struct rate_ctr_group *ctrg, void *sctx_)
{
	rate_ctr_for_each_counter(ctrg, rate_ctr_handler, sctx_);

	return 0;
}

/*** stat item support ***/

static int osmo_stats_reporter_send_item(struct osmo_stats_reporter *srep,
	const struct osmo_stat_item_group *statg,
	const struct osmo_stat_item_desc *desc,
	int32_t value)
{
	if (!srep->send_item)
		return 0;

	return srep->send_item(srep, statg, desc, value);
}

static int osmo_stat_item_handler(
	struct osmo_stat_item_group *statg, struct osmo_stat_item *item, void *sctx_)
{
	struct osmo_stats_reporter *srep;
	int32_t prev_reported_value = item->reported.max;
	int32_t new_value = item->value.max;

	llist_for_each_entry(srep, &osmo_stats_reporter_list, list) {
		if (!srep->running)
			continue;

		/* If the previously reported value is the same as the current value, skip resending the value.
		 * However, if the stats reporter is set to resend all values, do resend the current value regardless of
		 * repetitions.
		 */
		if (new_value == prev_reported_value && !srep->force_single_flush)
			continue;

		if (!osmo_stats_reporter_check_config(srep,
				statg->idx, statg->desc->class_id))
			continue;

		osmo_stats_reporter_send_item(srep, statg, item->desc, new_value);
	}

	osmo_stat_item_flush(item);

	return 0;
}

static int osmo_stat_item_group_handler(struct osmo_stat_item_group *statg, void *sctx_)
{
	osmo_stat_item_for_each_item(statg, osmo_stat_item_handler, sctx_);

	return 0;
}

/*** osmo counter support ***/

static int handle_counter(struct osmo_counter *counter, void *sctx_)
{
	struct osmo_stats_reporter *srep;
	struct rate_ctr_desc desc = {0};
	/* Fake a rate counter description */
	desc.name = counter->name;
	desc.description = counter->description;

	int delta = osmo_counter_difference(counter);

	llist_for_each_entry(srep, &osmo_stats_reporter_list, list) {
		if (!srep->running)
			continue;

		if (delta == 0 && !srep->force_single_flush)
			continue;

		osmo_stats_reporter_send_counter(srep, NULL, &desc,
			counter->value, delta);

		/* TODO: handle result (log?, inc counter(!)?) */
	}

	return 0;
}


/*** main reporting function ***/

static void flush_all_reporters(void)
{
	struct osmo_stats_reporter *srep;

	llist_for_each_entry(srep, &osmo_stats_reporter_list, list) {
		if (!srep->running)
			continue;

		osmo_stats_reporter_send_buffer(srep);

		/* reset force_single_flush first */
		srep->force_single_flush = 0;
		/* and schedule a new flush if it's time for it */
		if (srep->flush_period > 0) {
			srep->flush_period_counter++;
			if (srep->flush_period_counter >= srep->flush_period) {
				srep->force_single_flush = 1;
				srep->flush_period_counter = 0;
			}
		}
	}
}

int osmo_stats_report(void)
{
	/* per group actions */
	TRACE(LIBOSMOCORE_STATS_START());
	osmo_counters_for_each(handle_counter, NULL);
	rate_ctr_for_each_group(rate_ctr_group_handler, NULL);
	osmo_stat_item_for_each_group(osmo_stat_item_group_handler, NULL);

	/* global actions */
	flush_all_reporters();
	TRACE(LIBOSMOCORE_STATS_DONE());

	return 0;
}

#endif /* !EMBEDDED */

/*! @} */
