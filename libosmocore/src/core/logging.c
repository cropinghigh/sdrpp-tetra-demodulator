/*! \file logging.c
 * Debugging/Logging support code. */
/*
 * (C) 2008-2010 by Harald Welte <laforge@gnumonks.org>
 * (C) 2008 by Holger Hans Peter Freyther <zecke@selfish.org>
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
 * @{
 * libosmocore Logging sub-system
 *
 * \file logging.c */

#include "osmo_config.h"

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif

#ifdef HAVE_SYSTEMTAP
/* include the generated probes header and put markers in code */
#include "probes.h"
#define TRACE(probe) probe
#define TRACE_ENABLED(probe) probe ## _ENABLED()
#else
/* Wrap the probe to allow it to be removed when no systemtap available */
#define TRACE(probe)
#define TRACE_ENABLED(probe) (0)
#endif /* HAVE_SYSTEMTAP */

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/thread.h>
#include <osmocom/core/select.h>
#include <osmocom/core/write_queue.h>
#include <osmocom/core/gsmtap_util.h>

#include <osmocom/vty/logging.h>	/* for LOGGING_STR. */

/* maximum length of the log string of a single log event (typically  line) */
#define MAX_LOG_SIZE	4096

/* maximum number of log statements we queue in file/stderr target write queue */
#define LOG_WQUEUE_LEN	156

osmo_static_assert(_LOG_CTX_COUNT <= ARRAY_SIZE(((struct log_context*)NULL)->ctx),
		   enum_logging_ctx_items_fit_in_struct_log_context);
osmo_static_assert(_LOG_FLT_COUNT <= ARRAY_SIZE(((struct log_target*)NULL)->filter_data),
		   enum_logging_filters_fit_in_log_target_filter_data);
osmo_static_assert(_LOG_FLT_COUNT <= 8*sizeof(((struct log_target*)NULL)->filter_map),
		   enum_logging_filters_fit_in_log_target_filter_map);

struct log_info *osmo_log_info;

static struct log_context log_context;
void *tall_log_ctx = NULL;
LLIST_HEAD(osmo_log_target_list);

static __thread long int logging_tid;

#if (!EMBEDDED)
/*! One global copy that contains the union of log levels for all targets
*  for all categories, used for quick lock free checks of log targets. */
static volatile uint8_t *log_level_lookup_cache;

/*! Updates cache for all targets for all categies, caller must hold osmo_log_tgt_mutex. */
static void log_cache_update_all(void)
{
	struct log_target *tgt;
	uint8_t tmp_en[osmo_log_info->num_cat];
	uint8_t tmp_level[osmo_log_info->num_cat];

	if (!log_level_lookup_cache)
		return;

	memset(tmp_en, 0, osmo_log_info->num_cat);
	memset(tmp_level, UINT8_MAX, osmo_log_info->num_cat);

	/* values can also decrease.. */
	llist_for_each_entry(tgt, &osmo_log_target_list, entry) {
		for (int i = 0; i < osmo_log_info->num_cat; i++) {
			struct log_category *cat = &tgt->categories[i];
			tmp_en[i] = OSMO_MAX(tmp_en[i], cat->enabled);
			tmp_level[i] = OSMO_MIN(tmp_level[i], cat->loglevel);
			tmp_level[i] = tgt->loglevel ? OSMO_MIN(tmp_level[i], tgt->loglevel) : tmp_level[i];
		}
	}

	for (int i = 0; i < osmo_log_info->num_cat; i++)
		log_level_lookup_cache[i] = tmp_en[i] ? tmp_level[i] : UINT8_MAX;
}

/*! Updates single cache entry, caller must hold osmo_log_tgt_mutex.
 *
 *  \param[in] mapped_subsys plain category index (after mapping)
 *  \param[in] enabled log category enabled?
 *  \param[in] level log level
 */
void log_cache_update(int mapped_subsys, uint8_t enabled, uint8_t level)
{
	struct log_target *tgt;
	struct log_category tmp = { UINT8_MAX, 0 };

	if (!log_level_lookup_cache)
		return;

	/* values can also decrease.. */
	llist_for_each_entry(tgt, &osmo_log_target_list, entry) {
		struct log_category *cat = &tgt->categories[mapped_subsys];
		tmp.enabled = OSMO_MAX(tmp.enabled, cat->enabled);
		tmp.loglevel = OSMO_MIN(tmp.loglevel, cat->loglevel);
		tmp.loglevel = tgt->loglevel ? OSMO_MIN(tmp.loglevel, tgt->loglevel) : tmp.loglevel;
	}
	tmp.enabled = OSMO_MAX(tmp.enabled, enabled);
	tmp.loglevel = OSMO_MIN(tmp.loglevel, level);

	log_level_lookup_cache[mapped_subsys] = tmp.enabled ? tmp.loglevel : UINT8_MAX;
}

/*! Queries log level cache.
 *
 *  \param[in] mapped_subsys plain category index (after mapping)
 *  \param[in] level log level
 *  \returns true if logging should happen for at least one log target
*/
static bool log_cache_check(int mapped_subsys, int level)
{
	if (!log_level_lookup_cache) {
		/* log-cache is not enabled, so we simply behave like we did before the cache */
		return true;
	}

	return (level < log_level_lookup_cache[mapped_subsys]) ? false : true;
}

/*! This mutex must be held while using osmo_log_target_list or any of its
  log_targets in a multithread program. Prevents race conditions between threads
  like producing unordered timestamps or VTY deleting a target while another
  thread is writing to it */
static pthread_mutex_t osmo_log_tgt_mutex;
static bool osmo_log_tgt_mutex_on = false;

/*! Enable multithread support (mutex) in libosmocore logging system.
 * Must be called by processes willing to use logging subsystem from several
 * threads. Once enabled, it's not possible to disable it again.
 */
void log_enable_multithread(void) {
	if (osmo_log_tgt_mutex_on)
		return;
	pthread_mutex_init(&osmo_log_tgt_mutex, NULL);
	osmo_log_tgt_mutex_on = true;
}

/*! Acquire the osmo_log_tgt_mutex. Don't use this function directly, always use
 *  macro log_tgt_mutex_lock() instead.
 */
void log_tgt_mutex_lock_impl(void) {
	/* These lines are useful to debug scenarios where there's only 1 thread
	   and a double lock appears, for instance during startup and some
	   unlock() missing somewhere:
	if (osmo_log_tgt_mutex_on && pthread_mutex_trylock(&osmo_log_tgt_mutex) != 0)
		osmo_panic("acquiring already locked mutex!\n");
	return;
	*/

	if (osmo_log_tgt_mutex_on)
		pthread_mutex_lock(&osmo_log_tgt_mutex);
}

