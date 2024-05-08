/*! \file cpu_sched_vty.h
 * API to CPU / Threading / Scheduler properties from VTY configuration.
 */
/* (C) 2020 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * Author: Pau Espin Pedrol <pespin@sysmocom.de>
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#pragma once

#include <osmocom/vty/command.h>

/*! \defgroup cpu_sched_VTY Configuration
 * @{
 * \file cpu_sched_vty.h
 */

void osmo_cpu_sched_vty_init(void *tall_ctx);
int osmo_cpu_sched_vty_apply_localthread(void);

/*! @} */
