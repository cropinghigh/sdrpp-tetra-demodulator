/*
 * (C) 2016 by sysmocom s.f.m.c. GmbH <info@sysmocom.de>
 * All Rights Reserved
 *
 * Authors: Pau Espin Pedrol <pespin@sysmocom.de>
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

/*! \addtogroup timer
 *  @{
 * \file timer_clockgettime.c
 * Overriding Time: osmo_clock_gettime()
 *      - Useful to write and reproduce tests that depend on specific time
 *        factors. This API allows to fake the timespec provided by `clock_gettime()`
 *        by using a small shim osmo_clock_gettime().
 *      - Choose the clock you want to override, for instance CLOCK_MONOTONIC.
 *      - If the clock override is disabled (default) for a given clock,
 *        osmo_clock_gettime() will do the same as regular `clock_gettime()`.
 *      - If you want osmo_clock_gettime() to provide a specific time, you must
 *        enable time override with osmo_clock_override_enable(),
 *        then set a pointer to the timespec storing the fake time for that
 *        specific clock (`struct timespec *ts =
 *        osmo_clock_override_gettimespec()`) and set it as
 *        desired. Next time osmo_clock_gettime() is called, it will return the
 *        values previously set through the ts pointer.
 *      - A helper osmo_clock_override_add() is provided to increment a given
 *        overriden clock with a specific amount of time.
 */

/*! \file timer_clockgettime.c
 */

#include "osmo_config.h"
#ifdef HAVE_CLOCK_GETTIME

#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>
#include <time.h>

#include <osmocom/core/timer_compat.h>

/*! An internal structure to handle overriden time for each clock type. */
struct fakeclock {
	bool override;
	struct timespec time;
};

static struct fakeclock realtime;
static struct fakeclock realtime_coarse;
static struct fakeclock mono;
static struct fakeclock mono_coarse;
static struct fakeclock mono_raw;
static struct fakeclock boottime;
static struct fakeclock boottime;
static struct fakeclock proc_cputime_id;
static struct fakeclock th_cputime_id;

static struct fakeclock* clkid_to_fakeclock(clockid_t clk_id)
{
	switch(clk_id) {
	case CLOCK_REALTIME:
		return &realtime;
	case CLOCK_REALTIME_COARSE:
		return &realtime_coarse;
	case CLOCK_MONOTONIC:
		return &mono;
	case CLOCK_MONOTONIC_COARSE:
		return &mono_coarse;
	case CLOCK_MONOTONIC_RAW:
		return &mono_raw;
	case CLOCK_BOOTTIME:
		return &boottime;
	case CLOCK_PROCESS_CPUTIME_ID:
		return &proc_cputime_id;
	case CLOCK_THREAD_CPUTIME_ID:
		return &th_cputime_id;
	default:
		return NULL;
	}
}

/*! Shim around clock_gettime to be able to set the time manually.
 *
 * To override, use osmo_clock_override_enable and set the desired
 * current time with osmo_clock_gettimespec. */
int osmo_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
	struct fakeclock* c = clkid_to_fakeclock(clk_id);
	if (!c || !c->override)
		return clock_gettime(clk_id, tp);

	*tp = c->time;
	return 0;
}

/*! Convenience function to enable or disable a specific clock fake time.
 */
void osmo_clock_override_enable(clockid_t clk_id, bool enable)
{
	struct fakeclock* c = clkid_to_fakeclock(clk_id);
	if (c)
		c->override = enable;
}

/*! Convenience function to return a pointer to the timespec handling the
 * fake time for clock clk_id. */
struct timespec *osmo_clock_override_gettimespec(clockid_t clk_id)
{
	struct fakeclock* c = clkid_to_fakeclock(clk_id);
	if (c)
		return &c->time;
	return NULL;
}

/*! Convenience function to advance the fake time.
 *
 * Adds the given values to the clock time. */
void osmo_clock_override_add(clockid_t clk_id, time_t secs, long nsecs)
{
	struct timespec val = { secs, nsecs };
	struct fakeclock* c = clkid_to_fakeclock(clk_id);
	if (c)
		timespecadd(&c->time, &val, &c->time);
}

#endif /* HAVE_CLOCK_GETTIME */

/*! @} */
