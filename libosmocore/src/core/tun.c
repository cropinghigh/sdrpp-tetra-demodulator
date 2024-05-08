
/* TUN interface functions.
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

/*! \addtogroup tun
 *  @{
 *  tun network device (interface) convenience functions
 *
 * \file tundev.c
 *
 * Example lifecycle use of the API:
 *
 *	struct osmo_sockaddr_str osa_str = {};
 *	struct osmo_sockaddr osa = {};
 *
 *	// Allocate object:
 *	struct osmo_tundev *tundev = osmo_tundev_alloc(parent_talloc_ctx, name);
 *	OSMO_ASSERT(tundev);
 *
 *	// Configure object (before opening):
 *	osmo_tundev_set_data_ind_cb(tun, tun_data_ind_cb);
 *	rc = osmo_tundev_set_dev_name(tun, "mytunnel0");
 *	rc = osmo_tundev_set_netns_name(tun, "some_netns_name_or_null");
 *
 *	// Open the tundev object:
 *	rc = osmo_tundev_open(tundev);
 *	// The tunnel device is now created and an associatd netdev object
 *	// is available to operate the device:
 *	struct osmo_netdev *netdev = osmo_tundev_get_netdev(tundev);
 *	OSMO_ASSERT(netdev);
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
 *	// Close the tunnel (asssociated netdev object becomes unavailable)
 *	rc = osmo_tundev_close(tundev);
 *	// Free the object:
 *	osmo_tundev_free(tundev);
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

#if defined(__linux__)
#include <linux/if_tun.h>
#else
#error "Unknown platform!"
#endif

#include <osmocom/core/utils.h>
#include <osmocom/core/select.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/write_queue.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/netns.h>
#include <osmocom/core/netdev.h>
#include <osmocom/core/tun.h>

#define TUN_DEV_PATH "/dev/net/tun"
#define TUN_PACKET_MAX 8196

#define LOGTUN(tun, lvl, fmt, args ...) \
	LOGP(DLGLOBAL, lvl, "TUN(%s,if=%s/%u,ns=%s): " fmt, \
	    (tun)->name, (tun)->dev_name ? : "", \
	     (tun)->ifindex, (tun)->netns_name ? : "", ## args)

struct osmo_tundev {
	/* Name used to identify the osmo_tundev */
	char *name;

	/* netdev managing the tun interface: */
	struct osmo_netdev *netdev;

	/* ifindiex of the currently opened tunnel interface */
	unsigned int ifindex;

	/* Network interface name to use when setting up the tun device.
	 * NULL = let the system pick one. */
	char *dev_name;
	/* Whether dev_name is set by user or dynamically allocated by system */
	bool dev_name_dynamic;

	/* Write queue used since tun fd is set non-blocking */
	struct osmo_wqueue wqueue;

	/* netns name where the tun interface is created (NULL = default netns) */
	char *netns_name;

	/* API user private data */
	void *priv_data;

	/* Called by tundev each time a new packet is received on the tun interface. Can be NULL. */
	osmo_tundev_data_ind_cb_t data_ind_cb;

	/* Whether the tundev is in opened state (managing the tun interface) */
	bool opened;
};

/* A new pkt arrived from the tun device, dispatch it to the API user */
static int tundev_decaps(struct osmo_tundev *tundev)
{
	struct msgb *msg;
	int rc;

	msg = msgb_alloc(TUN_PACKET_MAX, "tundev_rx");

	if ((rc = read(tundev->wqueue.bfd.fd, msgb_data(msg), TUN_PACKET_MAX)) <= 0) {
		LOGTUN(tundev, LOGL_ERROR, "read() failed: %s (%d)\n", strerror(errno), errno);
		msgb_free(msg);
		return -1;
	}
	msgb_put(msg, rc);

	if (tundev->data_ind_cb)
		return tundev->data_ind_cb(tundev, msg);

	msgb_free(msg);
	return 0;
}