/*! Release the osmo_log_tgt_mutex. Don't use this function directly, always use
 *  macro log_tgt_mutex_unlock() instead.
 */
void log_tgt_mutex_unlock_impl(void) {
	if (osmo_log_tgt_mutex_on)
		pthread_mutex_unlock(&osmo_log_tgt_mutex);
}

#else /* if (!EMBEDDED) */
#pragma message ("logging multithread support disabled in embedded build")
void log_enable_multithread(void) {}
void log_tgt_mutex_lock_impl(void) {}
void log_tgt_mutex_unlock_impl(void) {}
#endif /* if (!EMBEDDED) */

const struct value_string loglevel_strs[] = {
	{ LOGL_DEBUG,	"DEBUG" },
	{ LOGL_INFO,	"INFO" },
	{ LOGL_NOTICE,	"NOTICE" },
	{ LOGL_ERROR,	"ERROR" },
	{ LOGL_FATAL,	"FATAL" },
	{ 0, NULL },
};

/* 256 color palette see https://en.wikipedia.org/wiki/ANSI_escape_code#8-bit */
#define INT2IDX(x)	(-1*(x)-1)
static const struct log_info_cat internal_cat[OSMO_NUM_DLIB] = {
	[INT2IDX(DLGLOBAL)] = {	/* -1 becomes 0 */
		.name = "DLGLOBAL",
		.description = "Library-internal global log family",
		.loglevel = LOGL_NOTICE,
		.enabled = 1,
	},
	[INT2IDX(DLLAPD)] = {	/* -2 becomes 1 */
		.name = "DLLAPD",
		.description = "LAPD in libosmogsm",
		.loglevel = LOGL_NOTICE,
		.enabled = 1,
		.color = "\033[38;5;12m",
	},
	[INT2IDX(DLINP)] = {
		.name = "DLINP",
		.description = "A-bis Intput Subsystem",
		.loglevel = LOGL_NOTICE,
		.enabled = 1,
		.color = "\033[38;5;23m",
	},
	[INT2IDX(DLMUX)] = {
		.name = "DLMUX",
		.description = "A-bis B-Subchannel TRAU Frame Multiplex",
		.loglevel = LOGL_NOTICE,
		.enabled = 1,
		.color = "\033[38;5;25m",
	},
	[INT2IDX(DLMI)] = {
		.name = "DLMI",
		.description = "A-bis Input Driver for Signalling",
		.enabled = 0, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;27m",
	},
	[INT2IDX(DLMIB)] = {
		.name = "DLMIB",
		.description = "A-bis Input Driver for B-Channels (voice)",
		.enabled = 0, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;29m",
	},
	[INT2IDX(DLSMS)] = {
		.name = "DLSMS",
		.description = "Layer3 Short Message Service (SMS)",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;31m",
	},
	[INT2IDX(DLCTRL)] = {
		.name = "DLCTRL",
		.description = "Control Interface",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;33m",
	},
	[INT2IDX(DLGTP)] = {
		.name = "DLGTP",
		.description = "GPRS GTP library",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;35m",
	},
	[INT2IDX(DLSTATS)] = {
		.name = "DLSTATS",
		.description = "Statistics messages and logging",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;37m",
	},
	[INT2IDX(DLGSUP)] = {
		.name = "DLGSUP",
		.description = "Generic Subscriber Update Protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;39m",
	},
	[INT2IDX(DLOAP)] = {
		.name = "DLOAP",
		.description = "Osmocom Authentication Protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;41m",
	},
	[INT2IDX(DLSS7)] = {
		.name = "DLSS7",
		.description = "libosmo-sigtran Signalling System 7",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;43m",
	},
	[INT2IDX(DLSCCP)] = {
		.name = "DLSCCP",
		.description = "libosmo-sigtran SCCP Implementation",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;45m",
	},
	[INT2IDX(DLSUA)] = {
		.name = "DLSUA",
		.description = "libosmo-sigtran SCCP User Adaptation",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;47m",
	},
	[INT2IDX(DLM3UA)] = {
		.name = "DLM3UA",
		.description = "libosmo-sigtran MTP3 User Adaptation",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;49m",
	},
	[INT2IDX(DLMGCP)] = {
		.name = "DLMGCP",
		.description = "libosmo-mgcp Media Gateway Control Protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;51m",
	},
	[INT2IDX(DLJIBUF)] = {
		.name = "DLJIBUF",
		.description = "libosmo-netif Jitter Buffer",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;53m",
	},
	[INT2IDX(DLRSPRO)] = {
		.name = "DLRSPRO",
		.description = "Remote SIM protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;55m",
	},
	[INT2IDX(DLNS)] = {
		.name = "DLNS",
		.description = "GPRS NS layer",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;57m",
	},
	[INT2IDX(DLBSSGP)] = {
		.name = "DLBSSGP",
		.description = "GPRS BSSGP layer",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;59m",
	},
	[INT2IDX(DLNSDATA)] = {
		.name = "DLNSDATA",
		.description = "GPRS NS layer data PDU",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;61m",
	},
	[INT2IDX(DLNSSIGNAL)] = {
		.name = "DLNSSIGNAL",
		.description = "GPRS NS layer signal PDU",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;63m",
	},
	[INT2IDX(DLIUUP)] = {
		.name = "DLIUUP",
		.description = "Iu UP layer",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;65m",
	},
	[INT2IDX(DLPFCP)] = {
		.name = "DLPFCP",
		.description = "libosmo-pfcp Packet Forwarding Control Protocol",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;51m",
	},
	[INT2IDX(DLCSN1)] = {
		.name = "DLCSN1",
		.description = "libosmo-csn1 Concrete Syntax Notation 1 codec",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;11m",
	},
	[INT2IDX(DLIO)] = {
		.name = "DLIO",
		.description = "libosmocore IO Subsystem",
		.enabled = 1, .loglevel = LOGL_NOTICE,
		.color = "\033[38;5;67m",
	},
};

void assert_loginfo(const char *src)
{
	if (!osmo_log_info) {
		fprintf(stderr, "ERROR: osmo_log_info == NULL! "
			"You must call log_init() before using logging in %s()!\n", src);
		OSMO_ASSERT(osmo_log_info);
	}
}

/* special magic for negative (library-internal) log subsystem numbers */
static int subsys_lib2index(int subsys)
{
	return (subsys * -1) + (osmo_log_info->num_cat_user-1);
}

/*! Parse a human-readable log level into a numeric value
 *  \param[in] lvl zero-terminated string containing log level name
 *  \returns numeric log level
 */
