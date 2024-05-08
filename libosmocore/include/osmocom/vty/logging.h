/*! \file logging.h */

#pragma once

#define LOGGING_STR	"Configure logging\n"
#define FILTER_STR	"Filter log messages\n"

struct log_info;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wstrict-prototypes"
/* note this undefined argument declaration is intentional. There used
 * to be an argument until 2017 which we no longer need .*/
void logging_vty_add_cmds();
#pragma GCC diagnostic pop
void logging_vty_add_deprecated_subsys(void *ctx, const char *name);
struct vty;
struct log_target *osmo_log_vty2tgt(struct vty *vty);
