/*! \file tdef.h
 * API to define Tnnn timers globally and use for FSM state changes.
 */
/*
 * (C) 2018-2019 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * Author: Neels Hofmeyr <neels@hofmeyr.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <stdint.h>
#include <osmocom/core/utils.h>

struct osmo_fsm_inst;

/*! \defgroup Tdef  Tnnn timer configuration
 * @{
 * \file tdef.h
 */

enum osmo_tdef_unit {
	OSMO_TDEF_S = 0,	/*!< most T are in seconds, keep 0 as default. */
	OSMO_TDEF_MS,		/*!< milliseconds */
	OSMO_TDEF_M,		/*!< minutes */
	OSMO_TDEF_CUSTOM,	/*!< unspecified unit, explained in osmo_tdef.desc. */
	OSMO_TDEF_US,		/*!< microseconds */
};

extern const struct value_string osmo_tdef_unit_names[];
/*! \return enum osmo_tdef_unit value as human readable unit letter, or "custom-unit". */
static inline const char *osmo_tdef_unit_name(enum osmo_tdef_unit val)
{ return get_value_string(osmo_tdef_unit_names, val); }

/*! Define a GSM timer of the form Tnnn, with unit, default value and doc string.
 * Typically used as an array with the last entry being left zero-initialized, e.g.:
 *
 *         struct osmo_tdef tdefs[] = {
 *                 { .T=10, .default_val=6, .desc="RR Assignment" },
 *                 { .T=101, .default_val=10, .desc="inter-BSC Handover MT, HO Request to HO Accept" },
 *                 { .T=3101, .default_val=3, .desc="RR Immediate Assignment" },
 *                 { .T=-23, .default_val=42, .desc="internal X23 timeout (contrived example)" },
 *                 {}
 *         };
 *
 * Program initialization should call osmo_tdefs_reset() so that all timers return the default_val, until e.g. the VTY
 * configuration sets user-defined values (see osmo_tdef_vty_init()).
 */
struct osmo_tdef {
	/*! T1234 or X1234 number, corresponding to struct osmo_fsm_inst::T.
	 * Positive values for T are considered to be 3GPP spec compliant and appear in logging and VTY as "T1234",
	 * while negative values are considered to be Osmocom specific timers, represented in logging and VTY as
	 * "X1234". Be aware that osmo_tdef_fsm_inst_state_chg() interprets T == 0 as "state without timeout". */
	const int T;
	/*! Timeout duration (according to unit), default value; type corresponds to osmo_fsm_inst_state_chg()'s
	 * timeout_secs argument. Note that osmo_fsm_inst_state_chg() clamps the range. */
	const unsigned long default_val;
	const enum osmo_tdef_unit unit;
	/*! Human readable description. For unit == OSMO_TDEF_CUSTOM, this should include an explanation of the value's
	 * unit. Best keep this a short one-liner (e.g. for VTY output). */
	const char *desc;
	/*! Currently active timeout value, e.g. set by user config. This is the only mutable member: a user may
	 * configure the timeout value, but neither unit nor any other field. */
	unsigned long val;
	/*! Minimum timer value (in this tdef unit), checked if set (not zero). */
	unsigned long min_val;
	/*! Maximum timer value (in this tdef unit), checked if set (not zero). */
	unsigned long max_val;
};

/*! Iterate an array of struct osmo_tdef, the last item should be fully zero, i.e. "{}".
 * Example:
 *
 *     struct osmo_tdef *t;
 *     osmo_tdef_for_each(t, tdefs) {
 *             printf("%lu %s %s\n", t->val, osmo_tdef_unit_name(t->unit), t->desc);
 *     }
 *
 * \param[inout] t  A struct osmo_tdef *t used for iteration, will point at the current entry inside the loop scope.
 * \param[in] tdefs  Array of struct osmo_tdef to iterate, zero-terminated.
 */
#define osmo_tdef_for_each(t, tdefs) \
	for (t = tdefs; t && (t->T || t->default_val || t->desc); t++)

void osmo_tdefs_reset(struct osmo_tdef *tdefs);
unsigned long osmo_tdef_get(const struct osmo_tdef *tdefs, int T, enum osmo_tdef_unit as_unit,
			    long val_if_not_present);
struct osmo_tdef *osmo_tdef_get_entry(struct osmo_tdef *tdefs, int T);
int osmo_tdef_set(struct osmo_tdef *tdefs, int T, unsigned long val, enum osmo_tdef_unit val_unit);
bool osmo_tdef_val_in_range(struct osmo_tdef *tdef, unsigned long new_val);
int osmo_tdef_range_str_buf(char *buf, size_t buf_len, struct osmo_tdef *t);

/*! Using osmo_tdef for osmo_fsm_inst: array entry for a mapping of state numbers to timeout definitions.
 * For a usage example, see osmo_tdef_get_state_timeout() and test_tdef_state_timeout() in tdef_test.c. */