int log_parse_level(const char *lvl)
{
	return get_string_value(loglevel_strs, lvl);
}

/*! convert a numeric log level into human-readable string
 *  \param[in] lvl numeric log level
 *  \returns zero-terminated string (log level name)
 */
const char *log_level_str(unsigned int lvl)
{
	return get_value_string(loglevel_strs, lvl);
}

/*! parse a human-readable log category into numeric form
 *  \param[in] category human-readable log category name
 *  \returns numeric category value, or -EINVAL otherwise
 */
int log_parse_category(const char *category)
{
	int i;

	assert_loginfo(__func__);

	for (i = 0; i < osmo_log_info->num_cat; ++i) {
		if (osmo_log_info->cat[i].name == NULL)
			continue;
		if (!strcasecmp(osmo_log_info->cat[i].name+1, category))
			return i;
	}

	return -EINVAL;
}

/*! parse the log category mask
 *  \param[in] target log target to be configured
 *  \param[in] _mask log category mask string
 *
 * The format can be this: category1:category2:category3
 * or category1,2:category2,3:...
 */
void log_parse_category_mask(struct log_target* target, const char *_mask)
{
	int i = 0;
	char *mask = strdup(_mask);
	char *category_token = NULL;

	assert_loginfo(__func__);

	/* Disable everything to enable it afterwards */
	for (i = 0; i < osmo_log_info->num_cat; ++i)
		target->categories[i].enabled = 0;

	category_token = strtok(mask, ":");
	OSMO_ASSERT(category_token);
	do {
		for (i = 0; i < osmo_log_info->num_cat; ++i) {
			size_t length, cat_length;
			char* colon = strstr(category_token, ",");

			if (!osmo_log_info->cat[i].name)
				continue;

			length = strlen(category_token);
			cat_length = strlen(osmo_log_info->cat[i].name);

			/* Use longest length not to match subocurrences. */
			if (cat_length > length)
				length = cat_length;

			if (colon)
			    length = colon - category_token;

			if (strncasecmp(osmo_log_info->cat[i].name,
					category_token, length) == 0) {
				int level = 0;

				if (colon)
					level = atoi(colon+1);

				target->categories[i].enabled = 1;
				target->categories[i].loglevel = level;
			}
		}
	} while ((category_token = strtok(NULL, ":")));

#if !defined(EMBEDDED)
	log_cache_update_all();
#endif

	free(mask);
}

static const char* color(int subsys)
{
	if (subsys < osmo_log_info->num_cat)
		return osmo_log_info->cat[subsys].color;

	return NULL;
}

static const struct value_string level_colors[] = {
	{ LOGL_DEBUG, OSMO_LOGCOLOR_BLUE },
	{ LOGL_INFO, OSMO_LOGCOLOR_GREEN },
	{ LOGL_NOTICE, OSMO_LOGCOLOR_YELLOW },
	{ LOGL_ERROR, OSMO_LOGCOLOR_RED },
	{ LOGL_FATAL, OSMO_LOGCOLOR_RED },
	{ 0, NULL }
};

static const char *level_color(int level)
{
	const char *c = get_value_string_or_null(level_colors, level);
	if (!c)
		return get_value_string(level_colors, LOGL_FATAL);
	return c;
}

const char* log_category_name(int subsys)
{
	if (subsys < osmo_log_info->num_cat)
		return osmo_log_info->cat[subsys].name;

	return NULL;
}

static const char *const_basename(const char *path)
{
	const char *bn = strrchr(path, '/');
	if (!bn || !bn[1])
		return path;
	return bn + 1;
}

/*! main output formatting function for log lines.
 *  \param[out] buf caller-allocated output buffer for the generated string
 *  \param[in] buf_len number of bytes available in buf
 *  \param[in] target log target for which the string is to be formatted
 *  \param[in] subsys Log sub-system number
 *  \param[in] level Log level
 *  \param[in] file name of source code file generating the log
 *  \param[in] line line source code line number within 'file' generating the log
 *  \param[in] cont is this a continuation (true) or not (false)
 *  \param[in] format format string
 *  \param[in] ap variable argument list for format
 *  \returns number of bytes written to out */
static int _output_buf(char *buf, int buf_len, struct log_target *target, unsigned int subsys,
			unsigned int level, const char *file, int line, int cont,
			const char *format, va_list ap)
{
	int ret;
	const char *c_subsys = NULL;
	struct osmo_strbuf sb = { .buf = buf, .pos = buf, .len = buf_len };

	/* safety net in case of encountering errors and returning nothing */
	buf[0] = '\0';

	/* are we using color */
	if (target->use_color) {
		c_subsys = color(subsys);
		if (c_subsys)
			OSMO_STRBUF_PRINTF(sb, "%s", c_subsys);
	}
	if (!cont) {
		if (target->print_ext_timestamp) {
#ifdef HAVE_LOCALTIME_R
			struct tm tm;
			struct timeval tv;
			osmo_gettimeofday(&tv, NULL);
			localtime_r(&tv.tv_sec, &tm);
			OSMO_STRBUF_PRINTF(sb, "%04d%02d%02d%02d%02d%02d%03d ",
					   tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
					   tm.tm_hour, tm.tm_min, tm.tm_sec,
					   (int)(tv.tv_usec / 1000));
#endif
		} else if (target->print_timestamp) {
			time_t tm;
			if ((tm = time(NULL)) == (time_t) -1)
				goto err;
			/* Get human-readable representation of time.
			   man ctime: we need at least 26 bytes in buf */
			if (OSMO_STRBUF_REMAIN(sb) < 26 || !ctime_r(&tm, sb.pos))
				goto err;
			ret = strnlen(sb.pos, 26);
			if (ret <= 0)
				goto err;
			OSMO_STRBUF_ADDED_TAIL(sb, ret);
			/* Get rid of useless final '\n' added by ctime_r. We want a space instead. */
			OSMO_STRBUF_DROP_TAIL(sb, 1);
			OSMO_STRBUF_PRINTF(sb, " ");
		}
		if (target->print_tid) {
			if (logging_tid == 0)
				logging_tid = (long int)osmo_gettid();
			OSMO_STRBUF_PRINTF(sb, "%ld ", logging_tid);
		}
		if (target->print_category)
			OSMO_STRBUF_PRINTF(sb, "%s%s%s%s ",
					   target->use_color ? level_color(level) : "",
					   log_category_name(subsys),
					   target->use_color ? OSMO_LOGCOLOR_END : "",
					   c_subsys ? c_subsys : "");
		if (target->print_level)
			OSMO_STRBUF_PRINTF(sb, "%s%s%s%s ",
					   target->use_color ? level_color(level) : "",
					   log_level_str(level),
					   target->use_color ? OSMO_LOGCOLOR_END : "",
					   c_subsys ? c_subsys : "");
		if (target->print_category_hex)
			OSMO_STRBUF_PRINTF(sb, "<%4.4x> ", subsys);

		if (target->print_filename_pos == LOG_FILENAME_POS_HEADER_END) {
			switch (target->print_filename2) {
			case LOG_FILENAME_NONE:
				break;
			case LOG_FILENAME_PATH:
				OSMO_STRBUF_PRINTF(sb, "%s:%d ", file, line);
				break;
			case LOG_FILENAME_BASENAME:
				OSMO_STRBUF_PRINTF(sb, "%s:%d ", const_basename(file), line);
				break;
			}
		}
	}
	OSMO_STRBUF_APPEND(sb, vsnprintf, format, ap);

	/* For LOG_FILENAME_POS_LAST, print the source file info only when the caller ended the log
	 * message in '\n'. If so, nip the last '\n' away, insert the source file info and re-append an
	 * '\n'. All this to allow LOGP("start..."); LOGPC("...end\n") constructs. */
	if (target->print_filename_pos == LOG_FILENAME_POS_LINE_END
	    && sb.pos > sb.buf && *(sb.pos - 1) == '\n') {
		switch (target->print_filename2) {
		case LOG_FILENAME_NONE:
			break;
		case LOG_FILENAME_PATH:
			OSMO_STRBUF_DROP_TAIL(sb, 1);
			OSMO_STRBUF_PRINTF(sb, " (%s:%d)\n", file, line);
			break;
		case LOG_FILENAME_BASENAME:
			OSMO_STRBUF_DROP_TAIL(sb, 1);
			OSMO_STRBUF_PRINTF(sb, " (%s:%d)\n", const_basename(file), line);
			break;
		}
	}

	if (target->use_color && c_subsys) {
		/* Ensure the last color escape is sent before the newline
		* (to not clobber journald, which works on single-lines only) */
		if (sb.pos > sb.buf && *(sb.pos - 1) == '\n') {
			OSMO_STRBUF_DROP_TAIL(sb, 1);
			OSMO_STRBUF_PRINTF(sb, OSMO_LOGCOLOR_END "\n");
		} else {
			OSMO_STRBUF_PRINTF(sb, OSMO_LOGCOLOR_END);
		}
	}
err:
	return OSMO_STRBUF_CHAR_COUNT(sb);
}

