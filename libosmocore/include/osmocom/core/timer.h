/*! \file timer.h
 *  Osmocom timer handling routines. */
/*
 * (C) 2008, 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
 * All Rights Reserved
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

/*! \defgroup timer Osmocom timers
 * Timer management:
 *      - Create a struct osmo_timer_list
 *      - Fill out timeout and use osmo_timer_add(), or
 *        use osmo_timer_schedule() to schedule a timer in
 *        x seconds and microseconds from now...
 *      - Use osmo_timer_del() to remove the timer
 *
 *  Internally:
 *      - We hook into select.c to give a timeval of the
 *        nearest timer. On already passed timers we give
 *        it a 0 to immediately fire after the select
 *      - osmo_timers_update() will call the callbacks and
 *        remove the timers.
 *  @{
 * \file timer.h */

#pragma once

#include <sys/time.h>
#include <time.h>
#include <stdbool.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/linuxrbtree.h>
#include <osmocom/core/timer_compat.h>

/* convert absolute time (in seconds) to elapsed days/hours/minutes */
#define OSMO_SEC2MIN(sec) ((sec % (60 * 60)) / 60)
#define OSMO_SEC2HRS(sec) ((sec % (60 * 60 * 24)) / (60 * 60))
#define OSMO_SEC2DAY(sec) ((sec % (60 * 60 * 24 * 365)) / (60 * 60 * 24)) /* we ignore leap year for simplicity */

/*! A structure representing a single instance of a timer */
struct osmo_timer_list {
	struct rb_node node;	  /*!< rb-tree node header */
	struct llist_head list;   /*!< internal list header */
	struct timeval timeout;   /*!< expiration time */
	unsigned int active  : 1; /*!< is it active? */

	void (*cb)(void*);	  /*!< call-back called at timeout */
	void *data;		  /*!< user data for callback */
};

/*
 * timer management
 */

void osmo_timer_setup(struct osmo_timer_list *timer, void (*cb)(void *data), void *data);

void osmo_timer_add(struct osmo_timer_list *timer);

void osmo_timer_schedule(struct osmo_timer_list *timer, int seconds, int microseconds);

void osmo_timer_del(struct osmo_timer_list *timer);

int osmo_timer_pending(const struct osmo_timer_list *timer);

int osmo_timer_remaining(const struct osmo_timer_list *timer,
			 const struct timeval *now,
			 struct timeval *remaining);
/*
 * internal timer list management
 */
struct timeval *osmo_timers_nearest(void);
int osmo_timers_nearest_ms(void);
void osmo_timers_prepare(void);
int osmo_timers_update(void);
int osmo_timers_check(void);

int osmo_gettimeofday(struct timeval *tv, struct timezone *tz);
int osmo_clock_gettime(clockid_t clk_id, struct timespec *tp);

/*
 * timer override
 */

extern bool osmo_gettimeofday_override;
extern struct timeval osmo_gettimeofday_override_time;
void osmo_gettimeofday_override_add(time_t secs, suseconds_t usecs);

void osmo_clock_override_enable(clockid_t clk_id, bool enable);
void osmo_clock_override_add(clockid_t clk_id, time_t secs, long nsecs);
struct timespec *osmo_clock_override_gettimespec(clockid_t clk_id);

/*! @} */
