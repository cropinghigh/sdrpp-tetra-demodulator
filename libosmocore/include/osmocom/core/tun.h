/*! \file tun.h
 *  tunnel network device convenience functions. */

#pragma once
#if (!EMBEDDED)

#include <stddef.h>
#include <stdint.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/netdev.h>

struct osmo_tundev;

/* callback user gets ownership of the msgb and is expected to free it. */
typedef int (*osmo_tundev_data_ind_cb_t)(struct osmo_tundev *tun, struct msgb *msg);

struct osmo_tundev *osmo_tundev_alloc(void *ctx, const char *name);
void osmo_tundev_free(struct osmo_tundev *tundev);
int osmo_tundev_open(struct osmo_tundev *tundev);
int osmo_tundev_close(struct osmo_tundev *tundev);
bool osmo_tundev_is_open(struct osmo_tundev *tundev);

void osmo_tundev_set_priv_data(struct osmo_tundev *tundev, void *priv_data);
void *osmo_tundev_get_priv_data(struct osmo_tundev *tundev);

void osmo_tundev_set_data_ind_cb(struct osmo_tundev *tundev, osmo_tundev_data_ind_cb_t data_ind_cb);

const char *osmo_tundev_get_name(const struct osmo_tundev *tundev);

int osmo_tundev_set_dev_name(struct osmo_tundev *tundev, const char *dev_name);
const char *osmo_tundev_get_dev_name(const struct osmo_tundev *tundev);

int osmo_tundev_set_netns_name(struct osmo_tundev *tundev, const char *netns);
const char *osmo_tundev_get_netns_name(const struct osmo_tundev *tundev);

struct osmo_netdev *osmo_tundev_get_netdev(struct osmo_tundev *tundev);

int osmo_tundev_send(struct osmo_tundev *tundev, struct msgb *msg);

#endif /* (!EMBEDDED) */
/*! @} */