/* Format the log line for given target; use a stack buffer and call target->output */
static void _output(struct log_target *target, unsigned int subsys,
		    unsigned int level, const char *file, int line, int cont,
		    const char *format, va_list ap)
{
	char buf[MAX_LOG_SIZE];
	int rc;

	rc = _output_buf(buf, sizeof(buf), target, subsys, level, file, line, cont, format, ap);
	if (rc > 0)
		target->output(target, level, buf);
}

/* Catch internal logging category indexes as well as out-of-bounds indexes.
 * For internal categories, the ID is negative starting with -1; and internal
 * logging categories are added behind the user categories. For out-of-bounds
 * indexes, return the index of DLGLOBAL. The returned category index is
 * guaranteed to exist in osmo_log_info, otherwise the program would abort,
 * which should never happen unless even the DLGLOBAL category is missing. */
static inline int map_subsys(int subsys)
{
	/* Note: comparing signed and unsigned integers */

	if (subsys > 0 && ((unsigned int)subsys) >= osmo_log_info->num_cat_user)
		subsys = DLGLOBAL;

	if (subsys < 0)
		subsys = subsys_lib2index(subsys);

	if (subsys < 0 || subsys >= osmo_log_info->num_cat)
		subsys = subsys_lib2index(DLGLOBAL);

	OSMO_ASSERT(!(subsys < 0 || subsys >= osmo_log_info->num_cat));

	return subsys;
}

static inline bool should_log_to_target(struct log_target *tar, int subsys,
					int level)
{
	struct log_category *category;

	category = &tar->categories[subsys];

	/* subsystem is not supposed to be logged */
	if (!category->enabled)
		return false;

	/* Check the global log level */
	if (tar->loglevel != 0 && level < tar->loglevel)
		return false;

	/* Check the category log level */
	if (tar->loglevel == 0 && category->loglevel != 0 &&
	    level < category->loglevel)
		return false;

	/* Apply filters here... if that becomes messy we will
	 * need to put filters in a list and each filter will
	 * say stop, continue, output */
	if ((tar->filter_map & (1 << LOG_FLT_ALL)) != 0)
		return true;

	if (osmo_log_info->filter_fn)
		return osmo_log_info->filter_fn(&log_context, tar);

	/* TODO: Check the filter/selector too? */
	return true;
}

/*! vararg version of logging function
 *  \param[in] subsys Logging sub-system
 *  \param[in] level Log level
 *  \param[in] file name of source code file
 *  \param[in] cont continuation (1) or new line (0)
 *  \param[in] format format string
 *  \param[in] ap vararg-list containing format string arguments
 */
void osmo_vlogp(int subsys, int level, const char *file, int line,
		int cont, const char *format, va_list ap)
{
	struct log_target *tar;

	subsys = map_subsys(subsys);

#if !defined(EMBEDDED)
	if (!log_cache_check(subsys, level))
		return;
#endif

	log_tgt_mutex_lock();

	llist_for_each_entry(tar, &osmo_log_target_list, entry) {
		va_list bp;

		if (!should_log_to_target(tar, subsys, level))
			continue;

		/* According to the manpage, vsnprintf leaves the value of ap
		 * in undefined state. Since _output uses vsnprintf and it may
		 * be called several times, we have to pass a copy of ap. */
		va_copy(bp, ap);
		if (tar->raw_output)
			tar->raw_output(tar, subsys, level, file, line, cont, format, bp);
		else
			_output(tar, subsys, level, file, line, cont, format, bp);
		va_end(bp);
	}

	log_tgt_mutex_unlock();
}

/*! logging function used by DEBUGP() macro
 *  \param[in] subsys Logging sub-system
 *  \param[in] file name of source code file
 *  \param[in] cont continuation (1) or new line (0)
 *  \param[in] format format string
 */
void logp(int subsys, const char *file, int line, int cont,
	  const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	osmo_vlogp(subsys, LOGL_DEBUG, file, line, cont, format, ap);
	va_end(ap);
}

/*! logging function used by LOGP() macro
 *  \param[in] subsys Logging sub-system
 *  \param[in] level Log level
 *  \param[in] file name of source code file
 *  \param[in] cont continuation (1) or new line (0)
 *  \param[in] format format string
 */
