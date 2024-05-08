/*
 * (C) 2021 by sysmocom - s.f.m.c. GmbH
 * Author: Philipp Maier <pmaier@sysmocom.de>
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
 *  \file stats_tcp.c */

#include "osmo_config.h"
#if !defined(EMBEDDED)

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <linux/tcp.h>
#include <errno.h>
#include <pthread.h>

#include <osmocom/core/select.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/stat_item.h>
#include <osmocom/core/stats.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/stats_tcp.h>

static struct osmo_tcp_stats_config s_tcp_stats_config = {
	.interval = TCP_STATS_DEFAULT_INTERVAL,
};

struct osmo_tcp_stats_config *osmo_tcp_stats_config = &s_tcp_stats_config;

static struct osmo_timer_list stats_tcp_poll_timer;

static LLIST_HEAD(stats_tcp);
static struct stats_tcp_entry *stats_tcp_entry_cur;
pthread_mutex_t stats_tcp_lock;

struct stats_tcp_entry {
	struct llist_head entry;
	const struct osmo_fd *fd;
	struct osmo_stat_item_group *stats_tcp;
	const char *name;
};

enum {
	STATS_TCP_UNACKED,
	STATS_TCP_LOST,
	STATS_TCP_RETRANS,
	STATS_TCP_RTT,
	STATS_TCP_RCV_RTT,
	STATS_TCP_NOTSENT_BYTES,
	STATS_TCP_RWND_LIMITED,
	STATS_TCP_SNDBUF_LIMITED,
	STATS_TCP_REORD_SEEN,
};

static struct osmo_stat_item_desc stats_tcp_item_desc[] = {
	[STATS_TCP_UNACKED] = { "tcp:unacked", "unacknowledged packets", "", 60, 0 },
	[STATS_TCP_LOST] = { "tcp:lost", "lost packets", "", 60, 0 },
	[STATS_TCP_RETRANS] = { "tcp:retrans", "retransmitted packets", "", 60, 0 },
	[STATS_TCP_RTT] = { "tcp:rtt", "roundtrip-time", "", 60, 0 },
	[STATS_TCP_RCV_RTT] = { "tcp:rcv_rtt", "roundtrip-time (receive)", "", 60, 0 },
	[STATS_TCP_NOTSENT_BYTES] = { "tcp:notsent_bytes", "bytes not yet sent", "", 60, 0 },
	[STATS_TCP_RWND_LIMITED] = { "tcp:rwnd_limited", "time (usec) limited by receive window", "", 60, 0 },
	[STATS_TCP_SNDBUF_LIMITED] = { "tcp:sndbuf_limited", "Time (usec) limited by send buffer", "", 60, 0 },
	[STATS_TCP_REORD_SEEN] = { "tcp:reord_seen", "reordering events seen", "", 60, 0 },
};

static struct osmo_stat_item_group_desc stats_tcp_desc = {
	.group_name_prefix = "tcp",
	.group_description = "stats tcp",
	.class_id = OSMO_STATS_CLASS_GLOBAL,
	.num_items = ARRAY_SIZE(stats_tcp_item_desc),
	.item_desc = stats_tcp_item_desc,
};

