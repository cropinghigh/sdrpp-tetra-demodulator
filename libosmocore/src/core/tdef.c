/*! \file tdef.c
 * Implementation to define Tnnn timers globally and use for FSM state changes.
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
 */

#include <limits.h>
#include <errno.h>

#include <osmocom/core/fsm.h>
#include <osmocom/core/tdef.h>

/*! \addtogroup Tdef
 *
 * Implementation to define Tnnn timers globally and use for FSM state changes.
 *
 * See also \ref Tdef_VTY
 *
 * osmo_tdef provides:
 *
 * - a list of Tnnnn (GSM) timers with description, unit and default value.
 * - vty UI to allow users to configure non-default timeouts.
 * - API to tie T timers to osmo_fsm states and set them on state transitions.
 *
 * - a few standard units (minute, second, millisecond) as well as a custom unit
 *   (which relies on the timer's human readable description to indicate the
 *   meaning of the value).
 * - conversion for standard units: for example, some GSM timers are defined in
 *   minutes, while our FSM definitions need timeouts in seconds. Conversion is
 *   for convenience only and can be easily avoided via the custom unit.
 *
 * By keeping separate osmo_tdef arrays, several groups of timers can be kept
 * separately. The VTY tests in tests/tdef/ showcase different schemes:
 *
 * - \ref tests/tdef/tdef_vty_config_root_test.c:
 *   Keep several timer definitions in separately named groups: showcase the
 *   osmo_tdef_vty_groups*() API. Each timer group exists exactly once.
 *
 * - \ref tests/tdef/tdef_vty_config_subnode_test.c:
 *   Keep a single list of timers without separate grouping.
 *   Put this list on a specific subnode below the CONFIG_NODE.
 *   There could be several separate subnodes with timers like this, i.e.
 *   continuing from this example, sets of timers could be separated by placing
 *   timers in specific config subnodes instead of using the global group name.
 *
 * - \ref tests/tdef/tdef_vty_dynamic_test.c:
 *   Dynamically allocate timer definitions per each new created object.
 *   Thus there can be an arbitrary number of independent timer definitions, one
 *   per allocated object.
 *
 * osmo_tdef was introduced because:
 *
 * - without osmo_tdef, each invocation of osmo_fsm_inst_state_chg() needs to be
 *   programmed with the right timeout value, for all code paths that invoke this
 *   state change. It is a likely source of errors to get one of them wrong.  By
 *   defining a T timer exactly for an FSM state, the caller can merely invoke the
 *   state change and trust on the original state definition to apply the correct
 *   timeout.
 *
 * - it is helpful to have a standardized config file UI to provide user
 *   configurable timeouts, instead of inventing new VTY commands for each
 *   separate application of T timer numbers. See \ref tdef_vty.h.
 *
 * @{
 * \file tdef.c
 */

