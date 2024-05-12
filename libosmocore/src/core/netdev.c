
/* network device (interface) functions.
 * (C) 2023 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 * Author: Pau Espin Pedrol <pespin@sysmocom.de>
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

/*! \addtogroup netdev
 *  @{
 *  network device (interface) convenience functions
 *
 * \file netdev.c
 *
 * Example lifecycle use of the API:
 *
 *	struct osmo_sockaddr_str osa_str = {};
 *	struct osmo_sockaddr osa = {};
 *
 *	// Allocate object:
 *	struct osmo_netdev *netdev = osmo_netdev_alloc(parent_talloc_ctx, name);
 *	OSMO_ASSERT(netdev);
 *
 *	// Configure object (before registration):
 *	rc = osmo_netdev_set_netns_name(netdev, "some_netns_name_or_null");
 *	rc = osmo_netdev_set_ifindex(netdev, if_nametoindex("eth0"));
 *
 *	// Register object:
 *	rc = osmo_netdev_register(netdev);
 *	// The network interface is now being monitored and the network interface
 *	// can be operated (see below)
 *
 *	// Add a local IPv4 address:
 *	rc = osmo_sockaddr_str_from_str2(&osa_str, "192.168.200.1");
 *	rc = osmo_sockaddr_str_to_sockaddr(&osa_str, &osa.u.sas);
 *	rc = osmo_netdev_add_addr(netdev, &osa, 24);
 *
 *	// Bring network interface up:
 *	rc = osmo_netdev_ifupdown(netdev, true);
 *
 *	// Add default route (0.0.0.0/0):
 *	rc = osmo_sockaddr_str_from_str2(&osa_str, "0.0.0.0");
 *	rc = osmo_sockaddr_str_to_sockaddr(&osa_str, &osa.u.sas);
 *	rc = osmo_netdev_add_route(netdev, &osa, 0, NULL);
 *
 *	// Unregister (can be freed directly too):
 *	rc = osmo_netdev_unregister(netdev);
 *	// Free the object:
 *	osmo_netdev_free(netdev);
 */

#if (!EMBEDDED)

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <net/if.h>
#include <net/route.h>

#if defined(__linux__)
#include <linux/if_link.h>
#include <linux/rtnetlink.h>
#else
#error "Unknown platform!"
#endif

#include <osmocom/core/utils.h>
#include <osmocom/core/select.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/netns.h>
#include <osmocom/core/netdev.h>

#define IFINDEX_UNUSED 0

