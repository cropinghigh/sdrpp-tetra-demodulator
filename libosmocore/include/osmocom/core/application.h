#pragma once

#include <osmocom/core/defs.h>

struct log_info;
struct log_target;

/*!
 * \file application.h
 * Routines for helping with the osmocom application setup.
 */

/*! the default logging target, logging to stderr */
extern struct log_target *osmo_stderr_target;

void osmo_init_ignore_signals(void);
int osmo_init_logging(const struct log_info *)
	OSMO_DEPRECATED("use osmo_init_logging2() instead to avoid a NULL talloc ctx");
int osmo_init_logging2(void *ctx, const struct log_info *log_info);

int osmo_daemonize(void);
