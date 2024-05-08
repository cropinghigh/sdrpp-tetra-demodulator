/*
 * (C) 2016 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 * Authors: Neels Hofmeyr <nhofmeyr@sysmocom.de>
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

/*! \addtogroup timer
 *  @{
 * \file timer_gettimeofday.c
 * Overriding Time: osmo_gettimeofday()
 *      - Useful to write and reproduce tests that depend on specific time
 *        factors. This API allows to fake the timeval provided by `gettimeofday()`
 *        by using a small shim osmo_gettimeofday().
 *      - If the clock override is disabled (default) for a given clock,
 *        osmo_gettimeofday() will do the same as regular `gettimeofday()`.
 *      - If you want osmo_gettimeofday() to provide a specific time, you must
 *        enable time override by setting the global variable
 *        osmo_gettimeofday_override (`osmo_gettimeofday_override = true`), then
 *        set the global struct timeval osmo_gettimeofday_override_time wih the
 *        desired value. Next time osmo_gettimeofday() is called, it will return
 *        the values previously set.
 *      - A helper osmo_gettimeofday_override_add() is provided to easily
 *        increment osmo_gettimeofday_override_time with a specific amount of
 *        time.
 */

#include <stdbool.h>
#include <sys/time.h>
#include <osmocom/core/timer_compat.h>

bool osmo_gettimeofday_override = false;
struct timeval osmo_gettimeofday_override_time = { 23, 424242 };

/*! shim around gettimeofday to be able to set the time manually.
 * To override, set osmo_gettimeofday_override == true and set the desired
 * current time in osmo_gettimeofday_override_time.
 *
 * N. B: gettimeofday() is affected by discontinuous jumps in the system time
 *       (e.g., if the system administrator manually changes the system time).
 *       Hence this should NEVER be used for elapsed time computation.
 *       Instead, osmo_clock_gettime() with CLOCK_MONOTONIC should be used for that.
 */
int osmo_gettimeofday(struct timeval *tv, struct timezone *tz)
{
	if (osmo_gettimeofday_override) {
		*tv = osmo_gettimeofday_override_time;
		return 0;
	}

	return gettimeofday(tv, tz);
}

/*! convenience function to advance the fake time.
 * Add the given values to osmo_gettimeofday_override_time. */
void osmo_gettimeofday_override_add(time_t secs, suseconds_t usecs)
{
	struct timeval val = { secs, usecs };
	timeradd(&osmo_gettimeofday_override_time, &val,
		 &osmo_gettimeofday_override_time);
}

/*! @} */
