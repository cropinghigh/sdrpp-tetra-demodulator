#pragma once

/*! \defgroup osmo_stat_item Statistics value item
 *  @{
 * \file stat_item.h */

#include <stdint.h>

#include <osmocom/core/defs.h>
#include <osmocom/core/linuxlist.h>

struct osmo_stat_item_desc;

#define OSMO_STAT_ITEM_NOVALUE_ID 0
#define OSMO_STAT_ITEM_NO_UNIT NULL

/*! data we keep for each actual item. Access via API functions only.
 * (This struct was made opaque after libosmocore 1.5.1)*/
struct osmo_stat_item;

/*! Statistics item description */
struct osmo_stat_item_desc {
	const char *name;	/*!< name of the item */
	const char *description;/*!< description of the item */
	const char *unit;	/*!< unit of a value */
	unsigned int num_values;/*!< DEPRECATED, this value is ignored after libosmocore version 1.5.1 */
	int32_t default_value;	/*!< default value */
};

/*! Description of a statistics item group */
struct osmo_stat_item_group_desc {
	/*! The prefix to the name of all values in this group */
	const char *group_name_prefix;
	/*! The human-readable description of the group */
	const char *group_description;
	/*! The class to which this group belongs */
	int class_id;
	/*! The number of values in this group (size of item_desc) */
	const unsigned int num_items;
	/*! Pointer to array of value names, length as per num_items */
	const struct osmo_stat_item_desc *item_desc;
};

/*! One instance of a counter group class */
struct osmo_stat_item_group {
	/*! Linked list of all value groups in the system */
	struct llist_head list;
	/*! Pointer to the counter group class */
	const struct osmo_stat_item_group_desc *desc;
	/*! The index of this value group within its class */
	unsigned int idx;
	/*! Optional string-based identifier to be used instead of index at report time */
	char *name;
	/*! Actual counter structures below */
	struct osmo_stat_item *items[0];
};

struct osmo_stat_item_group *osmo_stat_item_group_alloc(
	void *ctx,
	const struct osmo_stat_item_group_desc *group_desc,
	unsigned int idx);

static inline void osmo_stat_item_group_udp_idx(
	struct osmo_stat_item_group *grp, unsigned int idx)
{
	grp->idx = idx;
}
struct osmo_stat_item *osmo_stat_item_group_get_item(struct osmo_stat_item_group *grp, unsigned int idx);
void osmo_stat_item_group_set_name(struct osmo_stat_item_group *statg, const char *name);
void osmo_stat_item_group_free(struct osmo_stat_item_group *statg);

void osmo_stat_item_inc(struct osmo_stat_item *item, int32_t value);
void osmo_stat_item_dec(struct osmo_stat_item *item, int32_t value);
void osmo_stat_item_set(struct osmo_stat_item *item, int32_t value);

int osmo_stat_item_init(void *tall_ctx);

struct osmo_stat_item_group *osmo_stat_item_get_group_by_name_idx(
	const char *name, const unsigned int idx);
struct osmo_stat_item_group *osmo_stat_item_get_group_by_name_idxname(const char *group_name, const char *idx_name);

const struct osmo_stat_item *osmo_stat_item_get_by_name(
	const struct osmo_stat_item_group *statg, const char *name);

int32_t osmo_stat_item_get_last(const struct osmo_stat_item *item);

void osmo_stat_item_flush(struct osmo_stat_item *item);

typedef int (*osmo_stat_item_handler_t)(
	struct osmo_stat_item_group *, struct osmo_stat_item *, void *);

typedef int (*osmo_stat_item_group_handler_t)(struct osmo_stat_item_group *, void *);

int osmo_stat_item_for_each_item(struct osmo_stat_item_group *statg,
	osmo_stat_item_handler_t handle_item, void *data);

int osmo_stat_item_for_each_group(osmo_stat_item_group_handler_t handle_group, void *data);

void osmo_stat_item_reset(struct osmo_stat_item *item);
void osmo_stat_item_group_reset(struct osmo_stat_item_group *statg);

const struct osmo_stat_item_desc *osmo_stat_item_get_desc(struct osmo_stat_item *item);

/* DEPRECATION: up until libosmocore 1.5.1, this API also defined
 * - struct osmo_stat_item_value
 * - osmo_stat_item_get_next()
 * - osmo_stat_item_discard()
 * - osmo_stat_item_discard_all()
 * Despite our principle of never breaking API compatibility, we have decided to remove these, because there are no
 * known users. These items were never practically used/usable outside of libosmocore since the generic stats reporter
 * (stats.c) was introduced.
 * We also decided to make struct osmo_stat_item opaque to allow future changes of the struct without API breakage.
 */

/*! @} */
