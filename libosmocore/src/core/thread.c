/*
 * (C) 2021 by sysmocom s.f.m.c. GmbH <info@sysmocom.de>
 * All Rights Reserved
 *
 * Author: Pau Espin Pedrol <pespin@sysmocom.de>
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

/*! \addtogroup thread
 *  @{
 * \file thread.c
 */

/*! \file thread.c
 */

#include "osmo_config.h"

/* If HAVE_GETTID, then "_GNU_SOURCE" may need to be defined to use gettid() */
#if HAVE_GETTID
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/types.h>

#include <osmocom/core/thread.h>

/*! Wrapper around Linux's gettid() to make it easily accessible on different system versions.
 * If the gettid() API cannot be found, it will use the syscall directly if
 * available. If no syscall is found available, then getpid() is called as
 * fallback. See 'man 2 gettid' for further and details information.
 * \returns This call is always successful and returns returns the thread ID of
 *          the calling thread (or the process ID of the current process if
 *          gettid() or its syscall are unavailable in the system).
 */
pid_t osmo_gettid(void)
{
#if HAVE_GETTID
	return gettid();
#elif defined(LINUX) && defined(__NR_gettid)
	return (pid_t) syscall(__NR_gettid);
#else
	#pragma message ("use pid as tid")
	return getpid();
#endif
}
