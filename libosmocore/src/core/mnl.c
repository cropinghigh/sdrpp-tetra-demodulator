/*! \file mnl.c
 *
 * This code integrates libmnl (minimal netlink library) into the osmocom select
 * loop abstraction.  It allows other osmocom libraries or application code to
 * create netlink sockets and subscribe to netlink events via libmnl.  The completion
 * handler / callbacks are dispatched via libosmocore select loop handling.
 */

/*
 * (C) 2020 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <osmocom/core/select.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/mnl.h>

#include <libmnl/libmnl.h>

#include <errno.h>
#include <string.h>

/* osmo_fd call-back for when RTNL socket is readable */
static int osmo_mnl_fd_cb(struct osmo_fd *ofd, unsigned int what)
{
	uint8_t buf[MNL_SOCKET_BUFFER_SIZE];
	struct osmo_mnl *omnl = ofd->data;
	int rc;

	if (!(what & OSMO_FD_READ))
		return 0;

	rc = mnl_socket_recvfrom(omnl->mnls, buf, sizeof(buf));
	if (rc <= 0) {
		LOGP(DLGLOBAL, LOGL_ERROR, "Error in mnl_socket_recvfrom(): %s\n",
			strerror(errno));
		return -EIO;
	}

	return mnl_cb_run(buf, rc, 0, 0, omnl->mnl_cb, omnl);
}

/*! create an osmocom-wrapped limnl netlink socket.
 *  \param[in] ctx talloc context from which to allocate
 *  \param[in] bus netlink socket bus ID (see NETLINK_* constants)
 *  \param[in] groups groups of messages to bind/subscribe to
 *  \param[in] mnl_cb callback function called for each incoming message
 *  \param[in] priv opaque private user data
 *  \returns newly-allocated osmo_mnl or NULL in case of error. */
struct osmo_mnl *osmo_mnl_init(void *ctx, int bus, unsigned int groups, mnl_cb_t mnl_cb, void *priv)
{
	struct osmo_mnl *olm = talloc_zero(ctx, struct osmo_mnl);

	if (!olm)
		return NULL;

	olm->priv = priv;
	olm->mnl_cb = mnl_cb;
	olm->mnls = mnl_socket_open(bus);
	if (!olm->mnls) {
		LOGP(DLGLOBAL, LOGL_ERROR, "Error creating netlink socket for bus %d: %s\n",
			bus, strerror(errno));
		goto out_free;
	}

	if (mnl_socket_bind(olm->mnls, groups, MNL_SOCKET_AUTOPID) < 0) {
		LOGP(DLGLOBAL, LOGL_ERROR, "Error binding netlink socket for bus %d to groups 0x%x: %s\n",
			bus, groups, strerror(errno));
		goto out_close;
	}

	osmo_fd_setup(&olm->ofd, mnl_socket_get_fd(olm->mnls), OSMO_FD_READ, osmo_mnl_fd_cb, olm, 0);

	if (osmo_fd_register(&olm->ofd)) {
		LOGP(DLGLOBAL, LOGL_ERROR, "Error registering netlinks socket\n");
		goto out_close;
	}

	return olm;

out_close:
	mnl_socket_close(olm->mnls);
out_free:
	talloc_free(olm);
	return NULL;
}

/*! destroy an existing osmocom-wrapped mnl netlink socket: Unregister + close + free.
 *  \param[in] omnl osmo_mnl socket previously returned by osmo_mnl_init() */
void osmo_mnl_destroy(struct osmo_mnl *omnl)
{
	if (!omnl)
		return;

	osmo_fd_unregister(&omnl->ofd);
	mnl_socket_close(omnl->mnls);
	talloc_free(omnl);
}
