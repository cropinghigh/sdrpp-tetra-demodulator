/*! \file bitcomp.h
 *  Osmocom bit compression routines. */
/*
 * (C) 2016 by sysmocom - s.f.m.c. GmbH
 * Author: Max Suraev <msuraev@sysmocom.de>
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

#pragma once

/*! \defgroup bitcomp Bit compression
 *  @{
 * \file bitcomp.h */

#include <stdint.h>
#include <stdbool.h>

#include <osmocom/core/bitvec.h>


int osmo_t4_encode(struct bitvec *bv);

/*! @} */