void logp2(int subsys, unsigned int level, const char *file, int line, int cont, const char *format, ...)
{
	va_list ap;

	TRACE(LIBOSMOCORE_LOG_START());
	va_start(ap, format);
	osmo_vlogp(subsys, level, file, line, cont, format, ap);
	va_end(ap);
	TRACE(LIBOSMOCORE_LOG_DONE());
}

/* This logging function is used as a fallback when the logging framework is
 * not is not properly initialized. */
void logp_stub(const char *file, int line, int cont, const char *format, ...)
{
	va_list ap;
	if (!cont)
		fprintf(stderr, "%s:%d ", file, line);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}

/*! Register a new log target with the logging core
 *  \param[in] target Log target to be registered
 */
void log_add_target(struct log_target *target)
{
	llist_add_tail(&target->entry, &osmo_log_target_list);
#if (!EMBEDDED)
	log_cache_update_all();
#endif
}

/*! Unregister a log target from the logging core
 *  \param[in] target Log target to be unregistered
 */
void log_del_target(struct log_target *target)
{
	llist_del(&target->entry);
#if (!EMBEDDED)
	log_cache_update_all();
#endif
}

/*! Reset (clear) the logging context */
void log_reset_context(void)
{
	memset(&log_context, 0, sizeof(log_context));
}

/*! Set the logging context
 *  \param[in] ctx_nr logging context number
 *  \param[in] value value to which the context is to be set
 *  \returns 0 in case of success; negative otherwise
 *
 * A logging context is something like the subscriber identity to which
 * the currently processed message relates, or the BTS through which it
 * was received.  As soon as this data is known, it can be set using
 * this function.  The main use of context information is for logging
 * filters.
 */
int log_set_context(uint8_t ctx_nr, void *value)
{
	if (ctx_nr > LOG_MAX_CTX)
		return -EINVAL;

	log_context.ctx[ctx_nr] = value;

	return 0;
}

/*! Enable the \ref LOG_FLT_ALL log filter
 *  \param[in] target Log target to be affected
 *  \param[in] all enable (1) or disable (0) the ALL filter
 *
 * When the \ref LOG_FLT_ALL filter is enabled, all log messages will be
 * printed.  It acts as a wildcard.  Setting it to \a 1 means there is no
 * filtering.
 */
void log_set_all_filter(struct log_target *target, int all)
{
	if (all)
		target->filter_map |= (1 << LOG_FLT_ALL);
	else
		target->filter_map &= ~(1 << LOG_FLT_ALL);
}

/*! Enable or disable the use of colored output
 *  \param[in] target Log target to be affected
 *  \param[in] use_color Use color (1) or don't use color (0)
 */
void log_set_use_color(struct log_target *target, int use_color)
{
	target->use_color = use_color;
}

/*! Enable or disable printing of timestamps while logging
 *  \param[in] target Log target to be affected
 *  \param[in] print_timestamp Enable (1) or disable (0) timestamps
 */
void log_set_print_timestamp(struct log_target *target, int print_timestamp)
{
	target->print_timestamp = print_timestamp;
}

/*! Enable or disable printing of extended timestamps while logging
 *  \param[in] target Log target to be affected
 *  \param[in] print_timestamp Enable (1) or disable (0) timestamps
 *
 * When both timestamp and extended timestamp is enabled then only
 * the extended timestamp will be used. The format of the timestamp
 * is YYYYMMDDhhmmssnnn.
 */
void log_set_print_extended_timestamp(struct log_target *target, int print_timestamp)
{
	target->print_ext_timestamp = print_timestamp;
}

/*! Enable or disable printing of timestamps while logging
 *  \param[in] target Log target to be affected
 *  \param[in] print_tid Enable (1) or disable (0) Thread ID logging
 */
void log_set_print_tid(struct log_target *target, int print_tid)
{
	target->print_tid = print_tid;
}

/*! Use log_set_print_filename2() instead.
 * Call log_set_print_filename2() with LOG_FILENAME_PATH or LOG_FILENAME_NONE, *as well as* call
 * log_set_print_category_hex() with the argument passed to this function. This is to mirror legacy
 * behavior, which combined the category in hex with the filename. For example, if the category-hex
 * output were no longer affected by log_set_print_filename(), many unit tests (in libosmocore as well as
 * dependent projects) would fail since they expect the category to disappear along with the filename.
 *  \param[in] target Log target to be affected
 *  \param[in] print_filename Enable (1) or disable (0) filenames
 */
void log_set_print_filename(struct log_target *target, int print_filename)
{
	log_set_print_filename2(target, print_filename ? LOG_FILENAME_PATH : LOG_FILENAME_NONE);
	log_set_print_category_hex(target, print_filename);
}

/*! Enable or disable printing of the filename while logging.
 *  \param[in] target Log target to be affected.
 *  \param[in] lft An LOG_FILENAME_* enum value.
 * LOG_FILENAME_NONE omits the source file and line information from logs.
 * LOG_FILENAME_PATH prints the entire source file path as passed to LOGP macros.
 */
void log_set_print_filename2(struct log_target *target, enum log_filename_type lft)
{
	target->print_filename2 = lft;
}

/*! Set the position where on a log line the source file info should be logged.
 *  \param[in] target Log target to be affected.
 *  \param[in] pos A LOG_FILENAME_POS_* enum value.
 * LOG_FILENAME_POS_DEFAULT logs just before the caller supplied log message.
 * LOG_FILENAME_POS_LAST logs only at the end of a log line, where the caller issued an '\n' to end the
 */
void log_set_print_filename_pos(struct log_target *target, enum log_filename_pos pos)
{
	target->print_filename_pos = pos;
}

/*! Enable or disable printing of the category name
 *  \param[in] target Log target to be affected
 *  \param[in] print_category Enable (1) or disable (0) filenames
 *
 *  Print the category/subsys name in front of every log message.
 */
void log_set_print_category(struct log_target *target, int print_category)
{
	target->print_category = print_category;
}

/*! Enable or disable printing of the category number in hex ('<000b>').
 *  \param[in] target Log target to be affected.
 *  \param[in] print_category_hex Enable (1) or disable (0) hex category.
 */
void log_set_print_category_hex(struct log_target *target, int print_category_hex)
{
	target->print_category_hex = print_category_hex;
}

/*! Enable or disable printing of the log level name.
 *  \param[in] target Log target to be affected
 *  \param[in] print_level Enable (1) or disable (0) log level name
 *
 *  Print the log level name in front of every log message.
 */
void log_set_print_level(struct log_target *target, int print_level)
{
	target->print_level = (bool)print_level;
}

/*! Set the global log level for a given log target
 *  \param[in] target Log target to be affected
 *  \param[in] log_level New global log level
 */