/* callback for tun device osmocom select loop integration */
static int tundev_read_cb(struct osmo_fd *fd)
{
	struct osmo_tundev *tundev = fd->data;
	return tundev_decaps(tundev);
}

/* callback for tun device osmocom select loop integration */
static int tundev_write_cb(struct osmo_fd *fd, struct msgb *msg)
{
	struct osmo_tundev *tundev = fd->data;
	size_t pkt_len = msgb_length(msg);

	int rc;
	rc = write(tundev->wqueue.bfd.fd, msgb_data(msg), pkt_len);
	if (rc < 0)
		LOGTUN(tundev, LOGL_ERROR, "write() failed: %s (%d)\n", strerror(errno), errno);
	else if (rc < pkt_len)
		LOGTUN(tundev, LOGL_ERROR, "short write() %d < %zu\n", rc, pkt_len);
	return rc;
}

static int tundev_ifupdown_ind_cb(struct osmo_netdev *netdev, bool ifupdown)
{
	struct osmo_tundev *tundev = osmo_netdev_get_priv_data(netdev);
	LOGTUN(tundev, LOGL_NOTICE, "Physical link state changed: %s\n",
		  ifupdown ? "UP" : "DOWN");

	/* free any backlog, both on IFUP and IFDOWN. Keep the LMI, as it makes
	 * sense to get one out of the door ASAP. */
	osmo_wqueue_clear(&tundev->wqueue);
	return 0;
}

static int tundev_dev_name_chg_cb(struct osmo_netdev *netdev, const char *new_dev_name)
{
	struct osmo_tundev *tundev = osmo_netdev_get_priv_data(netdev);
	LOGTUN(tundev, LOGL_NOTICE, "netdev changed name: %s -> %s\n",
		  osmo_netdev_get_dev_name(netdev), new_dev_name);

	if (tundev->dev_name_dynamic) {
		osmo_talloc_replace_string(tundev, &tundev->dev_name, new_dev_name);
	} else {
		/* TODO: in here we probably want to force the iface name back
		 * to tundev->dev_name one we have a osmo_netdev_set_ifname() API */
		osmo_talloc_replace_string(tundev, &tundev->dev_name, new_dev_name);
	}

	return 0;
}

static int tundev_mtu_chg_cb(struct osmo_netdev *netdev, uint32_t new_mtu)
{
	struct osmo_tundev *tundev = osmo_netdev_get_priv_data(netdev);
	LOGTUN(tundev, LOGL_NOTICE, "netdev changed MTU: %u\n", new_mtu);

	return 0;
}

/*! Allocate a new tundev object.
 *  \param[in] ctx talloc context to use as a parent when allocating the tundev object
 *  \param[in] name A name providen to identify the tundev object
 *  \returns newly allocated tundev object on success; NULL on error
 */
struct osmo_tundev *osmo_tundev_alloc(void *ctx, const char *name)
{
	struct osmo_tundev *tundev;

	tundev = talloc_zero(ctx, struct osmo_tundev);
	if (!tundev)
		return NULL;

	tundev->netdev = osmo_netdev_alloc(tundev, name);
	if (!tundev->netdev) {
		talloc_free(tundev);
		return NULL;
	}
	osmo_netdev_set_priv_data(tundev->netdev, tundev);
	osmo_netdev_set_ifupdown_ind_cb(tundev->netdev, tundev_ifupdown_ind_cb);
	osmo_netdev_set_dev_name_chg_cb(tundev->netdev, tundev_dev_name_chg_cb);
	osmo_netdev_set_mtu_chg_cb(tundev->netdev, tundev_mtu_chg_cb);

	tundev->name = talloc_strdup(tundev, name);
	osmo_wqueue_init(&tundev->wqueue, 1000);
	osmo_fd_setup(&tundev->wqueue.bfd, -1, OSMO_FD_READ, osmo_wqueue_bfd_cb, tundev, 0);
	tundev->wqueue.read_cb = tundev_read_cb;
	tundev->wqueue.write_cb = tundev_write_cb;

