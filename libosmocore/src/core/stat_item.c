/*! \file stat_item.c
 * utility routines for keeping statistical values */
/*
 * (C) 2009-2010 by Harald Welte <laforge@gnumonks.org>
 * (C) 2015 by sysmocom - s.f.m.c. GmbH
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

/*! \addtogroup osmo_stat_item
 *  @{
 *
 *  This osmo_stat_item module adds instrumentation capabilities to
 *  gather measurement and statistical values in a similar fashion to
 *  what we have as \ref osmo_counter_group.
 *
 *  As opposed to counters, osmo_stat_item do not increment but consist
 *  of a configurable-sized FIFO, which can store not only the current
 *  (most recent) value, but also historic values.
 *
 *  The only supported value type is an int32_t.
 *
 *  Getting values from osmo_stat_item is usually done at a high level
 *  through the stats API (stats.c). It uses item->stats_next_id to
 *  store what has been sent to all enabled reporters. It is also
 *  possible to read from osmo_stat_item directly, without modifying
 *  its state, by storing next_id outside of osmo_stat_item.
 *
 *  Each value stored in the FIFO of an osmo_stat_item has an associated
 *  value_id.  The value_id is increased with each value, so (until the
 *  counter wraps) more recent values will have higher values.
 *
 *  When a new value is set, the oldest value in the FIFO gets silently
 *  overwritten.  Lost values are skipped when getting values from the
 *  item.
 *
 */

/* Struct overview:
 *
 * Group and item descriptions:
 * Each group description exists once as osmo_stat_item_group_desc,
 * each such group description lists N osmo_stat_item_desc, i.e. describes N stat items.
 *
 * Actual stats:
 * The global osmo_stat_item_groups llist contains all group instances, each points at a group description.
 * This list mixes all types of groups in a single llist, where each instance points at its group desc and has an index.
 * There are one or more instances of each group, each storing stats for a distinct object (for example, one description
 * for a BTS group, and any number of BTS instances with independent stats). A group is identified by a group index nr
 * and possibly also a given name for that particular index (e.g. in osmo-mgw, a group instance is named
 * "virtual-trunk-0" and can be looked up by that name instead of its more or less arbitrary group index number).
 *
 * Each group instance contains one osmo_stat_item instance per global stat item description.
 * Each osmo_stat_item keeps track of the values for the current reporting period (min, last, max, sum, n),
 * and also stores the set of values reported at the end of the previous reporting period.
 *
 *  const osmo_stat_item_group_desc foo
 *                                   +-- group_name_prefix = "foo"
 *                                   +-- item_desc[] (array of osmo_stat_item_desc)
 *                                        +-- osmo_stat_item_desc bar
 *                                        |    +-- name = "bar"
 *                                        |    +-- description
 *                                        |    +-- unit
 *                                        |    +-- default_value
 *                                        |
 *                                        +-- osmo_stat_item_desc: baz
 *                                             +-- ...
 *
 *  const osmo_stat_item_group_desc moo
 *                                   +-- group_name_prefix = "moo"
 *                                   +-- item_desc[]
 *                                        +-- osmo_stat_item_desc goo
 *                                        |    +-- name = "goo"
 *                                        |    +-- description
 *                                        |    +-- unit
 *                                        |    +-- default_value
 *                                        |
 *                                        +-- osmo_stat_item_desc: loo
 *                                             +-- ...
 *
 *  osmo_stat_item_groups (llist of osmo_stat_item_group)
 *   |
 *   +-- group: foo[0]
 *   |    +-- desc --> osmo_stat_item_group_desc foo
 *   |    +-- idx = 0
 *   |    +-- name = NULL (no given name for this group instance)
 *   |    +-- items[]
 *   |         |
 *   |        [0] --> osmo_stat_item instance for "bar"
 *   |         |       +-- desc --> osmo_stat_item_desc bar (see above)
 *   |         |       +-- value.{min, last, max, n, sum}
 *   |         |       +-- reported.{min, last, max, n, sum}
 *   |         |
 *   |        [1] --> osmo_stat_item instance for "baz"
 *   |         |       +-- desc --> osmo_stat_item_desc baz
 *   |         |       +-- value.{min, last, max, n, sum}
 *   |         |       +-- reported.{min, last, max, n, sum}
 *   |         .
 *   |         :
 *   |
 *   +-- group: foo[1]
 *   |    +-- desc --> osmo_stat_item_group_desc foo
 *   |    +-- idx = 1
 *   |    +-- name = "special-foo" (instance can be looked up by this index-name)
 *   |    +-- items[]
 *   |         |
 *   |        [0] --> osmo_stat_item instance for "bar"
 *   |         |       +-- desc --> osmo_stat_item_desc bar
 *   |         |       +-- value.{min, last, max, n, sum}
 *   |         |       +-- reported.{min, last, max, n, sum}
 *   |         |
 *   |        [1] --> osmo_stat_item instance for "baz"
 *   |         |       +-- desc --> osmo_stat_item_desc baz
 *   |         |       +-- value.{min, last, max, n, sum}
 *   |         |       +-- reported.{min, last, max, n, sum}
 *   |         .
 *   |         :
 *   |
 *   +-- group: moo[0]
 *   |    +-- desc --> osmo_stat_item_group_desc moo
 *   |    +-- idx = 0
 *   |    +-- name = NULL
 *   |    +-- items[]
 *   |         |
 *   |        [0] --> osmo_stat_item instance for "goo"
 *   |         |       +-- desc --> osmo_stat_item_desc goo
 *   |         |       +-- value.{min, last, max, n, sum}
 *   |         |       +-- reported.{min, last, max, n, sum}
 *   |         |
 *   |        [1] --> osmo_stat_item instance for "loo"
 *   |         |       +-- desc --> osmo_stat_item_desc loo
 *   |         |       +-- value.{min, last, max, n, sum}
 *   |         |       +-- reported.{min, last, max, n, sum}
 *   |         .
 *   |         :
 *   .
 *   :
 *
 */