void log_set_log_level(struct log_target *target, int log_level)
{
	target->loglevel = log_level;
#if !defined(EMBEDDED)
	log_cache_update_all();
#endif
}

/*! Set a category filter on a given log target
 *  \param[in] target Log target to be affected
 *  \param[in] category Log category to be affected
 *  \param[in] enable whether to enable or disable the filter
 *  \param[in] level Log level of the filter
 */
void log_set_category_filter(struct log_target *target, int category,
			       int enable, int level)
{
	if (!target)
		return;
	category = map_subsys(category);
	target->categories[category].enabled = !!enable;
	target->categories[category].loglevel = level;

#if !defined(EMBEDDED)
	log_cache_update(category, !!enable, level);
#endif
}

#if (!EMBEDDED)
/* write-queue tells us we should write another msgb (log line) to the output fd */
static int _file_wq_write_cb(struct osmo_fd *ofd, struct msgb *msg)
{
	int rc;

	rc = write(ofd->fd, msgb_data(msg), msgb_length(msg));
	if (rc < 0)
		return rc;
	if (rc != msgb_length(msg)) {
		/* pull the number of bytes we have already written */
		msgb_pull(msg, rc);
		/* ask write_queue to re-insert the msgb at the head of the queue */
		return -EAGAIN;
	}
	return 0;
}

/* output via buffered, blocking stdio streams */
static void _file_output_stream(struct log_target *target, unsigned int level,
			 const char *log)
{
	OSMO_ASSERT(target->tgt_file.out);
	fputs(log, target->tgt_file.out);
	fflush(target->tgt_file.out);
}

/* output via non-blocking write_queue, doing internal buffering */
static void _file_raw_output(struct log_target *target, int subsys, unsigned int level, const char *file,
			     int line, int cont, const char *format, va_list ap)
{
	struct msgb *msg;
	int rc;

	OSMO_ASSERT(target->tgt_file.wqueue);
	msg = msgb_alloc_c(target->tgt_file.wqueue, MAX_LOG_SIZE, "log_file_msg");
	if (!msg)
		return;

	/* we simply enqueue the log message to a write queue here, to avoid any blocking
	 * writes on the output file.  The write queue will tell us once the file is writable
	 * and call _file_wq_write_cb() */
	rc = _output_buf((char *)msgb_data(msg), msgb_tailroom(msg), target, subsys, level, file, line, cont, format, ap);
	msgb_put(msg, rc);

	/* attempt a synchronous, non-blocking write, if the write queue is empty */
	if (target->tgt_file.wqueue->current_length == 0) {
		rc = _file_wq_write_cb(&target->tgt_file.wqueue->bfd, msg);
		if (rc == 0) {
			/* the write was complete, we can exit early */
			msgb_free(msg);
			return;
		}
	}
	/* if we reach here, either we already had elements in the write_queue, or the synchronous write
	 * failed: enqueue the message to the write_queue (backlog) */
	if (osmo_wqueue_enqueue_quiet(target->tgt_file.wqueue, msg) < 0) {
		msgb_free(msg);
		/* TODO: increment some counter so we can see that messages were dropped */
	}
}
#endif

/*! Create a new log target skeleton
 *  \returns dynamically-allocated log target
 *  This funcition allocates a \ref log_target and initializes it
 *  with some default values.  The newly created target is not
 *  registered yet.
 */
struct log_target *log_target_create(void)
{
	struct log_target *target;
	unsigned int i;

	assert_loginfo(__func__);

	target = talloc_zero(tall_log_ctx, struct log_target);
	if (!target)
		return NULL;

	target->categories = talloc_zero_array(target,
						struct log_category,
						osmo_log_info->num_cat);
	if (!target->categories) {
		talloc_free(target);
		return NULL;
	}

	INIT_LLIST_HEAD(&target->entry);

	/* initialize the per-category enabled/loglevel from defaults */
	for (i = 0; i < osmo_log_info->num_cat; i++) {
		struct log_category *cat = &target->categories[i];
		cat->enabled = osmo_log_info->cat[i].enabled;
		cat->loglevel = osmo_log_info->cat[i].loglevel;
	}

	/* global settings */
	target->use_color = 1;
	target->print_timestamp = 0;
	target->print_tid = 0;
	target->print_filename2 = LOG_FILENAME_PATH;
	target->print_category_hex = true;

	/* global log level */
	target->loglevel = 0;

#if !defined(EMBEDDED)
	/* update cache */
	for (i = 0; i < osmo_log_info->num_cat; i++) {
		const struct log_info_cat *c = &osmo_log_info->cat[i];
		log_cache_update(i, c->enabled, c->loglevel);
	}
#endif

	return target;
}

/*! Create the STDERR log target
 *  \returns dynamically-allocated \ref log_target for STDERR */
struct log_target *log_target_create_stderr(void)
{
/* since C89/C99 says stderr is a macro, we can safely do this! */
#if !EMBEDDED && defined(stderr)
	struct log_target *target;

	target = log_target_create();
	if (!target)
		return NULL;

	target->type = LOG_TGT_TYPE_STDERR;
	target->tgt_file.out = stderr;
	target->output = _file_output_stream;
	return target;
#else
	return NULL;
#endif /* stderr */
}

#if (!EMBEDDED)
/*! Create a new file-based log target using buffered, blocking stream output
 *  \param[in] fname File name of the new log file
 *  \returns Log target in case of success, NULL otherwise
 */
struct log_target *log_target_create_file_stream(const char *fname)
{
	struct log_target *target;

	target = log_target_create();
	if (!target)
		return NULL;

	target->type = LOG_TGT_TYPE_FILE;
	target->tgt_file.out = fopen(fname, "a");
	if (!target->tgt_file.out) {
		log_target_destroy(target);
		return NULL;
	}
	target->output = _file_output_stream;
	target->tgt_file.fname = talloc_strdup(target, fname);

	return target;
}

/*! switch from non-blocking/write-queue to blocking + buffered stream output
 *  \param[in] target log target which we should switch
 *  \return 0 on success; 1 if already switched before; negative on error
 *  Must be called with mutex osmo_log_tgt_mutex held, see log_tgt_mutex_lock.
 */
