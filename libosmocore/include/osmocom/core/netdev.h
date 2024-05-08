/*! \file netdev.h
 *  network device (interface) convenience functions. */

#pragma once
#if (!EMBEDDED)

#include <stddef.h>
#include <stdint.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/socket.h>

struct osmo_netdev;

typedef int (*osmo_netdev_ifupdown_ind_cb_t)(struct osmo_netdev *netdev, bool ifupdown);
typedef int (*osmo_netdev_dev_name_chg_cb_t)(struct osmo_netdev *netdev, const char *new_dev_name);
typedef int (*osmo_netdev_mtu_chg_cb_t)(struct osmo_netdev *netdev, unsigned int new_mtu);

struct osmo_netdev *osmo_netdev_alloc(void *ctx, const char *name);
void osmo_netdev_free(struct osmo_netdev *netdev);

int osmo_netdev_register(struct osmo_netdev *netdev);
int osmo_netdev_unregister(struct osmo_netdev *netdev);
bool osmo_netdev_is_registered(struct osmo_netdev *netdev);

const char *osmo_netdev_get_name(const struct osmo_netdev *netdev);

void osmo_netdev_set_priv_data(struct osmo_netdev *netdev, void *priv_data);
void *osmo_netdev_get_priv_data(struct osmo_netdev *netdev);

int osmo_netdev_set_ifindex(struct osmo_netdev *netdev, unsigned int ifindex);
unsigned int osmo_netdev_get_ifindex(const struct osmo_netdev *netdev);

const char *osmo_netdev_get_dev_name(const struct osmo_netdev *netdev);

int osmo_netdev_set_netns_name(struct osmo_netdev *netdev, const char *netns);
const char *osmo_netdev_get_netns_name(const struct osmo_netdev *netdev);

void osmo_netdev_set_ifupdown_ind_cb(struct osmo_netdev *netdev, osmo_netdev_ifupdown_ind_cb_t ifupdown_ind_cb);
void osmo_netdev_set_dev_name_chg_cb(struct osmo_netdev *netdev, osmo_netdev_dev_name_chg_cb_t dev_name_chg_cb);
void osmo_netdev_set_mtu_chg_cb(struct osmo_netdev *netdev, osmo_netdev_mtu_chg_cb_t mtu_chg_cb);

int osmo_netdev_add_addr(struct osmo_netdev *netdev, const struct osmo_sockaddr *addr, uint8_t prefixlen);
int osmo_netdev_add_route(struct osmo_netdev *netdev, const struct osmo_sockaddr *dst_addr,
			  uint8_t dst_prefixlen, const struct osmo_sockaddr *gw_addr);
int osmo_netdev_ifupdown(struct osmo_netdev *netdev, bool ifupdown);

#endif /* (!EMBEDDED) */
/*! @} */