#define LOGNETDEV(netdev, lvl, fmt, args ...) \
	LOGP(DLGLOBAL, lvl, "NETDEV(%s,if=%s/%u,ns=%s): " fmt, \
	     (netdev)->name, osmo_netdev_get_dev_name(netdev) ? : "", \
	     (netdev)->ifindex, (netdev)->netns_name ? : "", ## args)

static struct llist_head g_netdev_netns_ctx_list = LLIST_HEAD_INIT(g_netdev_netns_ctx_list);
static struct llist_head g_netdev_list = LLIST_HEAD_INIT(g_netdev_list);

/* One per netns, shared by all osmo_netdev in a given netns: */
struct netdev_netns_ctx {
	struct llist_head entry; /* entry in g_netdev_netns_ctx_list */
	unsigned int refcount; /* Number of osmo_netdev currently registered on this netns */
	const char *netns_name; /* default netns has empty string "" (never NULL!) */
	int netns_fd; /* FD to the netns with name "netns_name" above */
};

static struct netdev_netns_ctx *netdev_netns_ctx_alloc(void *ctx, const char *netns_name)
{
	struct netdev_netns_ctx *netns_ctx;
	OSMO_ASSERT(netns_name);

	netns_ctx = talloc_zero(ctx, struct netdev_netns_ctx);
	if (!netns_ctx)
		return NULL;

	netns_ctx->netns_name = talloc_strdup(netns_ctx, netns_name);
	netns_ctx->netns_fd = -1;

	llist_add_tail(&netns_ctx->entry, &g_netdev_netns_ctx_list);
	return netns_ctx;

}

static void netdev_netns_ctx_free(struct netdev_netns_ctx *netns_ctx)
{
	if (!netns_ctx)
		return;

	llist_del(&netns_ctx->entry);

	if (netns_ctx->netns_fd != -1) {
		close(netns_ctx->netns_fd);
		netns_ctx->netns_fd = -1;
	}
	talloc_free(netns_ctx);
}

static int netdev_netns_ctx_init(struct netdev_netns_ctx *netns_ctx)
{
	struct osmo_netns_switch_state switch_state;
	int rc;

	if (netns_ctx->netns_name[0] != '\0') {
		LOGP(DLGLOBAL, LOGL_INFO, "Prepare netns: Switch to netns '%s'\n", netns_ctx->netns_name);
		netns_ctx->netns_fd = osmo_netns_open_fd(netns_ctx->netns_name);
		if (netns_ctx->netns_fd < 0) {
			LOGP(DLGLOBAL, LOGL_ERROR, "Prepare netns: Cannot switch to netns '%s': %s (%d)\n",
			     netns_ctx->netns_name, strerror(errno), errno);
			return netns_ctx->netns_fd;
		}

		/* temporarily switch to specified namespace to create netlink socket */
		rc = osmo_netns_switch_enter(netns_ctx->netns_fd, &switch_state);
		if (rc < 0) {
			LOGP(DLGLOBAL, LOGL_ERROR, "Prepare netns: Cannot switch to netns '%s': %s (%d)\n",
			     netns_ctx->netns_name, strerror(errno), errno);
			/* netns_ctx->netns_fd will be freed by future call to netdev_netns_ctx_free() */
			return rc;
		}
	}

	rc = 0;

	/* switch back to default namespace */
	if (netns_ctx->netns_name[0] != '\0') {
		int rc2 = osmo_netns_switch_exit(&switch_state);
		if (rc2 < 0) {
			LOGP(DLGLOBAL, LOGL_ERROR, "Prepare netns: Cannot switch back from netns '%s': %s\n",
			     netns_ctx->netns_name, strerror(errno));
			return rc2;
		}
		LOGP(DLGLOBAL, LOGL_INFO, "Prepare netns: Back from netns '%s'\n",
		     netns_ctx->netns_name);
	}
	return rc;
}

static struct netdev_netns_ctx *netdev_netns_ctx_find_by_netns_name(const char *netns_name)
{
	struct netdev_netns_ctx *netns_ctx;

	llist_for_each_entry(netns_ctx, &g_netdev_netns_ctx_list, entry) {
		if (strcmp(netns_ctx->netns_name, netns_name))
			continue;
		return netns_ctx;
	}

	return NULL;
}

static struct netdev_netns_ctx *netdev_netns_ctx_get(const char *netns_name)
{
	struct netdev_netns_ctx *netns_ctx;
	int rc;

	OSMO_ASSERT(netns_name);
	netns_ctx = netdev_netns_ctx_find_by_netns_name(netns_name);
	if (!netns_ctx) {
		netns_ctx = netdev_netns_ctx_alloc(NULL, netns_name);
		if (!netns_ctx)
			return NULL;
		rc = netdev_netns_ctx_init(netns_ctx);
		if (rc < 0) {
			netdev_netns_ctx_free(netns_ctx);
			return NULL;
		}
	}
	netns_ctx->refcount++;
	return netns_ctx;
}

static void netdev_netns_ctx_put(struct netdev_netns_ctx *netns_ctx)
{
	OSMO_ASSERT(netns_ctx);
	netns_ctx->refcount--;

	if (netns_ctx->refcount == 0)
		netdev_netns_ctx_free(netns_ctx);
}

struct osmo_netdev {
	/* entry in g_netdev_list */
	struct llist_head entry;

	/* Pointer to struct shared (refcounted) by all osmo_netdev in the same netns: */
	struct netdev_netns_ctx *netns_ctx;

	/* Name used to identify the osmo_netdev */
	char *name;

	/* ifindex of the network interface (address space is per netns) */
	unsigned int ifindex;

	/* Network interface name. Can change over lifetime of the interface. */
	char *dev_name;

	/* netns name where the netdev interface is created (NULL = default netns) */
	char *netns_name;

	/* API user private data */
	void *priv_data;

	/* Whether the netdev is in operation (managing the netdev interface) */
	bool registered;

	/* Called by netdev each time a new up/down state change is detected. Can be NULL. */
	osmo_netdev_ifupdown_ind_cb_t ifupdown_ind_cb;

	/* Called by netdev each time the registered network interface is renamed by the system. Can be NULL. */
	osmo_netdev_dev_name_chg_cb_t dev_name_chg_cb;

	/* Called by netdev each time the configured MTU changes in registered network interface. Can be NULL. */
	osmo_netdev_mtu_chg_cb_t mtu_chg_cb;

	/* Whether the netdev interface is UP */
	bool if_up;
	/* Whether we know the interface updown state (aka if if_up holds information)*/
	bool if_up_known;

	/* The netdev interface MTU size */
	uint32_t if_mtu;
	/* Whether we know the interface MTU size (aka if if_mtu holds information)*/
	bool if_mtu_known;
};

#define NETDEV_NETNS_ENTER(netdev, switch_state, str_prefix) \
	do { \
		if ((netdev)->netns_name) { \
			LOGNETDEV(netdev, LOGL_DEBUG, str_prefix ": Switch to netns '%s'\n", \
			       (netdev)->netns_name); \
			int rc2 = osmo_netns_switch_enter((netdev)->netns_ctx->netns_fd, switch_state); \
			if (rc2 < 0) { \
				LOGNETDEV(netdev, LOGL_ERROR, str_prefix ": Cannot switch to netns '%s': %s (%d)\n", \
				       (netdev)->netns_name, strerror(errno), errno); \
				return -EACCES; \
			} \
		} \
	} while (0)

#define NETDEV_NETNS_EXIT(netdev, switch_state, str_prefix) \
	do { \
		if ((netdev)->netns_name) { \
			int rc2 = osmo_netns_switch_exit(switch_state); \
			if (rc2 < 0) { \
				LOGNETDEV(netdev, LOGL_ERROR, str_prefix ": Cannot switch back from netns '%s': %s\n", \
				       (netdev)->netns_name, strerror(errno)); \
				return rc2; \
			} \
			LOGNETDEV(netdev, LOGL_DEBUG, str_prefix ": Back from netns '%s'\n", \
			       (netdev)->netns_name); \
		} \
	} while (0)

/*! Allocate a new netdev object.
 *  \param[in] ctx talloc context to use as a parent when allocating the netdev object
 *  \param[in] name A name providen to identify the netdev object
 *  \returns newly allocated netdev object on success; NULL on error
 */
struct osmo_netdev *osmo_netdev_alloc(void *ctx, const char *name)
{
	struct osmo_netdev *netdev;

	netdev = talloc_zero(ctx, struct osmo_netdev);
	if (!netdev)
		return NULL;

	netdev->name = talloc_strdup(netdev, name);

	llist_add_tail(&netdev->entry, &g_netdev_list);
	return netdev;
}

/*! Free an allocated netdev object.
 *  \param[in] netdev The netdev object to free
 */
void osmo_netdev_free(struct osmo_netdev *netdev)
{
	if (!netdev)
		return;
	if (osmo_netdev_is_registered(netdev))
		osmo_netdev_unregister(netdev);
	llist_del(&netdev->entry);
	talloc_free(netdev);
}

/*! Start managing the network device referenced by the netdev object.
 *  \param[in] netdev The netdev object to open
 *  \returns 0 on success; negative on error
 */
int osmo_netdev_register(struct osmo_netdev *netdev)
{
	char ifnamebuf[IF_NAMESIZE];
	struct osmo_netns_switch_state switch_state;
	int rc = 0;

	if (netdev->registered)
		return -EALREADY;

	netdev->netns_ctx = netdev_netns_ctx_get(netdev->netns_name ? : "");
	if (!netdev->netns_ctx)
		return -EFAULT;

	NETDEV_NETNS_ENTER(netdev, &switch_state, "register");

	if (!if_indextoname(netdev->ifindex, ifnamebuf)) {
		rc = -ENODEV;
		goto err_put_exit;
	}
	osmo_talloc_replace_string(netdev, &netdev->dev_name, ifnamebuf);

	NETDEV_NETNS_EXIT(netdev, &switch_state, "register");

	netdev->registered = true;
	return rc;

err_put_exit:
	NETDEV_NETNS_EXIT(netdev, &switch_state, "register");
	netdev_netns_ctx_put(netdev->netns_ctx);
	return rc;
}

/*! Unregister the netdev object (stop managing /moniutoring the interface)
 *  \param[in] netdev The netdev object to close
 *  \returns 0 on success; negative on error
 */
int osmo_netdev_unregister(struct osmo_netdev *netdev)
{
	if (!netdev->registered)
		return -EALREADY;

	netdev->if_up_known = false;
	netdev->if_mtu_known = false;

	netdev_netns_ctx_put(netdev->netns_ctx);
	netdev->registered = false;
	return 0;
}

/*! Retrieve whether the netdev object is in "registered" state.
 *  \param[in] netdev The netdev object to check
 *  \returns true if in state "registered"; false otherwise
 */
bool osmo_netdev_is_registered(struct osmo_netdev *netdev)
{
	return netdev->registered;
}

/*! Set private user data pointer on the netdev object.
 *  \param[in] netdev The netdev object where the field is set
 */
void osmo_netdev_set_priv_data(struct osmo_netdev *netdev, void *priv_data)
{
	netdev->priv_data = priv_data;
}

/*! Get private user data pointer from the netdev object.
 *  \param[in] netdev The netdev object from where to retrieve the field
 *  \returns The current value of the priv_data field.
 */
void *osmo_netdev_get_priv_data(struct osmo_netdev *netdev)
{
	return netdev->priv_data;
}

/*! Set data_ind_cb callback, called when a new packet is received on the network interface.
 *  \param[in] netdev The netdev object where the field is set
 *  \param[in] data_ind_cb the user provided function to be called when the link status (UP/DOWN) changes
 */
void osmo_netdev_set_ifupdown_ind_cb(struct osmo_netdev *netdev, osmo_netdev_ifupdown_ind_cb_t ifupdown_ind_cb)
{
	netdev->ifupdown_ind_cb = ifupdown_ind_cb;
}

/*! Set dev_name_chg_cb callback, called when a change in the network name is detected
 *  \param[in] netdev The netdev object where the field is set
 *  \param[in] dev_name_chg_cb the user provided function to be called when a the interface is renamed
 */
void osmo_netdev_set_dev_name_chg_cb(struct osmo_netdev *netdev, osmo_netdev_dev_name_chg_cb_t dev_name_chg_cb)
{
	netdev->dev_name_chg_cb = dev_name_chg_cb;
}

/*! Set mtu_chg_cb callback, called when a change in the network name is detected
 *  \param[in] netdev The netdev object where the field is set
 *  \param[in] mtu_chg_cb the user provided function to be called when the configured MTU at the interface changes
 */
void osmo_netdev_set_mtu_chg_cb(struct osmo_netdev *netdev, osmo_netdev_mtu_chg_cb_t mtu_chg_cb)
{
	netdev->mtu_chg_cb = mtu_chg_cb;
}

/*! Get name used to identify the netdev object.
 *  \param[in] netdev The netdev object from where to retrieve the field
 *  \returns The current value of the name used to identify the netdev object
 */
const char *osmo_netdev_get_name(const struct osmo_netdev *netdev)
{
	return netdev->name;
}

/*! Set (specify) interface index identifying the network interface to manage
 *  \param[in] netdev The netdev object where the field is set
 *  \param[in] ifindex The interface index identifying the interface
 *  \returns 0 on success; negative on error
 *
 *  The ifindex, together with the netns_name (see
 *  osmo_netdev_netns_name_set()), form together the key identifiers of a
 *  network interface to manage.
 *  This field is used during osmo_netdev_register() time, and hence must be set
 *  before calling that API, and cannot be changed when the netdev object is in
 *  "registered" state.
 */
int osmo_netdev_set_ifindex(struct osmo_netdev *netdev, unsigned int ifindex)
{
	if (netdev->registered)
		return -EALREADY;
	netdev->ifindex = ifindex;
	return 0;
}

/*! Get interface index identifying the interface managed by netdev
 *  \param[in] netdev The netdev object from where to retrieve the field
 *  \returns The current value of the configured netdev interface ifindex to use (0 = unset)
 */
unsigned int osmo_netdev_get_ifindex(const struct osmo_netdev *netdev)
{
	return netdev->ifindex;
}

/*! Set (specify) name of the network namespace where the network interface to manage is located
 *  \param[in] netdev The netdev object where the field is set
 *  \param[in] netns_name The network namespace where the network interface is located
 *  \returns 0 on success; negative on error
 *
 *  The netns_name, together with the ifindex (see
 *  osmo_netdev_ifindex_set()), form together the key identifiers of a
 *  network interface to manage.
 *  This field is used during osmo_netdev_register() time, and hence must be set
 *  before calling that API, and cannot be changed when the netdev object is in
 *  "registered" state.
 *  If left as NULL (default), the management will be done in the current network namespace.
 */
int osmo_netdev_set_netns_name(struct osmo_netdev *netdev, const char *netns_name)
{
	if (netdev->registered)
		return -EALREADY;
	osmo_talloc_replace_string(netdev, &netdev->netns_name, netns_name);
	return 0;
}

/*! Get name of network namespace used when opening the netdev interface
 *  \param[in] netdev The netdev object from where to retrieve the field
 *  \returns The current value of the configured network namespace
 */
const char *osmo_netdev_get_netns_name(const struct osmo_netdev *netdev)
{
	return netdev->netns_name;
}

/*! Get name used to name the network interface created by the netdev object
 *  \param[in] netdev The netdev object from where to retrieve the field
 *  \returns The interface name (or NULL if unknown)
 *
 * This information is retrieved internally once the netdev object enters the
 * "registered" state. Hence, when not registered NULL  can be returned.
 */
const char *osmo_netdev_get_dev_name(const struct osmo_netdev *netdev)
{
	return netdev->dev_name;
}

/*! Bring netdev interface UP or DOWN.
 *  \param[in] netdev The netdev object managing the netdev interface
 *  \param[in] ifupdown true to set the interface UP, false to set it DOWN
 *  \returns 0 on succes; negative on error.
 */
int osmo_netdev_ifupdown(struct osmo_netdev *netdev, bool ifupdown)
{
	struct osmo_netns_switch_state switch_state;
	int rc;

	if (!netdev->registered)
		return -ENODEV;

	LOGNETDEV(netdev, LOGL_NOTICE, "Bringing dev %s %s\n",
		  netdev->dev_name, ifupdown ? "UP" : "DOWN");

	NETDEV_NETNS_ENTER(netdev, &switch_state, "ifupdown");

	LOGNETDEV(netdev, LOGL_ERROR, "%s: NOT SUPPORTED. Build libosmocore with --enable-libmnl.\n", __func__);
	rc = -ENOTSUP;

	NETDEV_NETNS_EXIT(netdev, &switch_state, "ifupdown");

	return rc;
}

/*! Add IP address to netdev interface
 *  \param[in] netdev The netdev object managing the netdev interface
 *  \param[in] addr The local address to set on the interface
 *  \param[in] prefixlen The network prefix of addr
 *  \returns 0 on succes; negative on error.
 */
int osmo_netdev_add_addr(struct osmo_netdev *netdev, const struct osmo_sockaddr *addr, uint8_t prefixlen)
{
	struct osmo_netns_switch_state switch_state;
	char buf[INET6_ADDRSTRLEN];
	int rc;

	if (!netdev->registered)
		return -ENODEV;

	LOGNETDEV(netdev, LOGL_NOTICE, "Adding address %s/%u to dev %s\n",
	       osmo_sockaddr_ntop(&addr->u.sa, buf), prefixlen, netdev->dev_name);

	NETDEV_NETNS_ENTER(netdev, &switch_state, "Add address");

	LOGNETDEV(netdev, LOGL_ERROR, "%s: NOT SUPPORTED. Build libosmocore with --enable-libmnl.\n", __func__);
	rc = -ENOTSUP;

	NETDEV_NETNS_EXIT(netdev, &switch_state, "Add address");

	return rc;
}

/*! Add IP route to netdev interface
 *  \param[in] netdev The netdev object managing the netdev interface
 *  \param[in] dst_addr The destination address of the route
 *  \param[in] dst_prefixlen The network prefix of dst_addr
 *  \param[in] gw_addr The gateway address. Optional, can be NULL.
 *  \returns 0 on succes; negative on error.
 */
int osmo_netdev_add_route(struct osmo_netdev *netdev, const struct osmo_sockaddr *dst_addr, uint8_t dst_prefixlen, const struct osmo_sockaddr *gw_addr)
{
	struct osmo_netns_switch_state switch_state;
	char buf_dst[INET6_ADDRSTRLEN];
	char buf_gw[INET6_ADDRSTRLEN];
	int rc;

	if (!netdev->registered)
		return -ENODEV;

	LOGNETDEV(netdev, LOGL_NOTICE, "Adding route %s/%u%s%s dev %s\n",
	       osmo_sockaddr_ntop(&dst_addr->u.sa, buf_dst), dst_prefixlen,
	       gw_addr ? " via " : "",
	       gw_addr ? osmo_sockaddr_ntop(&gw_addr->u.sa, buf_gw) : "",
	       netdev->dev_name);

	NETDEV_NETNS_ENTER(netdev, &switch_state, "Add route");

	LOGNETDEV(netdev, LOGL_ERROR, "%s: NOT SUPPORTED. Build libosmocore with --enable-libmnl.\n", __func__);
	rc = -ENOTSUP;

	NETDEV_NETNS_EXIT(netdev, &switch_state, "Add route");

	return rc;
}

#endif /* (!EMBEDDED) */

/*! @} */