int log_target_file_switch_to_stream(struct log_target *target)
{
	struct osmo_wqueue *wq;

	if (!target)
		return -ENODEV;

	if (target->tgt_file.out) {
		/* target has already been switched over */
		return 1;
	}

	wq = target->tgt_file.wqueue;
	OSMO_ASSERT(wq);

	/* re-open output as stream */
	if (target->type == LOG_TGT_TYPE_STDERR)
		target->tgt_file.out = stderr;
	else
		target->tgt_file.out = fopen(target->tgt_file.fname, "a");
	if (!target->tgt_file.out) {
		return -EIO;
	}

	/* synchronously write anything left in the queue */
	while (!llist_empty(&wq->msg_queue)) {
		struct msgb *msg = msgb_dequeue(&wq->msg_queue);
		fwrite(msgb_data(msg), msgb_length(msg), 1, target->tgt_file.out);
		msgb_free(msg);
	}

	/* now that everything succeeded, we can finally close the old output fd */
	if (target->type == LOG_TGT_TYPE_FILE) {
		osmo_fd_unregister(&wq->bfd);
		close(wq->bfd.fd);
		wq->bfd.fd = -1;
	}

	/* release the queue itself */
	talloc_free(wq);
	target->tgt_file.wqueue = NULL;
	target->output = _file_output_stream;
	target->raw_output = NULL;

	return 0;
}

/*! switch from blocking + buffered file output to non-blocking write-queue based output.
 *  \param[in] target log target which we should switch
 *  \return 0 on success; 1 if already switched before; negative on error
 *  Must be called with mutex osmo_log_tgt_mutex held, see log_tgt_mutex_lock.
 */
int log_target_file_switch_to_wqueue(struct log_target *target)
{
	struct osmo_wqueue *wq;
	int rc;

	if (!target)
		return -ENODEV;

	if (!target->tgt_file.out) {
		/* target has already been switched over */
		return 1;
	}

	/* we create a ~640kB sized talloc pool within the write-queue to ensure individual
	 * log lines (stored as msgbs) will not put result in malloc() calls, and also to
	 * reduce the OOM probability within logging, as the pool is already allocated */
	wq = talloc_pooled_object(target, struct osmo_wqueue, LOG_WQUEUE_LEN,
				  LOG_WQUEUE_LEN*(sizeof(struct msgb)+MAX_LOG_SIZE));
	if (!wq)
		return -ENOMEM;
	osmo_wqueue_init(wq, LOG_WQUEUE_LEN);

	fflush(target->tgt_file.out);
	if (target->type == LOG_TGT_TYPE_FILE) {
		rc = open(target->tgt_file.fname, O_WRONLY|O_APPEND|O_CREAT|O_NONBLOCK, 0660);
		if (rc < 0) {
			talloc_free(wq);
			return -errno;
		}
	} else {
		rc = STDERR_FILENO;
	}
	wq->bfd.fd = rc;
	wq->bfd.when = OSMO_FD_WRITE;
	wq->write_cb = _file_wq_write_cb;

	rc = osmo_fd_register(&wq->bfd);
	if (rc < 0) {
		talloc_free(wq);
		return -EIO;
	}
	target->tgt_file.wqueue = wq;
	target->raw_output = _file_raw_output;
	target->output = NULL;

	/* now that everything succeeded, we can finally close the old output stream */
	if (target->type == LOG_TGT_TYPE_FILE)
		fclose(target->tgt_file.out);
	target->tgt_file.out = NULL;

	return 0;
}

/*! Create a new file-based log target using non-blocking write_queue
 *  \param[in] fname File name of the new log file
 *  \returns Log target in case of success, NULL otherwise
 */
struct log_target *log_target_create_file(const char *fname)
{
	struct log_target *target;
	struct osmo_wqueue *wq;
	int rc;

	target = log_target_create();
	if (!target)
		return NULL;

	target->type = LOG_TGT_TYPE_FILE;
	/* we create a ~640kB sized talloc pool within the write-queue to ensure individual
	 * log lines (stored as msgbs) will not put result in malloc() calls, and also to
	 * reduce the OOM probability within logging, as the pool is already allocated */
	wq = talloc_pooled_object(target, struct osmo_wqueue, LOG_WQUEUE_LEN,
				  LOG_WQUEUE_LEN*(sizeof(struct msgb)+MAX_LOG_SIZE));
	if (!wq) {
		log_target_destroy(target);
		return NULL;
	}
	osmo_wqueue_init(wq, LOG_WQUEUE_LEN);
	wq->bfd.fd = open(fname, O_WRONLY|O_APPEND|O_CREAT|O_NONBLOCK, 0660);
	if (wq->bfd.fd < 0) {
		talloc_free(wq);
		log_target_destroy(target);
		return NULL;
	}
	wq->bfd.when = OSMO_FD_WRITE;
	wq->write_cb = _file_wq_write_cb;

	rc = osmo_fd_register(&wq->bfd);
	if (rc < 0) {
		talloc_free(wq);
		log_target_destroy(target);
		return NULL;
	}

	target->tgt_file.wqueue = wq;
	target->raw_output = _file_raw_output;
	target->tgt_file.fname = talloc_strdup(target, fname);

	return target;
}
#endif

/*! Find a registered log target
 *  \param[in] type Log target type
 *  \param[in] fname File name
 *  \returns Log target (if found), NULL otherwise
 *  Must be called with mutex osmo_log_tgt_mutex held, see log_tgt_mutex_lock.
 */
struct log_target *log_target_find(enum log_target_type type, const char *fname)
{
	struct log_target *tgt;

	llist_for_each_entry(tgt, &osmo_log_target_list, entry) {
		if (tgt->type != type)
			continue;
		switch (tgt->type) {
		case LOG_TGT_TYPE_FILE:
			if (!strcmp(fname, tgt->tgt_file.fname))
				return tgt;
			break;
		case LOG_TGT_TYPE_GSMTAP:
			if (!strcmp(fname, tgt->tgt_gsmtap.hostname))
				return tgt;
			break;
		default:
			return tgt;
		}
	}
	return NULL;
}

/*! Unregister, close and delete a log target
 *  \param[in] target log target to unregister, close and delete */
void log_target_destroy(struct log_target *target)
{
	/* just in case, to make sure we don't have any references */
	log_del_target(target);

#if (!EMBEDDED)
	struct osmo_wqueue *wq;
	switch (target->type) {
	case LOG_TGT_TYPE_FILE:
	case LOG_TGT_TYPE_STDERR:
		if (target->tgt_file.out) {
			if (target->type == LOG_TGT_TYPE_FILE)
				fclose(target->tgt_file.out);
			target->tgt_file.out = NULL;
		}
		wq = target->tgt_file.wqueue;
		if (wq) {
			if (wq->bfd.fd >= 0) {
				osmo_fd_unregister(&wq->bfd);
				if (target->type == LOG_TGT_TYPE_FILE)
					close(wq->bfd.fd);
				wq->bfd.fd = -1;
			}
			osmo_wqueue_clear(wq);
			talloc_free(wq);
			target->tgt_file.wqueue = NULL;
		}
		talloc_free((void *)target->tgt_file.fname);
		target->tgt_file.fname = NULL;
		break;
	case LOG_TGT_TYPE_GSMTAP:
		gsmtap_source_free(target->tgt_gsmtap.gsmtap_inst);
		break;
#ifdef HAVE_SYSLOG_H
	case LOG_TGT_TYPE_SYSLOG:
		closelog();
		break;
#endif /* HAVE_SYSLOG_H */
	default:
		/* make GCC happy */
		break;
	}
#endif

	talloc_free(target);
}

