/*! \file loggingrb.c
 * Ringbuffer-backed logging support code. */
/*
 * (C) 2012-2013 by Katerina Barone-Adesi
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

/*! \addtogroup loggingrb
 *  @{
 *  This adds a log which consist of an in-memory ring buffer.  The idea
 *  is that the user can configure his logging in a way that critical
 *  messages get stored in the ring buffer, and that the last few
 *  critical messages can then always obtained by dumping the ring
 *  buffer.  It can hence be used as a more generic version of the
 *  "show me the last N alarms" functionality.
 *
 * \file loggingrb.c */

#include <osmocom/core/strrb.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/loggingrb.h>

static void _rb_output(struct log_target *target,
			  unsigned int level, const char *log)
{
	osmo_strrb_add(target->tgt_rb.rb, log);
}

/*! Return the number of log strings in the osmo_strrb-backed target.
 *  \param[in] target The target to search.
 *
 *  \return The number of log strings in the osmo_strrb-backed target.
 */
size_t log_target_rb_used_size(struct log_target const *target)
{
	return osmo_strrb_elements(target->tgt_rb.rb);
}

/*! Return the capacity of the osmo_strrb-backed target.
 *  \param[in] target The target to search.
 *
 * Note that this is the capacity (aka max number of messages).
 * It is not the number of unused message slots.
 *  \return The number of log strings in the osmo_strrb-backed target.
 */
size_t log_target_rb_avail_size(struct log_target const *target)
{
	struct osmo_strrb *rb = target->tgt_rb.rb;
	return rb->size - 1;
}

/*! Return the nth log entry in a target.
 *  \param[in] target The target to search.
 *  \param[in] logindex The index of the log entry/error message.
 *
 *  \return A pointer to the nth message, or NULL if logindex is invalid.
 */
const char *log_target_rb_get(struct log_target const *target, size_t logindex)
{
	return osmo_strrb_get_nth(target->tgt_rb.rb, logindex);
}

/*! Create a new logging target for ringbuffer-backed logging.
 *  \param[in] size The capacity (number of messages) of the logging target.
 *  \returns A log target in case of success, NULL in case of error.
 */
struct log_target *log_target_create_rb(size_t size)
{
	struct log_target *target;
	struct osmo_strrb *rb;

	target = log_target_create();
	if (!target)
		return NULL;

	rb = osmo_strrb_create(target, size + 1);
	if (!rb) {
		log_target_destroy(target);
		return NULL;
	}

	target->tgt_rb.rb = rb;
	target->type = LOG_TGT_TYPE_STRRB;
	target->output = _rb_output;

	return target;
}

/* @} */
