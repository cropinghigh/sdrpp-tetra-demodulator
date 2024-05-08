/*! \file timer_compat.h
 *  Compatibility header with some helpers
 */
/*
 * (C) 2011 Sylvain Munaut <tnt@246tNt.com>
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
 *  @{
 * \file timer_compat.h */

#pragma once

/* MacOS < 10.12 Sierra does not define clockid_t */
#if defined(__APPLE__) && (!defined(__DARWIN_C_LEVEL) || __DARWIN_C_LEVEL < 199309L)
typedef int clockid_t;
#endif

/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */

#ifndef timerisset
# define timerisset(tvp)	((tvp)->tv_sec || (tvp)->tv_usec)
#endif

#ifndef timerclear
# define timerclear(tvp)	((tvp)->tv_sec = (tvp)->tv_usec = 0)
#endif

#ifndef timercmp
# define timercmp(a, b, CMP) 						      \
  (((a)->tv_sec == (b)->tv_sec) ? 					      \
   ((a)->tv_usec CMP (b)->tv_usec) : 					      \
   ((a)->tv_sec CMP (b)->tv_sec))
#endif

#ifndef timeradd
# define timeradd(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;			      \
    if ((result)->tv_usec >= 1000000)					      \
      {									      \
	++(result)->tv_sec;						      \
	(result)->tv_usec -= 1000000;					      \
      }									      \
  } while (0)
#endif

#ifndef timersub
# define timersub(a, b, result)						      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;			      \
    if ((result)->tv_usec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_usec += 1000000;					      \
    }									      \
  } while (0)
#endif

/* Convenience macros for operations on timespecs.
   NOTE: `timercmp' does not work for >= or <=.  */

#ifndef timespecisset
# define timespecisset(tvp)	((tvp)->tv_sec || (tvp)->tv_nsec)
#endif

#ifndef timespecclear
# define timespecclear(tvp)	((tvp)->tv_sec = (tvp)->tv_nsec = 0)
#endif

#ifndef timespeccmp
# define timespeccmp(a, b, CMP) 				              \
  (((a)->tv_sec == (b)->tv_sec) ? 					      \
   ((a)->tv_nsec CMP (b)->tv_nsec) : 					      \
   ((a)->tv_sec CMP (b)->tv_sec))
#endif

#ifndef timespecadd
# define timespecadd(a, b, result)					      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;			      \
    (result)->tv_nsec = (a)->tv_nsec + (b)->tv_nsec;			      \
    if ((result)->tv_nsec >= 1000000000)			              \
      {									      \
	++(result)->tv_sec;						      \
	(result)->tv_nsec -= 1000000000;				      \
      }									      \
  } while (0)
#endif

#ifndef timespecsub
# define timespecsub(a, b, result)					      \
  do {									      \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;			      \
    (result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec;			      \
    if ((result)->tv_nsec < 0) {					      \
      --(result)->tv_sec;						      \
      (result)->tv_nsec += 1000000000;					      \
    }									      \
  } while (0)
#endif



/*! @} */