/*! a = return_val * b. \return 0 if factor is below 1. */
static unsigned long osmo_tdef_factor(enum osmo_tdef_unit a, enum osmo_tdef_unit b)
{
	if (b == a
	    || b == OSMO_TDEF_CUSTOM || a == OSMO_TDEF_CUSTOM)
		return 1;

	switch (b) {
	case OSMO_TDEF_US:
		switch (a) {
		case OSMO_TDEF_MS:
			return 1000;
		case OSMO_TDEF_S:
			return 1000*1000;
		case OSMO_TDEF_M:
			return 60*1000*1000;
		default:
			return 0;
		}
	case OSMO_TDEF_MS:
		switch (a) {
		case OSMO_TDEF_S:
			return 1000;
		case OSMO_TDEF_M:
			return 60*1000;
		default:
			return 0;
		}
	case OSMO_TDEF_S:
		switch (a) {
		case OSMO_TDEF_M:
			return 60;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

/*! \return val in unit to_unit, rounded up to the next integer value and clamped to ULONG_MAX, or 0 if val == 0. */
static unsigned long osmo_tdef_round(unsigned long val, enum osmo_tdef_unit from_unit, enum osmo_tdef_unit to_unit)
{
	unsigned long f;
	if (!val)
		return 0;

	f = osmo_tdef_factor(from_unit, to_unit);
	if (f == 1)
		return val;
	if (f < 1) {
		f = osmo_tdef_factor(to_unit, from_unit);
		return (val / f) + (val % f? 1 : 0);
	}
	/* range checking */
	if (f > (ULONG_MAX / val))
		return ULONG_MAX;
	return val * f;
}

/*! Set all osmo_tdef values to the default_val.
 * It is convenient to define a tdefs array by setting only the default_val, and calling osmo_tdefs_reset() once for
 * program startup. (See also osmo_tdef_vty_init()).
 * During call to this function, default values are verified to be inside valid range; process is aborted otherwise.
 * \param[in] tdefs  Array of timer definitions, last entry being fully zero.
 */
void osmo_tdefs_reset(struct osmo_tdef *tdefs)
{
	struct osmo_tdef *t;
	osmo_tdef_for_each(t, tdefs) {
		if (!osmo_tdef_val_in_range(t, t->default_val)) {
			char range_str[64];
			osmo_tdef_range_str_buf(range_str, sizeof(range_str), t);
			osmo_panic("%s:%d Timer " OSMO_T_FMT " contains default value %lu not in range %s\n",
				   __FILE__, __LINE__, OSMO_T_FMT_ARGS(t->T), t->default_val, range_str);
		}
		t->val = t->default_val;
	}
}

/*! Return the value of a T timer from a list of osmo_tdef, in the given unit.
 * If no such timer is defined, return the default value passed, or abort the program if default < 0.
 *
 * Round up any value match as_unit: 1100 ms as OSMO_TDEF_S becomes 2 seconds, as OSMO_TDEF_M becomes one minute.
 * However, always return a value of zero as zero (0 ms as OSMO_TDEF_M still is 0 m).
 *
 * Range: even though the value range is unsigned long here, in practice, using ULONG_MAX as value for a timeout in
 * seconds may actually wrap to negative or low timeout values (e.g. in struct timeval). It is recommended to stay below
 * INT_MAX seconds. See also osmo_fsm_inst_state_chg().
 *
 * Usage example:
 *
 * 	struct osmo_tdef global_T_defs[] = {
 * 		{ .T=7, .default_val=50, .desc="Water Boiling Timeout" },  // default is .unit=OSMO_TDEF_S == 0
 * 		{ .T=8, .default_val=300, .desc="Tea brewing" },
 * 		{ .T=9, .default_val=5, .unit=OSMO_TDEF_M, .desc="Let tea cool down before drinking" },
 * 		{ .T=10, .default_val=20, .unit=OSMO_TDEF_M, .desc="Forgot to drink tea while it's warm" },
 * 		{}  //  <-- important! last entry shall be zero
 * 	};
 * 	osmo_tdefs_reset(global_T_defs); // make all values the default
 * 	osmo_tdef_vty_init(global_T_defs, CONFIG_NODE);
 *
 * 	val = osmo_tdef_get(global_T_defs, 7, OSMO_TDEF_S, -1); // -> 50
 * 	sleep(val);
 *
 * 	val = osmo_tdef_get(global_T_defs, 7, OSMO_TDEF_M, -1); // 50 seconds becomes 1 minute -> 1
 * 	sleep_minutes(val);
 *
 * 	val = osmo_tdef_get(global_T_defs, 99, OSMO_TDEF_S, 3); // not defined, returns 3
 *
 * 	val = osmo_tdef_get(global_T_defs, 99, OSMO_TDEF_S, -1); // not defined, program aborts!
 *
 * \param[in] tdefs  Array of timer definitions, last entry must be fully zero initialized.
 * \param[in] T  Timer number to get the value for.
 * \param[in] as_unit  Return timeout value in this unit.
 * \param[in] val_if_not_present  Fallback value to return if no timeout is defined; if this is a negative number, a
 *                                missing T timer definition aborts the program via OSMO_ASSERT().
 * \return Timeout value in the unit given by as_unit, rounded up if necessary, or val_if_not_present.
 *         If val_if_not_present is negative and no T timer is defined, trigger OSMO_ASSERT() and do not return.
 */
unsigned long osmo_tdef_get(const struct osmo_tdef *tdefs, int T, enum osmo_tdef_unit as_unit, long val_if_not_present)
{
	const struct osmo_tdef *t = osmo_tdef_get_entry((struct osmo_tdef*)tdefs, T);
	if (!t) {
		OSMO_ASSERT(val_if_not_present >= 0);
		return val_if_not_present;
	}
	return osmo_tdef_round(t->val, t->unit, as_unit);
}

/*! Find tdef entry matching T.
 * This is useful for manipulation, which is usually limited to the VTY configuration. To retrieve a timeout value,
 * most callers probably should use osmo_tdef_get() instead.
 * \param[in] tdefs  Array of timer definitions, last entry being fully zero.
 * \param[in] T  Timer number to get the entry for.
 * \return osmo_tdef entry matching T in given array, or NULL if no match is found.
 */
struct osmo_tdef *osmo_tdef_get_entry(struct osmo_tdef *tdefs, int T)
{
	struct osmo_tdef *t;
	osmo_tdef_for_each(t, tdefs) {
		if (t->T == T)
			return t;
	}
	return NULL;
}

/*! Set value in entry matching T, converting val from val_unit to unit of T.
 * The converted value is rounded up to the next integer value of T's unit and clamped to ULONG_MAX, or 0 if val == 0.
 * \param[in] tdefs  Array of timer definitions, last entry being fully zero.
 * \param[in] T  Timer number to set the value for.
 * \param[in] val  The new timer value to set.
 * \param[in] val_unit  Units of value in parameter val.
 * \return 0 on success, negative on error.
 */
int osmo_tdef_set(struct osmo_tdef *tdefs, int T, unsigned long val, enum osmo_tdef_unit val_unit)
{
	unsigned long new_val;
	struct osmo_tdef *t = osmo_tdef_get_entry(tdefs, T);
	if (!t)
		return -EEXIST;

	new_val = osmo_tdef_round(val, val_unit, t->unit);
	if (!osmo_tdef_val_in_range(t, new_val))
		return -ERANGE;

	t->val = new_val;
	return 0;
}

/*! Check if value new_val is in range of valid possible values for timer entry tdef.
 * \param[in] tdef  Timer entry from a timer definition table.
 * \param[in] new_val  The value whose validity to check, in units as per this timer entry.
 * \return true if inside range, false otherwise.
 */
bool osmo_tdef_val_in_range(struct osmo_tdef *tdef, unsigned long new_val)
{
	return new_val >= tdef->min_val && (!tdef->max_val || new_val <= tdef->max_val);
}

/*! Write string representation of osmo_tdef range into buf.
 * \param[in] buf  The buffer where the string representation is stored.
 * \param[in] buf_len  Length of buffer in bytes.
 * \param[in] tdef  Timer entry from a timer definition table.
 * \return The number of characters printed on success (or number of characters
 *         which would have been written to the final string if enough space
 *         had been available), negative on error. See snprintf().
 */
int osmo_tdef_range_str_buf(char *buf, size_t buf_len, struct osmo_tdef *t)
{
	int ret, len = 0, offset = 0, rem = buf_len;

	buf[0] = '\0';
	ret = snprintf(buf + offset, rem, "[%lu .. ", t->min_val);
	if (ret < 0)
		return ret;
	OSMO_SNPRINTF_RET(ret, rem, offset, len);

	if (t->max_val)
		ret = snprintf(buf + offset, rem, "%lu]", t->max_val);
	else
		ret = snprintf(buf + offset, rem, "inf]");
	if (ret < 0)
		return ret;
	OSMO_SNPRINTF_RET(ret, rem, offset, len);
	return len;
}

/*! Using osmo_tdef for osmo_fsm_inst: find a given state's osmo_tdef_state_timeout entry.
 *
 * The timeouts_array shall contain exactly 32 elements, regardless whether only some of them are actually populated
 * with nonzero values. 32 corresponds to the number of states allowed by the osmo_fsm_* API. Lookup is by array index.
 * Not populated entries imply a state change invocation without timeout.
 *
 * For example:
 *
 * 	struct osmo_tdef_state_timeout my_fsm_timeouts[32] = {
 * 		[MY_FSM_STATE_3] = { .T = 423 }, // look up timeout configured for T423
 * 		[MY_FSM_STATE_7] = { .keep_timer = true, .T = 235 }, // keep previous timer if running, or start T235
 * 		[MY_FSM_STATE_8] = { .keep_timer = true }, // keep previous state's T number, continue timeout.
 * 		// any state that is omitted will remain zero == no timeout
 *	};
 *	osmo_tdef_get_state_timeout(MY_FSM_STATE_0, &my_fsm_timeouts) -> NULL,
 *	osmo_tdef_get_state_timeout(MY_FSM_STATE_7, &my_fsm_timeouts) -> { .T = 235 }
 *
 * The intention is then to obtain the timer like osmo_tdef_get(global_T_defs, T=235); see also
 * fsm_inst_state_chg_T() below.
 *
 * \param[in] state  State constant to look up.
 * \param[in] timeouts_array  Array[32] of struct osmo_tdef_state_timeout defining which timer number to use per state.
 * \return A struct osmo_tdef_state_timeout entry, or NULL if that entry is zero initialized.
 */
const struct osmo_tdef_state_timeout *osmo_tdef_get_state_timeout(uint32_t state, const struct osmo_tdef_state_timeout *timeouts_array)
{
	const struct osmo_tdef_state_timeout *t;
	OSMO_ASSERT(state < 32);
	t = &timeouts_array[state];
	if (!t->keep_timer && !t->T)
		return NULL;
	return t;
}

/*! See invocation macro osmo_tdef_fsm_inst_state_chg() instead.
 * \param[in] file  Source file name, like __FILE__.
 * \param[in] line  Source file line number, like __LINE__.
 */
int _osmo_tdef_fsm_inst_state_chg(struct osmo_fsm_inst *fi, uint32_t state,
				  const struct osmo_tdef_state_timeout *timeouts_array,
				  const struct osmo_tdef *tdefs, long default_timeout,
				  const char *file, int line)
{
	const struct osmo_tdef_state_timeout *t = osmo_tdef_get_state_timeout(state, timeouts_array);
	unsigned long val_ms = 0;

	/* No timeout defined for this state? */
	if (!t)
		return _osmo_fsm_inst_state_chg(fi, state, 0, 0, file, line);

	if (t->T) {
		const struct osmo_tdef *tdef = osmo_tdef_get_entry((struct osmo_tdef *)tdefs, t->T);
		if (tdef == NULL) {
			/* emulate the old behavior: treat default_timeout as OSMO_TDEF_S */
			OSMO_ASSERT(default_timeout >= 0);
			val_ms = default_timeout * 1000;
		} else {
			val_ms = osmo_tdef_round(tdef->val, tdef->unit, OSMO_TDEF_MS);
			/* emulate the old behavior: treat OSMO_TDEF_CUSTOM as OSMO_TDEF_S */
			if (tdef->unit == OSMO_TDEF_CUSTOM)
				val_ms *= 1000;
		}
	}

	if (t->keep_timer) {
		if (t->T)
			return _osmo_fsm_inst_state_chg_keep_or_start_timer_ms(fi, state, val_ms, t->T, file, line);
		else
			return _osmo_fsm_inst_state_chg_keep_timer(fi, state, file, line);
	}

	/* val_ms is always initialized here, because if t->keep_timer is false, t->T must be != 0.
	 * Otherwise osmo_tdef_get_state_timeout() would have returned NULL. */
	OSMO_ASSERT(t->T);
	return _osmo_fsm_inst_state_chg_ms(fi, state, val_ms, t->T, file, line);
}

const struct value_string osmo_tdef_unit_names[] = {
	{ OSMO_TDEF_S, "s" },
	{ OSMO_TDEF_MS, "ms" },
	{ OSMO_TDEF_M, "m" },
	{ OSMO_TDEF_CUSTOM, "custom-unit" },
	{ OSMO_TDEF_US, "us" },
	{}
};

/*! @} */
