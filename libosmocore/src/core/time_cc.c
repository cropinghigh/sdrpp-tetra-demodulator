/*! \file foo.c
 * Report the cumulative counter of time for which a flag is true as rate counter.
 */
/* Copyright (C) 2021 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * All Rights Reserved
 *
 * Author: Neels Hofmeyr <nhofmeyr@sysmocom.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*! \addtogroup time_cc
 *
 * Report the cumulative counter of time for which a flag is true as rate counter.
 *
 * Useful for reporting cumulative time counters as defined in 3GPP TS 52.402, for example allAvailableSDCCHAllocated,
 * allAvailableTCHAllocated, availablePDCHAllocatedTime.
 *
 * For a usage example, see the description of struct osmo_time_cc.
 *
 * @{
 * \file time_cc.c
 */
#include "osmo_config.h"
#ifdef HAVE_CLOCK_GETTIME

#include <limits.h>
#include <time.h>

#include <osmocom/core/tdef.h>
#include <osmocom/core/rate_ctr.h>
#include <osmocom/core/time_cc.h>

#define GRAN_USEC(TIME_CC) ((TIME_CC)->cfg.gran_usec ? : 1000000)
#define ROUND_THRESHOLD_USEC(TIME_CC) ((TIME_CC)->cfg.round_threshold_usec ? \
					OSMO_MIN((TIME_CC)->cfg.round_threshold_usec, GRAN_USEC(TIME_CC)) \
					: (GRAN_USEC(TIME_CC) / 2))

static uint64_t time_now_usec(void)
{
	struct timespec tp;
	if (osmo_clock_gettime(CLOCK_MONOTONIC, &tp))
		return 0;
	return (uint64_t)tp.tv_sec * 1000000 + tp.tv_nsec / 1000;
}

static void osmo_time_cc_forget_sum(struct osmo_time_cc *tc, uint64_t now);

static void osmo_time_cc_update_from_tdef(struct osmo_time_cc *tc, uint64_t now)
{
	bool do_forget_sum = false;
	if (!tc->cfg.T_defs)
		return;
	if (tc->cfg.T_gran) {
		uint64_t was = GRAN_USEC(tc);
		tc->cfg.gran_usec = osmo_tdef_get(tc->cfg.T_defs, tc->cfg.T_gran, OSMO_TDEF_US, -1);
		if (was != GRAN_USEC(tc))
			do_forget_sum = true;
	}
	if (tc->cfg.T_round_threshold)
		tc->cfg.round_threshold_usec = osmo_tdef_get(tc->cfg.T_defs, tc->cfg.T_round_threshold,
							     OSMO_TDEF_US, -1);
	if (tc->cfg.T_forget_sum) {
		uint64_t was = tc->cfg.forget_sum_usec;
		tc->cfg.forget_sum_usec = osmo_tdef_get(tc->cfg.T_defs, tc->cfg.T_forget_sum, OSMO_TDEF_US, -1);
		if (tc->cfg.forget_sum_usec && was != tc->cfg.forget_sum_usec)
			do_forget_sum = true;
	}

	if (do_forget_sum && tc->sum)
		osmo_time_cc_forget_sum(tc, now);
}

static void osmo_time_cc_schedule_timer(struct osmo_time_cc *tc, uint64_t now);

/*! Clear out osmo_timer and internal counting state of struct osmo_time_cc. The .cfg remains unaffected. After calling,
 * the osmo_time_cc instance can be used again to accumulate state as if it had just been initialized. */
void osmo_time_cc_cleanup(struct osmo_time_cc *tc)
{
	osmo_timer_del(&tc->timer);
	*tc = (struct osmo_time_cc){
		.cfg = tc->cfg,
	};
}

static void osmo_time_cc_start(struct osmo_time_cc *tc, uint64_t now)
{
	osmo_time_cc_cleanup(tc);
	tc->start_time = now;
	tc->last_counted_time = now;
	osmo_time_cc_update_from_tdef(tc, now);
	osmo_time_cc_schedule_timer(tc, now);
}

static void osmo_time_cc_count_time(struct osmo_time_cc *tc, uint64_t now)
{
	uint64_t time_delta = now - tc->last_counted_time;
	tc->last_counted_time = now;
	if (!tc->flag_state)
		return;
	/* Flag is currently true, cumulate the elapsed time */
	tc->total_sum += time_delta;
	tc->sum += time_delta;
}

