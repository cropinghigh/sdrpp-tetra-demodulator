/*
 * bit64gen.h
 *
 * Copyright (C) 2014  Max <max.suraev@fairwaves.co>
 *
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
 */

#pragma once

#include <osmocom/core/utils.h>

/*! load unaligned n-byte integer (little-endian encoding) into uint64_t, into the least significant octets.
 *  \param[in] p Buffer where integer is stored
 *  \param[in] n Number of bytes stored in p
 *  \returns 64 bit unsigned integer
 */
static inline uint64_t osmo_load64le_ext(const void *p, uint8_t n)
{
	uint8_t i;
	uint64_t r = 0;
	const uint8_t *q = (uint8_t *)p;
	OSMO_ASSERT(n <= sizeof(r));
	for(i = 0; i < n; r |= ((uint64_t)q[i] << (8 * i)), i++);
	return r;
}

/*! load unaligned n-byte integer (big-endian encoding) into uint64_t, into the MOST significant octets.
 * WARNING: for n < sizeof(uint64_t), the result is not returned in the least significant octets, as one might expect.
 * To always return the same value as fed to osmo_store64be_ext() before, use osmo_load64be_ext_2().
 *  \param[in] p Buffer where integer is stored
 *  \param[in] n Number of bytes stored in p
 *  \returns 64 bit unsigned integer
 */
static inline uint64_t osmo_load64be_ext(const void *p, uint8_t n)
{
	uint8_t i;
	uint64_t r = 0;
	const uint8_t *q = (uint8_t *)p;
	OSMO_ASSERT(n <= sizeof(r));
	for(i = 0; i < n; r |= ((uint64_t)q[i] << (64 - 8* (1 + i))), i++);
	return r;
}

/*! load unaligned n-byte integer (big-endian encoding) into uint64_t, into the least significant octets.
 *  \param[in] p Buffer where integer is stored
 *  \param[in] n Number of bytes stored in p
 *  \returns 64 bit unsigned integer
 */
static inline uint64_t osmo_load64be_ext_2(const void *p, uint8_t n)
{
	uint8_t i;
	uint64_t r = 0;
	const uint8_t *q = (uint8_t *)p;
	OSMO_ASSERT(n <= sizeof(r));
	for(i = 0; i < n; r |= ((uint64_t)q[i] << (64 - 8* (1 + i + (sizeof(r) - n)))), i++);
	return r;
}


/*! store unaligned n-byte integer (little-endian encoding) from uint64_t
 *  \param[in] x unsigned 64 bit integer
 *  \param[out] p Buffer to store integer
 *  \param[in] n Number of bytes to store
 */
static inline void osmo_store64le_ext(uint64_t x, void *p, uint8_t n)
{
	uint8_t i;
	uint8_t *q = (uint8_t *)p;
	OSMO_ASSERT(n <= sizeof(x));
	for(i = 0; i < n; q[i] = (x >> i * 8) & 0xFF, i++);
}

/*! store unaligned n-byte integer (big-endian encoding) from uint64_t
 *  \param[in] x unsigned 64 bit integer
 *  \param[out] p Buffer to store integer
 *  \param[in] n Number of bytes to store
 */
static inline void osmo_store64be_ext(uint64_t x, void *p, uint8_t n)
{
	uint8_t i;
	uint8_t *q = (uint8_t *)p;
	OSMO_ASSERT(n <= sizeof(x));
	for(i = 0; i < n; q[i] = (x >> ((n - 1 - i) * 8)) & 0xFF, i++);
}


/* Convenience function for most-used cases */


/*! load unaligned 64-bit integer (little-endian encoding) */
static inline uint64_t osmo_load64le(const void *p)
{
	return osmo_load64le_ext(p, 64 / 8);
}

/*! load unaligned 64-bit integer (big-endian encoding) */
static inline uint64_t osmo_load64be(const void *p)
{
	return osmo_load64be_ext(p, 64 / 8);
}


/*! store unaligned 64-bit integer (little-endian encoding) */
static inline void osmo_store64le(uint64_t x, void *p)
{
	osmo_store64le_ext(x, p, 64 / 8);
}

/*! store unaligned 64-bit integer (big-endian encoding) */
static inline void osmo_store64be(uint64_t x, void *p)
{
	osmo_store64be_ext(x, p, 64 / 8);
}
