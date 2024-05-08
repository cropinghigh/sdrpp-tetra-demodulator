/*! \file tdef_vty.h
 * API to configure osmo_tdef Tnnn timers from VTY configuration.
 */
/* (C) 2018-2019 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * Author: Neels Hofmeyr <neels@hofmeyr.de>
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

#include <stdint.h>
#include <stdarg.h>

#include <osmocom/vty/command.h>

struct vty;

/*! \defgroup Tdef_VTY  Tnnn timer VTY configuration
 * @{
 * \file tdef_vty.h
 */

struct osmo_tdef;
struct osmo_tdef_group;

#define OSMO_TDEF_VTY_ARG_T "TNNNN"
#define OSMO_TDEF_VTY_DOC_T \
	"T- or X-timer-number -- 3GPP compliant timer number of the format '1234' or 'T1234' or 't1234';" \
	" Osmocom-specific timer number of the format: 'X1234' or 'x1234'.\n"
#define OSMO_TDEF_VTY_ARG_T_OPTIONAL "[" OSMO_TDEF_VTY_ARG_T "]"

#define OSMO_TDEF_VTY_ARG_VAL "(<0-2147483647>|default)"
#define OSMO_TDEF_VTY_DOC_VAL "New timer value\n" "Set to default timer value\n"
#define OSMO_TDEF_VTY_ARG_VAL_OPTIONAL "[" OSMO_TDEF_VTY_ARG_VAL "]"

#define OSMO_TDEF_VTY_ARG_SET	OSMO_TDEF_VTY_ARG_T " " OSMO_TDEF_VTY_ARG_VAL
#define OSMO_TDEF_VTY_DOC_SET	OSMO_TDEF_VTY_DOC_T OSMO_TDEF_VTY_DOC_VAL
#define OSMO_TDEF_VTY_ARG_SET_OPTIONAL	OSMO_TDEF_VTY_ARG_T_OPTIONAL " " OSMO_TDEF_VTY_ARG_VAL_OPTIONAL

int osmo_tdef_vty_set_cmd(struct vty *vty, struct osmo_tdef *tdefs, const char **args);
int osmo_tdef_vty_show_cmd(struct vty *vty, struct osmo_tdef *tdefs, const char *T_arg,
			   const char *prefix_fmt, ...);
void osmo_tdef_vty_write(struct vty *vty, struct osmo_tdef *tdefs,
			 const char *prefix_fmt, ...);

void osmo_tdef_vty_out_one(struct vty *vty, struct osmo_tdef *t, const char *prefix_fmt, ...);
void osmo_tdef_vty_out_all(struct vty *vty, struct osmo_tdef *tdefs, const char *prefix_fmt, ...);

void osmo_tdef_vty_out_one_va(struct vty *vty, struct osmo_tdef *t, const char *prefix_fmt, va_list va);
void osmo_tdef_vty_out_all_va(struct vty *vty, struct osmo_tdef *tdefs, const char *prefix_fmt, va_list va);

struct osmo_tdef *osmo_tdef_vty_parse_T_arg(struct vty *vty, struct osmo_tdef *tdefs, const char *osmo_tdef_str);
unsigned long osmo_tdef_vty_parse_val_arg(const char *val_arg, unsigned long default_val);

void osmo_tdef_vty_groups_init(unsigned int parent_cfg_node, struct osmo_tdef_group *groups);
void osmo_tdef_vty_groups_write(struct vty *vty, const char *indent);

/*! @} */