#include <stdint.h>
#include <string.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/stat_item.h>

#include <stat_item_internal.h>

/*! global list of stat_item groups */
static LLIST_HEAD(osmo_stat_item_groups);

/*! talloc context from which we allocate */
static void *tall_stat_item_ctx;

/*! Allocate a new group of counters according to description.
 *  Allocate a group of stat items described in \a desc from talloc context \a ctx,
 *  giving the new group the index \a idx.
 *  \param[in] ctx \ref talloc context
 *  \param[in] desc Statistics item group description
 *  \param[in] idx Index of new stat item group
 */
struct osmo_stat_item_group *osmo_stat_item_group_alloc(void *ctx,
					    const struct osmo_stat_item_group_desc *group_desc,
					    unsigned int idx)
{
	unsigned int group_size;
	unsigned int item_idx;
	struct osmo_stat_item *items;

	struct osmo_stat_item_group *group;

	group_size = sizeof(struct osmo_stat_item_group) +
			group_desc->num_items * sizeof(struct osmo_stat_item *);

	if (!ctx)
		ctx = tall_stat_item_ctx;

	group = talloc_zero_size(ctx, group_size);
	if (!group)
		return NULL;

	group->desc = group_desc;
	group->idx = idx;

	items = talloc_array(group, struct osmo_stat_item, group_desc->num_items);
	OSMO_ASSERT(items);
	for (item_idx = 0; item_idx < group_desc->num_items; item_idx++) {
		struct osmo_stat_item *item = &items[item_idx];
		const struct osmo_stat_item_desc *item_desc = &group_desc->item_desc[item_idx];
		group->items[item_idx] = item;
		*item = (struct osmo_stat_item){
			.desc = item_desc,
			.value = {
				.n = 0,
				.last = item_desc->default_value,
				.min = item_desc->default_value,
				.max = item_desc->default_value,
				.sum = 0,
			},
		};
	}

	llist_add(&group->list, &osmo_stat_item_groups);
	return group;
}

