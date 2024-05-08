
/* Network namespace convenience functions
 * (C) 2023 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
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

/*! \addtogroup netns
 *  @{
 *  Network namespace convenience functions
 *
 * \file netns.c */

#if defined(__linux__)

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/netns.h>

#define NETNS_PREFIX_PATH "/var/run/netns"
#define NETNS_CURRENT_PATH "/proc/self/ns/net"

/*! Open a file descriptor for the current network namespace.
 *  \returns fd of the current network namespace on success; negative in case of error
 */
static int netns_open_current_fd(void)
{
	int fd;
	/* store the default namespace for later reference */
	if ((fd = open(NETNS_CURRENT_PATH, O_RDONLY)) < 0)
		return -errno;
	return fd;
}

/*! switch to a (non-default) namespace, store existing signal mask in oldmask.
 *  \param[in] nsfd file descriptor representing the namespace to which we shall switch
 *  \param[out] state caller-provided memory location to which state of previous netns is stored
 *  \returns 0 on success; negative on error */
int osmo_netns_switch_enter(int nsfd, struct osmo_netns_switch_state *state)
{
	sigset_t intmask;
	int rc;

	state->prev_nsfd = -1;

	if (sigfillset(&intmask) < 0)
		return -errno;
	if ((rc = sigprocmask(SIG_BLOCK, &intmask, &state->prev_sigmask)) != 0)
		return -rc;
	state->prev_nsfd = netns_open_current_fd();

	if (setns(nsfd, CLONE_NEWNET) < 0) {
		/* restore old mask if we couldn't switch the netns */
		sigprocmask(SIG_SETMASK, &state->prev_sigmask, NULL);
		close(state->prev_nsfd);
		state->prev_nsfd = -1;
		return -errno;
	}
	return 0;
}

/*! switch back to the previous namespace, restoring signal mask.
 *  \param[in] state information about previous netns, filled by osmo_netns_switch_enter()
 *  \returns 0 on successs; negative on error */
int osmo_netns_switch_exit(struct osmo_netns_switch_state *state)
{
	if (state->prev_nsfd < 0)
		return -EINVAL;

	int rc;
	if (setns(state->prev_nsfd, CLONE_NEWNET) < 0)
		return -errno;

	close(state->prev_nsfd);
	state->prev_nsfd = -1;

	if ((rc = sigprocmask(SIG_SETMASK, &state->prev_sigmask, NULL)) != 0)
		return -rc;
	return 0;
}

static int create_netns(const char *name)
{
	char path[MAXPATHLEN];
	sigset_t intmask, oldmask;
	int fd, prev_nsfd;
	int rc, rc2;

	/* create /var/run/netns, if it doesn't exist already */
	rc = mkdir(NETNS_PREFIX_PATH, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH);
	if (rc < 0 && errno != EEXIST)
		return rc;

	/* create /var/run/netns/[name], if it doesn't exist already */
	rc = snprintf(path, sizeof(path), "%s/%s", NETNS_PREFIX_PATH, name);
	if (rc >= sizeof(path))
		return -ENAMETOOLONG;
	fd = open(path, O_RDONLY|O_CREAT|O_EXCL, 0);
	if (fd < 0)
		return -errno;
	if (close(fd) < 0)
		return -errno;

	/* mask off all signals, store old signal mask */
	if (sigfillset(&intmask) < 0)
		return -errno;
	if ((rc = sigprocmask(SIG_BLOCK, &intmask, &oldmask)) != 0)
		return -rc;

	prev_nsfd = netns_open_current_fd();
	if (prev_nsfd < 0)
		return prev_nsfd;

	/* create a new network namespace */
	if (unshare(CLONE_NEWNET) < 0) {
		rc = -errno;
		goto restore_sigmask;
	}
	if (mount(NETNS_CURRENT_PATH, path, "none", MS_BIND, NULL) < 0) {
		rc = -errno;
		goto restore_sigmask;
	}

	/* switch back to previous namespace */
	if (setns(prev_nsfd, CLONE_NEWNET) < 0) {
		rc = -errno;
		goto restore_sigmask;
	}

restore_sigmask:
	close(prev_nsfd);

	/* restore process mask */
	if ((rc2 = sigprocmask(SIG_SETMASK, &oldmask, NULL)) != 0)
		return -rc2;

	/* might have been set above in case mount fails */
	if (rc < 0)
		return rc;

	/* finally, open the created namespace file descriptor from previous ns */
	if ((fd = open(path, O_RDONLY)) < 0)
		return -errno;

	return fd;
}

/*! Open a file descriptor for the network namespace with provided name.
 *  Creates /var/run/netns/ directory if it doesn't exist already.
 *  \param[in] name Name of the network namespace (in /var/run/netns/)
 *  \returns File descriptor of network namespace; negative in case of error
 */
int osmo_netns_open_fd(const char *name)
{
	int rc;
	int fd;
	char path[MAXPATHLEN];

	/* path = /var/run/netns/[name] */
	rc = snprintf(path, sizeof(path), "%s/%s", NETNS_PREFIX_PATH, name);
	if (rc >= sizeof(path))
		return -ENAMETOOLONG;

	/* If netns already exists, simply open it: */
	fd = open(path, O_RDONLY);
	if (fd >= 0)
		return fd;

	/* The netns doesn't exist yet, let's create it: */
	fd = create_netns(name);
	return fd;
}

#endif /* defined(__linux__) */

/*! @} */