struct osmo_tdef_state_timeout {
	/*! Timer number to match struct osmo_tdef.T, and to pass to osmo_fsm_inst_state_chg(). Positive values for T
	 * are considered to be 3GPP spec compliant and appear in logging and VTY as "T1234", while negative values are
	 * considered to be Osmocom specific timers, represented in logging and VTY as "X1234". */
	int T;
	/*! If true, call osmo_fsm_inst_state_chg_keep_timer().
	 * If T == 0, keep previous T number, otherwise also set fi->T. */
	bool keep_timer;
};

const struct osmo_tdef_state_timeout *osmo_tdef_get_state_timeout(uint32_t state,
								  const struct osmo_tdef_state_timeout *timeouts_array);

/*! Call osmo_fsm_inst_state_chg[_ms]() or osmo_fsm_inst_state_chg_keep_timer[_ms](),
 * depending on the timeouts_array, tdefs and default_timeout.
 *
 * A timer defined with sub-millisecond precision (e.g OSMO_TDEF_US) is rounded up to the next full millisecond.
 * A timer value defined in units higher than millisecond (e.g. OSMO_TDEF_S, OSMO_TDEF_M) is converted to milliseconds.
 * A timer in unit = OSMO_TDEF_CUSTOM is applied as if the unit is in seconds (i.e. this macro does not make sense
 * for custom units!).
 *
 * See osmo_tdef_get_state_timeout() and osmo_tdef_get().
 *
 * If no T timer is defined for the given state (T == 0), invoke the state change without a timeout.
 *
 * Should a T number be defined in timeouts_array that is not defined in tdefs, use default_timeout (in seconds). If
 * default_timeout is negative, a missing T definition in tdefs instead causes a program abort.
 *
 * This is best used by wrapping this function call in a macro suitable for a specific FSM implementation, which can
 * become as short as: my_fsm_state_chg(fi, NEXT_STATE):
 *
 *     #define my_fsm_state_chg(fi, NEXT_STATE) \
 *             osmo_tdef_fsm_inst_state_chg(fi, NEXT_STATE, my_fsm_timeouts, global_T_defs, 5)
 *
 *     my_fsm_state_chg(fi, MY_FSM_STATE_1);
 *     // -> No timeout configured, will enter state without timeout.
 *
 *     my_fsm_state_chg(fi, MY_FSM_STATE_3);
 *     // T423 configured for this state, will look up T423 in tdefs, or use 5 seconds if unset.
 *
 *     my_fsm_state_chg(fi, MY_FSM_STATE_8);
 *     // keep_timer == true for this state, will invoke osmo_fsm_inst_state_chg_keep_timer().
 *
 * \param[inout] fi  osmo_fsm_inst to transition to another state.
 * \param[in] state  State number to transition to.
 * \param[in] timeouts_array  Array of struct osmo_tdef_state_timeout[32] to look up state in.
 * \param[in] tdefs  Array of struct osmo_tdef (last entry zero initialized) to look up T in.
 * \param[in] default_timeout  If a T is set in timeouts_array, but no timeout value is configured for T,
 *                             then use this default timeout value (in seconds) as fallback,
 *                             or pass a negative number to abort the program.
 * \return Return value from osmo_fsm_inst_state_chg[_ms]() or osmo_fsm_inst_state_chg_keep_timer[_ms]().
 */
#define osmo_tdef_fsm_inst_state_chg(fi, state, timeouts_array, tdefs, default_timeout) \
	_osmo_tdef_fsm_inst_state_chg(fi, state, timeouts_array, tdefs, default_timeout, \
				      __FILE__, __LINE__)
int _osmo_tdef_fsm_inst_state_chg(struct osmo_fsm_inst *fi, uint32_t state,
				  const struct osmo_tdef_state_timeout *timeouts_array,
				  const struct osmo_tdef *tdefs, signed long default_timeout,
				  const char *file, int line);

/*! Manage timer definitions in named groups.
 * This should be defined as an array with the final element kept fully zero-initialized,
 * to be compatible with osmo_tdef_vty* API. There must not be any tdefs == NULL entries except on the final
 * zero-initialized entry. */
struct osmo_tdef_group {
	const char *name;
	const char *desc;
	struct osmo_tdef *tdefs;
};

/*! Iterate an array of struct osmo_tdef_group, the last item should be fully zero, i.e. "{}".
 * \param[inout] g  A struct osmo_tdef_group *g used for iteration, will point at the current entry inside the loop scope.
 * \param[in] tdefs  Array of struct osmo_tdef_group to iterate, zero-terminated.
 */
#define osmo_tdef_groups_for_each(g, tdef_groups) \
	for (g = tdef_groups; g && g->tdefs; g++)

/*! @} */