	return tundev;
}

/*! Free an allocated tundev object.
 *  \param[in] tundev The tundev object to free
 */
void osmo_tundev_free(struct osmo_tundev *tundev)
{
	if (!tundev)
		return;
	osmo_tundev_close(tundev);
	osmo_netdev_free(tundev->netdev);
	talloc_free(tundev);
}

/*! Open and configure fd of the tunnel device.
 *  \param[in] tundev The tundev object whose tunnel interface to open
 *  \param[in] flags internal linux flags to pass when creating the device (not used yet)
 *  \returns 0 on success; negative on error
 */
static int tundev_open_fd(struct osmo_tundev *tundev, int flags)
{
	struct ifreq ifr;
	int fd, rc;

	fd = open(TUN_DEV_PATH, O_RDWR);
	if (fd < 0) {
		LOGTUN(tundev, LOGL_ERROR, "Cannot open " TUN_DEV_PATH ": %s\n", strerror(errno));
		return fd;
	}

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI | flags;
	if (tundev->dev_name) {
		/* if a TUN interface name was specified, put it in the structure; otherwise,
		   the kernel will try to allocate the "next" device of the specified type */
		osmo_strlcpy(ifr.ifr_name, tundev->dev_name, IFNAMSIZ);
	}

	/* try to create the device */
	rc = ioctl(fd, TUNSETIFF, (void *) &ifr);
	if (rc < 0)
		goto close_ret;

	/* Read name back from device */
	if (!tundev->dev_name) {
		ifr.ifr_name[IFNAMSIZ - 1] = '\0';
		tundev->dev_name = talloc_strdup(tundev, ifr.ifr_name);
		tundev->dev_name_dynamic = true;
	}

	/* Store interface index:
	 * (Note: there's a potential race condition here between creating the
	 * iface with a given name above and attempting to retrieve its ifindex based
	 * on that name. Someone (ie udev) could have the iface renamed in
	 * between here. It's a pity that TUNSETIFF doesn't copy back to us the
	 * newly allocated ifinidex as it does with ifname)
	 */
	tundev->ifindex = if_nametoindex(tundev->dev_name);
	if (tundev->ifindex == 0) {
		LOGTUN(tundev, LOGL_ERROR, "Unable to find ifinidex for dev %s\n",
		       tundev->dev_name);
		rc = -ENODEV;
		goto close_ret;
	}

	LOGTUN(tundev, LOGL_INFO, "TUN device created\n");

	/* set non-blocking: */
	rc = fcntl(fd, F_GETFL);
	if (rc < 0) {
		LOGTUN(tundev, LOGL_ERROR, "fcntl(F_GETFL) failed: %s (%d)\n",
		       strerror(errno), errno);
		goto close_ret;
	}
	rc = fcntl(fd, F_SETFL, rc | O_NONBLOCK);
	if (rc < 0) {
		LOGTUN(tundev, LOGL_ERROR, "fcntl(F_SETFL, O_NONBLOCK) failed: %s (%d)\n",
		       strerror(errno), errno);
		goto close_ret;
	}
	return fd;

close_ret:
	close(fd);
	return rc;
}

/*! Open the tunnel device owned by the tundev object.
 *  \param[in] tundev The tundev object to open
 *  \returns 0 on success; negative on error
 */
