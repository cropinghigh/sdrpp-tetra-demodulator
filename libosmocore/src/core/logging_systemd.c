/*
 * (C) 2020 by Vadim Yanitskiy <axilirator@gmail.com>
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

/*! \addtogroup logging
 *  @{
 * \file logging_systemd.c */

#include <stdio.h>
#include <syslog.h>

/* Do not use this file as location in sd_journal_print() */
#define SD_JOURNAL_SUPPRESS_LOCATION

#include <systemd/sd-journal.h>

#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>

/* FIXME: copy-pasted from logging_syslog.c */
static int logp2syslog_level(unsigned int level)
{
	if (level >= LOGL_FATAL)
		return LOG_CRIT;
	else if (level >= LOGL_ERROR)
		return LOG_ERR;
	else if (level >= LOGL_NOTICE)
		return LOG_NOTICE;
	else if (level >= LOGL_INFO)
		return LOG_INFO;
	else
		return LOG_DEBUG;
}

static void _systemd_output(struct log_target *target,
			    unsigned int level, const char *log)
{
	/* systemd accepts the same level constants as syslog */
	sd_journal_print(logp2syslog_level(level), "%s", log);
}

static void _systemd_raw_output(struct log_target *target, int subsys,
				unsigned int level, const char *file,
				int line, int cont, const char *format,
				va_list ap)
{
	char buf[4096];
	int rc;

	rc = vsnprintf(buf, sizeof(buf), format, ap);
	if (rc < 0) {
		sd_journal_print(LOG_ERR, "vsnprintf() failed to render a message "
					  "originated from %s:%d (rc=%d)\n",
					  file, line, rc);
		return;
	}

	sd_journal_send("CODE_FILE=%s, CODE_LINE=%d", file, line,
			"PRIORITY=%d", logp2syslog_level(level),
			"OSMO_SUBSYS=%s", log_category_name(subsys),
			"OSMO_SUBSYS_HEX=%4.4x", subsys,
			"MESSAGE=%s", buf,
			NULL);
}

/*! Create a new logging target for systemd journal logging.
 *  \param[in] raw whether to offload rendering of the meta information
 *		   (location, category) to systemd-journal.
 *  \returns Log target in case of success, NULL in case of error.
 */
struct log_target *log_target_create_systemd(bool raw)
{
	struct log_target *target;

	target = log_target_create();
	if (!target)
		return NULL;

	target->type = LOG_TGT_TYPE_SYSTEMD;
	log_target_systemd_set_raw(target, raw);

	return target;
}

/*! Change meta information handling of an existing logging target.
 *  \param[in] target logging target to be modified.
 *  \param[in] raw whether to offload rendering of the meta information
 *		   (location, category) to systemd-journal.
 */
void log_target_systemd_set_raw(struct log_target *target, bool raw)
{
	target->sd_journal.raw = raw;
	if (raw) {
		target->raw_output = _systemd_raw_output;
		target->output = NULL;
	} else {
		target->output = _systemd_output;
		target->raw_output = NULL;
	}
}

/* @} */
