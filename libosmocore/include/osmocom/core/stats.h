/*
 * (C) 2015 by sysmocom - s.f.m.c. GmbH
 *
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

/*! \defgroup stats Statistics reporting
 *  @{
 *  \file stats.h */

/* a bit of a crude way to disable building/using this on (bare iron)
 * embedded systems.  We cannot use the autoconf-defined HAVE_... macros
 * here, as that only works at library compile time, not at application
 * compile time */
#if defined(unix) || defined(__APPLE__)

#include <sys/socket.h>
#include <arpa/inet.h>

#include <osmocom/core/linuxlist.h>

#include <stdint.h>

struct msgb;
struct osmo_stat_item_group;
struct osmo_stat_item_desc;
struct rate_ctr_group;
struct rate_ctr_desc;

/*! Statistics Class definitions */
enum osmo_stats_class {
	OSMO_STATS_CLASS_UNKNOWN,	/*!< unknown class */
	OSMO_STATS_CLASS_GLOBAL,	/*!< global counter/stat_item */
	OSMO_STATS_CLASS_PEER,		/*!< peer in a communications link */
	OSMO_STATS_CLASS_SUBSCRIBER,	/*!< subscriber */
};

/*! Statistics Reporter Type */
enum osmo_stats_reporter_type {
	OSMO_STATS_REPORTER_LOG,	/*!< libosmocore logging */
	OSMO_STATS_REPORTER_STATSD,	/*!< statsd backend */
};

/*! One statistics reporter instance. */
struct osmo_stats_reporter {
	/*! Type of the reporter (log, statsd) */
	enum osmo_stats_reporter_type type;
	/*! Human-readable name of this reporter */
	char *name;

	unsigned int have_net_config : 1;

	/* config */
	int enabled;		/*!< is this reporter enabled */
	char *name_prefix;	/*!< prefix for counter names */
	char *dest_addr_str;	/*!< destination IP address */
	char *bind_addr_str;	/*!< local bind IP address */
	int dest_port;		/*!< destination (UDP) port */
	int mtu;		/*!< Maximum Transmission Unit */
	unsigned int flush_period;	/*!< period between regular flushes */

	/*! Maximum class/index to report. FIXME: More details! */
	enum osmo_stats_class max_class;

	/* state */

	int running;			/*!< is this reporter running */
	struct sockaddr dest_addr;	/*!< destination address of socket */
	int dest_addr_len;		/*!< length of \a dest_addr in bytes */
	struct sockaddr bind_addr;	/*!< local bind address of socket */
	int bind_addr_len;		/*!< length of \a bind_addr in bytes */
	int fd;				/*!< file descriptor of socket */
	struct msgb *buffer;		/*!< message buffer for log output */
	int agg_enabled;		/*!< is aggregation enabled? */
	int force_single_flush;		/*!< set to 1 to force a flush (send even unchanged stats values) */
	unsigned int flush_period_counter;	/*!< count sends between forced flushes */

	struct llist_head list;
	int (*open)(struct osmo_stats_reporter *srep);
	int (*close)(struct osmo_stats_reporter *srep);
	int (*send_counter)(struct osmo_stats_reporter *srep,
		const struct rate_ctr_group *ctrg,
		const struct rate_ctr_desc *desc,
		int64_t value, int64_t delta);
	int (*send_item)(struct osmo_stats_reporter *srep,
		const struct osmo_stat_item_group *statg,
		const struct osmo_stat_item_desc *desc,
		int64_t value);
};

struct osmo_stats_config {
	int interval;
};

extern struct llist_head osmo_stats_reporter_list;
extern struct osmo_stats_config *osmo_stats_config;

void osmo_stats_init(void *ctx);
int osmo_stats_report(void);

int osmo_stats_set_interval(int interval);

struct osmo_stats_reporter *osmo_stats_reporter_alloc(enum osmo_stats_reporter_type type,
	const char *name);
void osmo_stats_reporter_free(struct osmo_stats_reporter *srep);

struct osmo_stats_reporter *osmo_stats_reporter_find(enum osmo_stats_reporter_type type,
	const char *name);

int osmo_stats_reporter_set_remote_addr(struct osmo_stats_reporter *srep, const char *addr);
int osmo_stats_reporter_set_remote_port(struct osmo_stats_reporter *srep, int port);
int osmo_stats_reporter_set_local_addr(struct osmo_stats_reporter *srep, const char *addr);
int osmo_stats_reporter_set_mtu(struct osmo_stats_reporter *srep, int mtu);
int osmo_stats_reporter_set_max_class(struct osmo_stats_reporter *srep,
	enum osmo_stats_class class_id);
int osmo_stats_reporter_set_name_prefix(struct osmo_stats_reporter *srep, const char *prefix);
int osmo_stats_reporter_enable(struct osmo_stats_reporter *srep);
int osmo_stats_reporter_disable(struct osmo_stats_reporter *srep);
int osmo_stats_reporter_set_flush_period(struct osmo_stats_reporter *srep, unsigned int period);

/* reporter creation */
struct osmo_stats_reporter *osmo_stats_reporter_create_log(const char *name);
struct osmo_stats_reporter *osmo_stats_reporter_create_statsd(const char *name);

/* helper functions for reporter implementations */
int osmo_stats_reporter_send(struct osmo_stats_reporter *srep, const char *data,
	int data_len);
int osmo_stats_reporter_send_buffer(struct osmo_stats_reporter *srep);
int osmo_stats_reporter_udp_open(struct osmo_stats_reporter *srep);
int osmo_stats_reporter_udp_close(struct osmo_stats_reporter *srep);

#endif /* unix || __APPLE__ */
/*! @} */