static void fill_stats(struct stats_tcp_entry *stats_tcp_entry)
{
	int rc;
	struct tcp_info tcp_info;
	socklen_t tcp_info_len = sizeof(tcp_info);
	char stat_name[256];

	/* Do not fill in anything before the socket is connected to a remote end */
	if (osmo_sock_get_ip_and_port(stats_tcp_entry->fd->fd, NULL, 0, NULL, 0, false) != 0)
		return;

	/* Gather TCP statistics and update the stats items */
	rc = getsockopt(stats_tcp_entry->fd->fd, IPPROTO_TCP, TCP_INFO, &tcp_info, &tcp_info_len);
	if (rc < 0)
		return;

	/* Create stats items if they do not exist yet */
	if (!stats_tcp_entry->stats_tcp) {
		stats_tcp_entry->stats_tcp =
		    osmo_stat_item_group_alloc(stats_tcp_entry, &stats_tcp_desc, stats_tcp_entry->fd->fd);
		OSMO_ASSERT(stats_tcp_entry->stats_tcp);
	}

	/* Update statistics */
	if (stats_tcp_entry->name)
		snprintf(stat_name, sizeof(stat_name), "%s", stats_tcp_entry->name);
	else
		snprintf(stat_name, sizeof(stat_name), "%s", osmo_sock_get_name2(stats_tcp_entry->fd->fd));
	osmo_stat_item_group_set_name(stats_tcp_entry->stats_tcp, stat_name);

	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_UNACKED),
			   tcp_info.tcpi_unacked);
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_LOST),
			   tcp_info.tcpi_lost);
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_RETRANS),
			   tcp_info.tcpi_retrans);
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_RTT), tcp_info.tcpi_rtt);
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_RCV_RTT),
			   tcp_info.tcpi_rcv_rtt);
#if HAVE_TCP_INFO_TCPI_NOTSENT_BYTES == 1
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_NOTSENT_BYTES),
			   tcp_info.tcpi_notsent_bytes);
#else
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_NOTSENT_BYTES), -1);
#endif

#if HAVE_TCP_INFO_TCPI_RWND_LIMITED == 1
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_RWND_LIMITED),
			   tcp_info.tcpi_rwnd_limited);
#else
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_RWND_LIMITED), -1);
#endif

#if STATS_TCP_SNDBUF_LIMITED == 1
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_REORD_SEEN),
			   tcp_info.tcpi_sndbuf_limited);
#else
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_REORD_SEEN), -1);
#endif

#if HAVE_TCP_INFO_TCPI_REORD_SEEN == 1
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_REORD_SEEN),
			   tcp_info.tcpi_reord_seen);
#else
	osmo_stat_item_set(osmo_stat_item_group_get_item(stats_tcp_entry->stats_tcp, STATS_TCP_REORD_SEEN), -1);
#endif

}

static bool is_tcp(const struct osmo_fd *fd)
{
	int rc;
	struct stat fd_stat;
	int so_protocol = 0;
	socklen_t so_protocol_len = sizeof(so_protocol);

	/* Is this a socket? */
	rc = fstat(fd->fd, &fd_stat);
	if (rc < 0)
		return false;
	if (!S_ISSOCK(fd_stat.st_mode))
		return false;

	/* Is it a TCP socket? */
	rc = getsockopt(fd->fd, SOL_SOCKET, SO_PROTOCOL, &so_protocol, &so_protocol_len);
	if (rc < 0)
		return false;
	if (so_protocol == IPPROTO_TCP)
		return true;

	return false;
}

/*! Register an osmo_fd for TCP stats monitoring.
 *  \param[in] fd osmocom file descriptor to be registered.
 *  \param[in] human readbla name that is used as prefix for the related stats item.
 *  \returns 0 on success; negative in case of error. */
int osmo_stats_tcp_osmo_fd_register(const struct osmo_fd *fd, const char *name)
{
	struct stats_tcp_entry *stats_tcp_entry;

	/* Only TCP sockets can be registered for monitoring, anything else will fall through. */
	if (!is_tcp(fd))
		return -EINVAL;

	/* When the osmo_fd is registered and unregistered properly there shouldn't be any leftovers from already closed
	 * osmo_fds in the stats_tcp list. But lets proactively make sure that any leftovers are cleaned up. */
	osmo_stats_tcp_osmo_fd_unregister(fd);

	/* Make a new list object, attach the osmo_fd... */
	stats_tcp_entry = talloc_zero(OTC_GLOBAL, struct stats_tcp_entry);
	OSMO_ASSERT(stats_tcp_entry);
	stats_tcp_entry->fd = fd;
	stats_tcp_entry->name = talloc_strdup(stats_tcp_entry, name);

	pthread_mutex_lock(&stats_tcp_lock);
	llist_add_tail(&stats_tcp_entry->entry, &stats_tcp);
	pthread_mutex_unlock(&stats_tcp_lock);

	return 0;
}