/*! close and re-open a log file (for log file rotation)
 *  \param[in] target log target to re-open
 *  \returns 0 in case of success; negative otherwise */
int log_target_file_reopen(struct log_target *target)
{
	struct osmo_wqueue *wq;
	int rc;

	OSMO_ASSERT(target->type == LOG_TGT_TYPE_FILE || target->type == LOG_TGT_TYPE_STDERR);
	OSMO_ASSERT(target->tgt_file.out || target->tgt_file.wqueue);

	if (target->tgt_file.out) {
		fclose(target->tgt_file.out);
		target->tgt_file.out = fopen(target->tgt_file.fname, "a");
		if (!target->tgt_file.out)
			return -errno;
	} else {
		wq = target->tgt_file.wqueue;
		if (wq->bfd.fd >= 0) {
			osmo_fd_unregister(&wq->bfd);
			close(wq->bfd.fd);
			wq->bfd.fd = -1;
		}

		rc = open(target->tgt_file.fname, O_WRONLY|O_APPEND|O_CREAT|O_NONBLOCK, 0660);
		if (rc < 0)
			return -errno;
		wq->bfd.fd = rc;
		rc = osmo_fd_register(&wq->bfd);
		if (rc < 0)
			return rc;
	}

	return 0;
}

/*! close and re-open all log files (for log file rotation)
 *  \returns 0 in case of success; negative otherwise */
int log_targets_reopen(void)
{
	struct log_target *tar;
	int rc = 0;

	log_tgt_mutex_lock();

	llist_for_each_entry(tar, &osmo_log_target_list, entry) {
		switch (tar->type) {
		case LOG_TGT_TYPE_FILE:
			if (log_target_file_reopen(tar) < 0)
				rc = -1;
			break;
		default:
			break;
		}
	}

	log_tgt_mutex_unlock();

	return rc;
}

/*! Enable the log level lookup cache to bypass string formatting and other code for log statements which are
 *  not actually enabled/needed by any existing log target.
 *  \retruns 0 in case of success, negative -errno in case of error. */
int log_cache_enable(void)
{
#if !defined(EMBEDDED)
	if (log_level_lookup_cache)
		return -EEXIST;

	log_level_lookup_cache = talloc_zero_array(osmo_log_info, uint8_t, osmo_log_info->num_cat);
	if (!log_level_lookup_cache)
		return -ENOMEM;

	/* copy everything for level lookup cache */
	log_tgt_mutex_lock();
	log_cache_update_all();
	log_tgt_mutex_unlock();

	return 0;
#else
	return -ENOTSUP;
#endif
}


/*! Initialize the Osmocom logging core
 *  \param[in] inf Information regarding logging categories, could be NULL
 *  \param[in] ctx talloc context for logging allocations
 *  \returns 0 in case of success, negative in case of error
 *
 *  If inf is NULL then only library-internal categories are initialized.
 */
int log_init(const struct log_info *inf, void *ctx)
{
	int i;
	struct log_info_cat *cat_ptr;

	/* Ensure that log_init is not called multiple times */
	OSMO_ASSERT(tall_log_ctx == NULL)

	tall_log_ctx = talloc_named_const(ctx, 1, "logging");
	if (!tall_log_ctx)
		return -ENOMEM;

	osmo_log_info = talloc_zero(tall_log_ctx, struct log_info);
	if (!osmo_log_info)
		return -ENOMEM;

	osmo_log_info->num_cat = ARRAY_SIZE(internal_cat);

	if (inf) {
		osmo_log_info->filter_fn = inf->filter_fn;
		osmo_log_info->num_cat_user = inf->num_cat;
		osmo_log_info->num_cat += inf->num_cat;
	}

	cat_ptr = talloc_zero_array(osmo_log_info, struct log_info_cat,
				    osmo_log_info->num_cat);
	if (!cat_ptr) {
		talloc_free(osmo_log_info);
		osmo_log_info = NULL;
		return -ENOMEM;
	}

	/* copy over the user part and sanitize loglevel */
	if (inf) {
		for (i = 0; i < inf->num_cat; i++) {
			memcpy(&cat_ptr[i], &inf->cat[i],
			       sizeof(struct log_info_cat));

			/* Make sure that the loglevel is set to NOTICE in case
			 * no loglevel has been preset. */
			if (!cat_ptr[i].loglevel) {
				cat_ptr[i].loglevel = LOGL_NOTICE;
			}
		}
	}

	/* copy over the library part */
	for (i = 0; i < ARRAY_SIZE(internal_cat); i++) {
		unsigned int cn = osmo_log_info->num_cat_user + i;
		memcpy(&cat_ptr[cn], &internal_cat[i], sizeof(struct log_info_cat));
	}

	osmo_log_info->cat = cat_ptr;

	return 0;
}

/* De-initialize the Osmocom logging core
 * This function destroys all targets and releases associated memory */
void log_fini(void)
{
	struct log_target *tar, *tar2;

	log_tgt_mutex_lock();

	llist_for_each_entry_safe(tar, tar2, &osmo_log_target_list, entry)
		log_target_destroy(tar);

	talloc_free(osmo_log_info);
	osmo_log_info = NULL;
	talloc_free(tall_log_ctx);
	tall_log_ctx = NULL;

	log_tgt_mutex_unlock();
}

/*! Check whether a log entry will be generated.
 *  \returns != 0 if a log entry might get generated by at least one target */
int log_check_level(int subsys, unsigned int level)
{
	struct log_target *tar;

	assert_loginfo(__func__);

	subsys = map_subsys(subsys);

#if !defined(EMBEDDED)
	if (!log_cache_check(subsys, level))
		return 0;
#endif

	/* TODO: The following could/should be cached (update on config) */

	log_tgt_mutex_lock();

	llist_for_each_entry(tar, &osmo_log_target_list, entry) {
		if (!should_log_to_target(tar, subsys, level))
			continue;

		/* This might get logged (ignoring filters) */
		log_tgt_mutex_unlock();
		return 1;
	}

	/* We are sure, that this will not be logged. */
	log_tgt_mutex_unlock();
	return 0;
}

/*! @} */