/*! Free the memory for the specified group of stat items */
void osmo_stat_item_group_free(struct osmo_stat_item_group *grp)
{
	if (!grp)
		return;

	llist_del(&grp->list);
	talloc_free(grp);
}

/*! Get statistics item from group, identified by index idx
 *  \param[in] grp Rate counter group
 *  \param[in] idx Index of the counter to retrieve
 *  \returns rate counter requested
 */
struct osmo_stat_item *osmo_stat_item_group_get_item(struct osmo_stat_item_group *grp, unsigned int idx)
{
	return grp->items[idx];
}

/*! Set a name for the statistics item group to be used instead of index value
  at report time.
 *  \param[in] statg Statistics item group
 *  \param[in] name Name identifier to assign to the statistics item group
 */
void osmo_stat_item_group_set_name(struct osmo_stat_item_group *statg, const char *name)
{
	osmo_talloc_replace_string(statg, &statg->name, name);
}

/*! Increase the stat_item to the given value.
 *  This function adds a new value for the given stat_item at the end of
 *  the FIFO.
 *  \param[in] item The stat_item whose \a value we want to set
 *  \param[in] value The numeric value we want to store at end of FIFO
 */
void osmo_stat_item_inc(struct osmo_stat_item *item, int32_t value)
{
	osmo_stat_item_set(item, item->value.last + value);
}

/*! Descrease the stat_item to the given value.
 *  This function adds a new value for the given stat_item at the end of
 *  the FIFO.
 *  \param[in] item The stat_item whose \a value we want to set
 *  \param[in] value The numeric value we want to store at end of FIFO
 */
void osmo_stat_item_dec(struct osmo_stat_item *item, int32_t value)
{
	osmo_stat_item_set(item, item->value.last - value);
}

/*! Set the a given stat_item to the given value.
 *  This function adds a new value for the given stat_item at the end of
 *  the FIFO.
 *  \param[in] item The stat_item whose \a value we want to set
 *  \param[in] value The numeric value we want to store at end of FIFO
 */
void osmo_stat_item_set(struct osmo_stat_item *item, int32_t value)
{
	item->value.last = value;
	if (item->value.n == 0) {
		/* No values recorded yet, clamp min and max to this first value. */
		item->value.min = item->value.max = value;
		/* Overwrite any cruft remaining in value.sum */
		item->value.sum = value;
		item->value.n = 1;
	} else {
		item->value.min = OSMO_MIN(item->value.min, value);
		item->value.max = OSMO_MAX(item->value.max, value);
		item->value.sum += value;
		item->value.n++;
	}
}

/*! Indicate that a reporting period has elapsed, and prepare the stat item for a new period of collecting min/max/avg.
 * \param item  Stat item to flush.
 */
void osmo_stat_item_flush(struct osmo_stat_item *item)
{
	item->reported = item->value;

	/* Indicate a new reporting period: no values have been received, but the previous value.last remains unchanged
	 * for the case that an entire period elapses without a new value appearing. */
	item->value.n = 0;
	item->value.sum = 0;

	/* Also for the case that an entire period elapses without any osmo_stat_item_set(), put the min and max to the
	 * last value. As soon as one osmo_stat_item_set() occurs, these are both set to the new value (when n is still
	 * zero from above). */
	item->value.min = item->value.max = item->value.last;
}

/*! Initialize the stat item module. Call this once from your program.
 *  \param[in] tall_ctx Talloc context from which this module allocates */
int osmo_stat_item_init(void *tall_ctx)
{
	tall_stat_item_ctx = tall_ctx;

	return 0;
}

/*! Search for item group based on group name and index
 *  \param[in] name Name of stats_item_group we want to find
 *  \param[in] idx Index of the group we want to find
 *  \returns pointer to group, if found; NULL otherwise */