static void next_stats_tcp_entry(void)
{
	struct stats_tcp_entry *last;

	if (llist_empty(&stats_tcp)) {
		stats_tcp_entry_cur = NULL;
		return;
	}

	last = (struct stats_tcp_entry *)llist_last_entry(&stats_tcp, struct stats_tcp_entry, entry);

	if (!stats_tcp_entry_cur || stats_tcp_entry_cur == last)
		stats_tcp_entry_cur =
		    (struct stats_tcp_entry *)llist_first_entry(&stats_tcp, struct stats_tcp_entry, entry);
	else
		stats_tcp_entry_cur =
		    (struct stats_tcp_entry *)llist_entry(stats_tcp_entry_cur->entry.next, struct stats_tcp_entry,
							  entry);
}

/*! Register an osmo_fd for TCP stats monitoring.
 *  \param[in] fd osmocom file descriptor to be unregistered.
 *  \returns 0 on success; negative in case of error. */
int osmo_stats_tcp_osmo_fd_unregister(const struct osmo_fd *fd)
{
	struct stats_tcp_entry *stats_tcp_entry;
	int rc = -EINVAL;

	pthread_mutex_lock(&stats_tcp_lock);
	llist_for_each_entry(stats_tcp_entry, &stats_tcp, entry) {
		if (fd->fd == stats_tcp_entry->fd->fd) {
			/* In case we want to remove exactly that item which is also
			 * selected as the current item, we must designate either a
			 * different item or invalidate the current item.
			 */
			if (stats_tcp_entry == stats_tcp_entry_cur) {
				if (llist_count(&stats_tcp) > 2)
					next_stats_tcp_entry();
				else
					stats_tcp_entry_cur = NULL;
			}

			/* Date item from list */
			llist_del(&stats_tcp_entry->entry);
			osmo_stat_item_group_free(stats_tcp_entry->stats_tcp);
			talloc_free(stats_tcp_entry);
			rc = 0;
			break;
		}
	}
	pthread_mutex_unlock(&stats_tcp_lock);

	return rc;
}

static void stats_tcp_poll_timer_cb(void *data)
{
	int i;
	int batch_size;
	int llist_size;

	pthread_mutex_lock(&stats_tcp_lock);

	/* Make sure we do not run over the same sockets multiple times if the
	 * configured llist_size is larger then the actual list */
	batch_size = osmo_tcp_stats_config->batch_size;
	llist_size = llist_count(&stats_tcp);
	if (llist_size < batch_size)
		batch_size = llist_size;

	/* Process a batch of sockets */
	for (i = 0; i < batch_size; i++) {
		next_stats_tcp_entry();
		if (stats_tcp_entry_cur)
			fill_stats(stats_tcp_entry_cur);
	}

	pthread_mutex_unlock(&stats_tcp_lock);

	if (osmo_tcp_stats_config->interval > 0)
		osmo_timer_schedule(&stats_tcp_poll_timer, osmo_tcp_stats_config->interval, 0);
}

/*! Set the polling interval (common for all sockets)
 *  \param[in] interval Poll interval in seconds
 *  \returns 0 on success; negative on error */
int osmo_stats_tcp_set_interval(int interval)
{
	osmo_tcp_stats_config->interval = interval;
	if (osmo_tcp_stats_config->interval > 0)
		osmo_timer_schedule(&stats_tcp_poll_timer, osmo_tcp_stats_config->interval, 0);
	return 0;
}

static __attribute__((constructor))
void on_dso_load_stats_tcp(void)
{
	stats_tcp_entry_cur = NULL;
	pthread_mutex_init(&stats_tcp_lock, NULL);

	osmo_tcp_stats_config->interval = TCP_STATS_DEFAULT_INTERVAL;
	osmo_tcp_stats_config->batch_size = TCP_STATS_DEFAULT_BATCH_SIZE;

	osmo_timer_setup(&stats_tcp_poll_timer, stats_tcp_poll_timer_cb, NULL);
}

#endif /* !EMBEDDED */

/* @} */
