/*! \file counter.c
 * utility routines for keeping some statistics. */
/*
 * (C) 2009 by Harald Welte <laforge@gnumonks.org>
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

#include <string.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/counter.h>

static LLIST_HEAD(counters);

/*! Global talloc context for all osmo_counter allocations. */
void *tall_ctr_ctx;

/*! Allocate a new counter with given name. Allocates from tall_ctr_ctx
 *  \param[in] name Human-readable string name for the counter
 *  \returns Allocated counter on success; NULL on error */
struct osmo_counter *osmo_counter_alloc(const char *name)
{
	struct osmo_counter *ctr = talloc_zero(tall_ctr_ctx, struct osmo_counter);

	if (!ctr)
		return NULL;

	ctr->name = name;
	llist_add_tail(&ctr->list, &counters);

	return ctr;
}

/*! Release/Destroy a given counter
 *  \param[in] ctr Counter to be destroyed */
void osmo_counter_free(struct osmo_counter *ctr)
{
	llist_del(&ctr->list);
	talloc_free(ctr);
}

/*! Iterate over all counters; call \a handle_cunter call-back for each.
 *  \param[in] handle_counter Call-back to be called for each counter; aborts if rc < 0
 *  \param[in] data Opaque data passed through to \a handle_counter function
 *  \returns 0 if all \a handle_counter calls successfull; negative on error */
int osmo_counters_for_each(int (*handle_counter)(struct osmo_counter *, void *),
			   void *data)
{
	struct osmo_counter *ctr;
	int rc = 0;

	llist_for_each_entry(ctr, &counters, list) {
		rc = handle_counter(ctr, data);
		if (rc < 0)
			return rc;
	}

	return rc;
}

/*! Counts the registered counter
 *  \returns amount of counters */
int osmo_counters_count(void)
{
	return llist_count(&counters);
}

/*! Find a counter by its name.
 *  \param[in] name Name used to look-up/search counter
 *  \returns Counter on success; NULL if not found */
struct osmo_counter *osmo_counter_get_by_name(const char *name)
{
	struct osmo_counter *ctr;

	llist_for_each_entry(ctr, &counters, list) {
		if (!strcmp(ctr->name, name))
			return ctr;
	}
	return NULL;
}

/*! Compute difference between current and previous counter value.
 *  \param[in] ctr Counter of which the difference is to be computed
 *  \returns Delta value between current counter and previous counter. Please
 *	     note that the actual counter values are unsigned long, while the
 *	     difference is computed as signed integer! */
int osmo_counter_difference(struct osmo_counter *ctr)
{
	int delta = ctr->value - ctr->previous;
	ctr->previous = ctr->value;

	return delta;
}
