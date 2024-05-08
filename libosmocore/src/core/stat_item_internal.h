/*! \file stat_item_internal.h
 * internal definitions for the osmo_stat_item API */
#pragma once

/*! \addtogroup osmo_stat_item
 *  @{
 */

struct osmo_stat_item_period {
	/*! Number of osmo_stat_item_set() that occurred during the reporting period, zero if none. */
	uint32_t n;
	/*! Smallest value seen in a reporting period. */
	int32_t min;
	/*! Most recent value passed to osmo_stat_item_set(), or the item->desc->default_value if none. */
	int32_t last;
	/*! Largest value seen in a reporting period. */
	int32_t max;
	/*! Sum of all values passed to osmo_stat_item_set() in the reporting period. */
	int64_t sum;
};

/*! data we keep for each actual item */
struct osmo_stat_item {
	/*! back-reference to the item description */
	const struct osmo_stat_item_desc *desc;

	/*! Current reporting period / current value. */
	struct osmo_stat_item_period value;

	/*! The results of the previous reporting period. According to these, the stats reporter decides whether to
	 * re-send values or omit an unchanged value from a report. */
	struct osmo_stat_item_period reported;
};

/*! @} */