static void osmo_time_cc_report(struct osmo_time_cc *tc, uint64_t now)
{
	uint64_t delta;
	uint64_t n;
	/* We report a sum "rounded up", ahead of time. If the granularity period has not yet elapsed after the last
	 * reporting, do not report again yet. */
	if (tc->reported_sum > tc->sum)
		return;
	delta = tc->sum - tc->reported_sum;
	/* elapsed full periods */
	n = delta / GRAN_USEC(tc);
	/* If the delta has passed round_threshold (normally half of gran_usec), increment. */
	delta -= n * GRAN_USEC(tc);
	if (delta >= ROUND_THRESHOLD_USEC(tc))
		n++;
	if (!n)
		return;

	/* integer sanity, since rate_ctr_add() takes an int argument. */
	if (n > INT_MAX)
		n = INT_MAX;
	if (tc->cfg.rate_ctr)
		rate_ctr_add(tc->cfg.rate_ctr, n);
	/* Store the increments of gran_usec that were counted. */
	tc->reported_sum += n * GRAN_USEC(tc);
}

static void osmo_time_cc_forget_sum(struct osmo_time_cc *tc, uint64_t now)
{
	tc->reported_sum = 0;
	tc->sum = 0;

	if (tc->last_counted_time < now)
		tc->last_counted_time = now;
}

/*! Initialize struct osmo_time_cc. Call this once before use, and before setting up the .cfg items. */
void osmo_time_cc_init(struct osmo_time_cc *tc)
{
	*tc = (struct osmo_time_cc){0};
}

/*! Report state to be recorded by osmo_time_cc instance. Setting an unchanged state repeatedly has no effect. */
void osmo_time_cc_set_flag(struct osmo_time_cc *tc, bool flag)
{
	uint64_t now = time_now_usec();
	if (!tc->start_time)
		osmo_time_cc_start(tc, now);
	/* No flag change == no effect */
	if (flag == tc->flag_state)
		return;
	/* Sum up elapsed time, report increments for that. */
	osmo_time_cc_count_time(tc, now);
	osmo_time_cc_report(tc, now);
	tc->flag_state = flag;
	osmo_time_cc_schedule_timer(tc, now);
}

static void osmo_time_cc_timer_cb(void *data)
{
	struct osmo_time_cc *tc = data;
	uint64_t now = time_now_usec();

	osmo_time_cc_update_from_tdef(tc, now);

	if (tc->flag_state) {
		osmo_time_cc_count_time(tc, now);
		osmo_time_cc_report(tc, now);
	} else if (tc->cfg.forget_sum_usec && tc->sum
		   && (now >= tc->last_counted_time + tc->cfg.forget_sum_usec)) {
		osmo_time_cc_forget_sum(tc, now);
	}
	osmo_time_cc_schedule_timer(tc, now);
}

/*! Figure out the next time we should do anything, if the flag state remains unchanged. */
static void osmo_time_cc_schedule_timer(struct osmo_time_cc *tc, uint64_t now)
{
	uint64_t next_event = UINT64_MAX;

	osmo_time_cc_update_from_tdef(tc, now);

	/* If it is required, when will the next forget_sum happen? */
	if (tc->cfg.forget_sum_usec && !tc->flag_state && tc->sum > 0) {
		uint64_t next_forget_time = tc->last_counted_time + tc->cfg.forget_sum_usec;
		next_event = OSMO_MIN(next_event, next_forget_time);
	}
	/* Next rate_ctr increment? */
	if (tc->flag_state) {
		uint64_t next_inc = now + (tc->reported_sum - tc->sum) + ROUND_THRESHOLD_USEC(tc);
		next_event = OSMO_MIN(next_event, next_inc);
	}

	/* No event coming up? */
	if (next_event == UINT64_MAX)
		return;

	if (next_event <= now)
		next_event = 0;
	else
		next_event -= now;

	osmo_timer_setup(&tc->timer, osmo_time_cc_timer_cb, tc);
	osmo_timer_del(&tc->timer);
	osmo_timer_schedule(&tc->timer, next_event / 1000000, next_event % 1000000);
}

#endif /* HAVE_CLOCK_GETTIME */

/*! @} */