int osmo_tundev_open(struct osmo_tundev *tundev)
{
	struct osmo_netns_switch_state switch_state;
	int rc;
	int netns_fd = -1;

	if (tundev->opened)
		return -EALREADY;

	/* temporarily switch to specified namespace to create tun device */
	if (tundev->netns_name) {
		LOGTUN(tundev, LOGL_INFO, "Open tun: Switch to netns '%s'\n",
		       tundev->netns_name);
		netns_fd = osmo_netns_open_fd(tundev->netns_name);
		if (netns_fd < 0) {
			LOGP(DLGLOBAL, LOGL_ERROR, "Open tun: Cannot switch to netns '%s': %s (%d)\n",
			     tundev->netns_name, strerror(errno), errno);
			return netns_fd;
		}
		rc = osmo_netns_switch_enter(netns_fd, &switch_state);
		if (rc < 0) {
			LOGTUN(tundev, LOGL_ERROR, "Open tun: Cannot switch to netns '%s': %s (%d)\n",
			       tundev->netns_name, strerror(errno), errno);
			goto err_close_netns_fd;
		}
	}

	tundev->wqueue.bfd.fd = tundev_open_fd(tundev, 0);
	if (tundev->wqueue.bfd.fd < 0) {
		LOGTUN(tundev, LOGL_ERROR, "Cannot open TUN device: %s\n", strerror(errno));
		rc = -ENODEV;
		goto err_restore_ns;
	}

	/* switch back to default namespace */
	if (tundev->netns_name) {
		rc = osmo_netns_switch_exit(&switch_state);
		if (rc < 0) {
			LOGTUN(tundev, LOGL_ERROR, "Open tun: Cannot switch back from netns '%s': %s\n",
				tundev->netns_name, strerror(errno));
			goto err_close_tun;
		}
		LOGTUN(tundev, LOGL_INFO, "Open tun: Back from netns '%s'\n",
		       tundev->netns_name);
	}

	rc = osmo_netdev_set_netns_name(tundev->netdev, tundev->netns_name);
	if (rc < 0)
		goto err_close_tun;
	rc = osmo_netdev_set_ifindex(tundev->netdev, tundev->ifindex);
	if (rc < 0)
		goto err_close_tun;

	rc = osmo_netdev_register(tundev->netdev);
	if (rc < 0)
		goto err_close_tun;

	rc = osmo_fd_register(&tundev->wqueue.bfd);
	if (rc < 0)
		goto err_unregister_netdev;

	tundev->opened = true;
	return 0;

err_unregister_netdev:
	osmo_netdev_unregister(tundev->netdev);
err_close_tun:
	close(tundev->wqueue.bfd.fd);
	tundev->wqueue.bfd.fd = -1;
err_restore_ns:
	if (tundev->netns_name)
		osmo_netns_switch_exit(&switch_state);
err_close_netns_fd:
	if (netns_fd >= 0)
		close(netns_fd);
	return rc;
}

/*! Close the tunnel device owned by the tundev object.
 *  \param[in] tundev The tundev object to close
 *  \returns 0 on success; negative on error
 */
int osmo_tundev_close(struct osmo_tundev *tundev)
{
	if (!tundev->opened)
		return -EALREADY;

	osmo_wqueue_clear(&tundev->wqueue);
	if (tundev->wqueue.bfd.fd != -1) {
		osmo_fd_unregister(&tundev->wqueue.bfd);
		close(tundev->wqueue.bfd.fd);
		tundev->wqueue.bfd.fd = -1;
	}

	osmo_netdev_unregister(tundev->netdev);
	if (tundev->dev_name_dynamic) {
		TALLOC_FREE(tundev->dev_name);
		tundev->dev_name_dynamic = false;
	}
	tundev->opened = false;
	return 0;
}

/*! Retrieve whether the tundev object is in "opened" state.
 *  \param[in] tundev The tundev object to check
 *  \returns true if in state "opened"; false otherwise
 */
bool osmo_tundev_is_open(struct osmo_tundev *tundev)
{
	return tundev->opened;
}

/*! Set private user data pointer on the tundev object.
 *  \param[in] tundev The tundev object where the field is set
 */
void osmo_tundev_set_priv_data(struct osmo_tundev *tundev, void *priv_data)
{
	tundev->priv_data = priv_data;
}

/*! Get private user data pointer from the tundev object.
 *  \param[in] tundev The tundev object from where to retrieve the field
 *  \returns The current value of the priv_data field.
 */
void *osmo_tundev_get_priv_data(struct osmo_tundev *tundev)
{
	return tundev->priv_data;
}

