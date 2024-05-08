/*! \file select.h
 *  libmnl integration
 */
#pragma once

#include <osmocom/core/select.h>
#include <libmnl/libmnl.h>

/*! osmocom wrapper around libmnl abstraction of netlink socket */
struct osmo_mnl {
	/*! osmo-wrapped netlink file descriptor */
	struct osmo_fd ofd;
	/*! libmnl socket abstraction */
	struct mnl_socket *mnls;
	/*! call-back called for received netlink messages */
	mnl_cb_t mnl_cb;
	/*! opaque data provided by user */
	void *priv;
};

struct osmo_mnl *osmo_mnl_init(void *ctx, int bus, unsigned int groups, mnl_cb_t mnl_cb, void *priv);
void osmo_mnl_destroy(struct osmo_mnl *omnl);
