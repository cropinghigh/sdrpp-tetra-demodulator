/*! \file crcXXgen.h
 * Osmocom generic CRC routines (for max XX bits poly) header. */
/*
 * Copyright (C) 2011  Sylvain Munaut <tnt@246tNt.com>
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

/*! \addtogroup crc
 *  @{
 * \file crcXXgen.h.tpl */

#include <stdint.h>
#include <osmocom/core/bits.h>


/*! structure describing a given CRC code of max XX bits */
struct osmo_crcXXgen_code {
	int bits;           /*!< Actual number of bits of the CRC */
	uintXX_t poly;      /*!< Polynom (normal representation, MSB omitted */
	uintXX_t init;      /*!< Initialization value of the CRC state */
	uintXX_t remainder; /*!< Remainder of the CRC (final XOR) */
};

uintXX_t osmo_crcXXgen_compute_bits(const struct osmo_crcXXgen_code *code,
                                    const ubit_t *in, int len);
int osmo_crcXXgen_check_bits(const struct osmo_crcXXgen_code *code,
                             const ubit_t *in, int len, const ubit_t *crc_bits);
void osmo_crcXXgen_set_bits(const struct osmo_crcXXgen_code *code,
                            const ubit_t *in, int len, ubit_t *crc_bits);


/*! @} */

/* vim: set syntax=c: */