/*! Set data_ind_cb callback, called when a new packet is received on the tun interface.
 *  \param[in] tundev The tundev object where the field is set
 *  \param[in] data_ind_cb the user provided function to be called when a new packet is received
 */
void osmo_tundev_set_data_ind_cb(struct osmo_tundev *tundev, osmo_tundev_data_ind_cb_t data_ind_cb)
{
	tundev->data_ind_cb = data_ind_cb;
}

/*! Get name used to identify the tundev object.
 *  \param[in] tundev The tundev object from where to retrieve the field
 *  \returns The current value of the name used to identify the tundev object
 */
const char *osmo_tundev_get_name(const struct osmo_tundev *tundev)
{
	return tundev->name;
}

/*! Set name used to name the tunnel interface created by the tundev object.
 *  \param[in] tundev The tundev object where the field is set
 *  \param[in] dev_name The tunnel interface name to use
 *  \returns 0 on success; negative on error
 *
 *  This is used during osmo_tundev_open() time, and hence shouldn't be changed
 *  when the tundev object is in "opened" state.
 *  If left as NULL (default), the system will pick a suitable name during
 *  osmo_tundev_open(), and the field will be updated to the system-selected
 *  name, which can be retrieved later with osmo_tundev_get_dev_name().
 */
int osmo_tundev_set_dev_name(struct osmo_tundev *tundev, const char *dev_name)
{
	if (tundev->opened)
		return -EALREADY;
	osmo_talloc_replace_string(tundev, &tundev->dev_name, dev_name);
	tundev->dev_name_dynamic = false;
	return 0;
}

/*! Get name used to name the tunnel interface created by the tundev object
 *  \param[in] tundev The tundev object from where to retrieve the field
 *  \returns The current value of the configured tunnel interface name to use
 */
const char *osmo_tundev_get_dev_name(const struct osmo_tundev *tundev)
{
	return tundev->dev_name;
}

/*! Set name of the network namespace to use when opening the tunnel interface
 *  \param[in] tundev The tundev object where the field is set
 *  \param[in] netns_name The network namespace to use during tunnel interface creation
 *  \returns 0 on success; negative on error
 *
 *  This is used during osmo_tundev_open() time, and hence shouldn't be changed
 *  when the tundev object is in "opened" state.
 *  If left as NULL (default), the system will stay in the current network namespace.
 */
int osmo_tundev_set_netns_name(struct osmo_tundev *tundev, const char *netns_name)
{
	if (tundev->opened)
		return -EALREADY;
	osmo_talloc_replace_string(tundev, &tundev->netns_name, netns_name);
	return 0;
}

/*! Get name of network namespace used when opening the tunnel interface
 *  \param[in] tundev The tundev object from where to retrieve the field
 *  \returns The current value of the configured network namespace
 */
const char *osmo_tundev_get_netns_name(const struct osmo_tundev *tundev)
{
	return tundev->netns_name;
}

/*! Get netdev managing the tunnel interface of tundev
 *  \param[in] tundev The tundev object from where to retrieve the field
 *  \returns The netdev objet managing the tun interface
 */
struct osmo_netdev *osmo_tundev_get_netdev(struct osmo_tundev *tundev)
{
	return tundev->netdev;
}

/*! Submit a packet to the tunnel device managed by the tundev object
 *  \param[in] tundev The tundev object owning the tunnel device where to inject the packet
 *  \param[in] msg The msgb containg the packet to transfer
 *  \returns The current value of the configured network namespace
 *
 * This function takes the ownership of msg in all cases.
 */
int osmo_tundev_send(struct osmo_tundev *tundev, struct msgb *msg)
{
	int rc = osmo_wqueue_enqueue(&tundev->wqueue, msg);
	if (rc < 0) {
		LOGTUN(tundev, LOGL_ERROR, "Failed to enqueue the packet\n");
		msgb_free(msg);
		return rc;
	}
	return rc;
}


#endif /* (!EMBEDDED) */

/*! @} */
