/*! \file byteswap.h */

#pragma once
#include <stdint.h>
#include <osmocom/core/endian.h>

/*! byte-swap a 32bit word
 *  \param[in] in to be swapped 32bit word
 *  \returns byte-swapped 32bit word */
static inline uint32_t osmo_swab32(uint32_t in)
{
	uint32_t out;

	out = (in & 0xff) << 24;
	out |= (in & 0xff00) << 8;
	out |= (in & 0xff0000) >> 8;
	out |= (in & 0xff000000) >> 24;

	return out;
}

/*! byte-swap a 16bit word
 *  \param[in] in to be swapped 16bit word
 *  \returns byte-swapped 16bit word */
static inline uint16_t osmo_swab16(uint16_t in)
{
	uint16_t out;

	out = (in & 0xff) << 8;
	out |= (in & 0xff00) >> 8;

	return out;
}

#if OSMO_IS_LITTLE_ENDIAN == 1
#define osmo_ntohl(x)	osmo_swab32(x)
#define osmo_ntohs(x)	osmo_swab16(x)
#define osmo_htonl(x)	osmo_swab32(x)
#define osmo_htons(x)	osmo_swab16(x)
#else
#define osmo_ntohl(x)	(x)
#define osmo_ntohs(x)	(x)
#define osmo_htonl(x)	(x)
#define osmo_htons(x)	(x)
#endif