struct osmo_stat_item_group *osmo_stat_item_get_group_by_name_idx(
	const char *name, const unsigned int idx)
{
	struct osmo_stat_item_group *statg;

	llist_for_each_entry(statg, &osmo_stat_item_groups, list) {
		if (!statg->desc)
			continue;

		if (!strcmp(statg->desc->group_name_prefix, name) &&
				statg->idx == idx)
			return statg;
	}
	return NULL;
}

/*! Search for item group based on group name and index's name.
 *  \param[in] name Name of stats_item_group we want to find.
 *  \param[in] idx_name Index of the group we want to find, by the index's name (osmo_stat_item_group->name).
 *  \returns pointer to group, if found; NULL otherwise. */
struct osmo_stat_item_group *osmo_stat_item_get_group_by_name_idxname(const char *group_name, const char *idx_name)
{
	struct osmo_stat_item_group *statg;

	llist_for_each_entry(statg, &osmo_stat_item_groups, list) {
		if (!statg->desc || !statg->name)
			continue;
		if (strcmp(statg->desc->group_name_prefix, group_name))
			continue;
		if (strcmp(statg->name, idx_name))
			continue;
		return statg;
	}
	return NULL;
}

/*! Search for item based on group + item name
 *  \param[in] statg group in which to search for the item
 *  \param[in] name name of item to search within \a statg
 *  \returns pointer to item, if found; NULL otherwise */
const struct osmo_stat_item *osmo_stat_item_get_by_name(
	const struct osmo_stat_item_group *statg, const char *name)
{
	int i;
	const struct osmo_stat_item_desc *item_desc;

	if (!statg->desc)
		return NULL;

	for (i = 0; i < statg->desc->num_items; i++) {
		item_desc = &statg->desc->item_desc[i];

		if (!strcmp(item_desc->name, name)) {
			return statg->items[i];
		}
	}
	return NULL;
}

/*! Iterate over all items in group, call user-supplied function on each
 *  \param[in] statg stat_item group over whose items to iterate
 *  \param[in] handle_item Call-back function, aborts if rc < 0
 *  \param[in] data Private data handed through to \a handle_item
 */
int osmo_stat_item_for_each_item(struct osmo_stat_item_group *statg,
	osmo_stat_item_handler_t handle_item, void *data)
{
	int rc = 0;
	int i;

	for (i = 0; i < statg->desc->num_items; i++) {
		struct osmo_stat_item *item = statg->items[i];
		rc = handle_item(statg, item, data);
		if (rc < 0)
			return rc;
	}

	return rc;
}

/*! Iterate over all stat_item groups in system, call user-supplied function on each
 *  \param[in] handle_group Call-back function, aborts if rc < 0
 *  \param[in] data Private data handed through to \a handle_group
 */
int osmo_stat_item_for_each_group(osmo_stat_item_group_handler_t handle_group, void *data)
{
	struct osmo_stat_item_group *statg;
	int rc = 0;

	llist_for_each_entry(statg, &osmo_stat_item_groups, list) {
		rc = handle_group(statg, data);
		if (rc < 0)
			return rc;
	}

	return rc;
}

/*! Get the last (freshest) value. */
int32_t osmo_stat_item_get_last(const struct osmo_stat_item *item)
{
	return item->value.last;
}

/*! Remove all values of a stat item
 *  \param[in] item stat item to reset
 */
void osmo_stat_item_reset(struct osmo_stat_item *item)
{
	item->value.sum = 0;
	item->value.n = 0;
	item->value.last = item->value.min = item->value.max = item->desc->default_value;
}

/*! Reset all osmo stat items in a group
 *  \param[in] statg stat item group to reset
 */
void osmo_stat_item_group_reset(struct osmo_stat_item_group *statg)
{
	int i;

	for (i = 0; i < statg->desc->num_items; i++) {
		struct osmo_stat_item *item = statg->items[i];
                osmo_stat_item_reset(item);
	}
}

/*! Return the description for an osmo_stat_item. */
const struct osmo_stat_item_desc *osmo_stat_item_get_desc(struct osmo_stat_item *item)
{
	return item->desc;
}

/*! @} */
