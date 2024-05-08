/*! 
 * (C) 2015-2017 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * \addtogroup prim
 *  @{
 *  \file prim.c */

#include <osmocom/core/utils.h>
#include <osmocom/core/prim.h>

/*! human-readable string mapping for
 *  \ref osmo_prim_operation */
const struct value_string osmo_prim_op_names[5] = {
	{ PRIM_OP_REQUEST,			"request" },
	{ PRIM_OP_RESPONSE,			"response" },
	{ PRIM_OP_INDICATION,			"indication" },
	{ PRIM_OP_CONFIRM,			"confirm" },
	{ 0, NULL }
};

/*! resolve the (fsm) event for a given primitive using a map
 *  \param[in] oph primitive header used as key for match
 *  \param[in] maps list of mappings from primitive to event
 *  \returns event determined by map; \ref OSMO_NO_EVENT if no match */
uint32_t osmo_event_for_prim(const struct osmo_prim_hdr *oph,
			     const struct osmo_prim_event_map *maps)
{
	const struct osmo_prim_event_map *map;

	for (map = maps; map->event != OSMO_NO_EVENT; map++) {
		if (map->sap == oph->sap &&
		    map->primitive == oph->primitive &&
		    map->operation == oph->operation)
			return map->event;
	}
	return OSMO_NO_EVENT;
}

/*! @} */
