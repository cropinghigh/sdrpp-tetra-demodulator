#pragma once

/*! \defgroup logging_internal Osmocom logging internals
 *  @{
 * \file logging_internal.h */

#include <osmocom/core/utils.h>

extern void *tall_log_ctx;
extern struct log_info *osmo_log_info;
extern const struct value_string loglevel_strs[];
extern struct llist_head osmo_log_target_list;

void assert_loginfo(const char *src);

/*! @} */
