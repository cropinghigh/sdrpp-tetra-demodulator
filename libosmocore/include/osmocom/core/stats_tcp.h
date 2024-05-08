#pragma once

#define TCP_STATS_DEFAULT_INTERVAL 0	/* secs */
#define TCP_STATS_DEFAULT_BATCH_SIZE 5	/* sockets per interval */

struct osmo_tcp_stats_config {
	/* poll interval in seconds, use osmo_stats_tcp_set_interval() to manipulate this value */
	int interval;
	/* specify how many sockets are processed when the interval timer expires */
	int batch_size;
};
extern struct osmo_tcp_stats_config *osmo_tcp_stats_config;

int osmo_stats_tcp_osmo_fd_register(const struct osmo_fd *fd, const char *name);
int osmo_stats_tcp_osmo_fd_unregister(const struct osmo_fd *fd);
int osmo_stats_tcp_set_interval(int interval);
