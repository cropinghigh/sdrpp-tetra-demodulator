#pragma once

/*! \defgroup logging Osmocom logging framework
 *  @{
 * \file logging.h */

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <osmocom/core/defs.h>
#include <osmocom/core/linuxlist.h>

extern struct log_info *osmo_log_info;

#ifndef DEBUG
#define DEBUG
#endif

#ifdef LIBOSMOCORE_NO_LOGGING
#undef DEBUG
#endif

#ifdef DEBUG
/*! Log a debug message through the Osmocom logging framework
 *  \param[in] ss logging subsystem (e.g. \ref DLGLOBAL)
 *  \param[in] fmt format string
 *  \param[in] args variable argument list
 */
#define DEBUGP(ss, fmt, args...) LOGP(ss, LOGL_DEBUG, fmt, ##args)
#define DEBUGPC(ss, fmt, args...) LOGPC(ss, LOGL_DEBUG, fmt, ##args)
#else
#define DEBUGP(xss, fmt, args...)
#define DEBUGPC(ss, fmt, args...)
#endif


void osmo_vlogp(int subsys, int level, const char *file, int line,
		int cont, const char *format, va_list ap);

void logp(int subsys, const char *file, int line, int cont, const char *format, ...) OSMO_DEPRECATED("Use DEBUGP* macros instead");

/*! Log a new message through the Osmocom logging framework
 *  \param[in] ss logging subsystem (e.g. \ref DLGLOBAL)
 *  \param[in] level logging level (e.g. \ref LOGL_NOTICE)
 *  \param[in] fmt format string
 *  \param[in] args variable argument list
 */
#define LOGP(ss, level, fmt, args...) \
	LOGPSRC(ss, level, NULL, 0, fmt, ## args)

/*! Continue a log message through the Osmocom logging framework
 *  \param[in] ss logging subsystem (e.g. \ref DLGLOBAL)
 *  \param[in] level logging level (e.g. \ref LOGL_NOTICE)
 *  \param[in] fmt format string
 *  \param[in] args variable argument list
 */
#ifndef LIBOSMOCORE_NO_LOGGING
#define LOGPC(ss, level, fmt, args...) \
	do { \
		if (!osmo_log_info) { \
			logp_stub(__FILE__, __LINE__, 1, fmt, ##args);	\
			break; \
		} \
		if (log_check_level(ss, level)) \
			logp2(ss, level, __FILE__, __LINE__, 1, fmt, ##args); \
	} while(0)
#else
#define LOGPC(ss, level, fmt, args...)
#endif

/*! Log through the Osmocom logging framework with explicit source.
 *  If caller_file is passed as NULL, __FILE__ and __LINE__ are used
 *  instead of caller_file and caller_line (so that this macro here defines
 *  both cases in the same place, and to catch cases where callers fail to pass
 *  a non-null filename string).
 *  \param[in] ss logging subsystem (e.g. \ref DLGLOBAL)
 *  \param[in] level logging level (e.g. \ref LOGL_NOTICE)
 *  \param[in] caller_file caller's source file string (e.g. __FILE__)
 *  \param[in] caller_line caller's source line nr (e.g. __LINE__)
 *  \param[in] fmt format string
 *  \param[in] args variable argument list
 */
#define LOGPSRC(ss, level, caller_file, caller_line, fmt, args...) \
	LOGPSRCC(ss, level, caller_file, caller_line, 0, fmt, ##args)

/*! Log through the Osmocom logging framework with explicit source.
 *  If caller_file is passed as NULL, __FILE__ and __LINE__ are used
 *  instead of caller_file and caller_line (so that this macro here defines
 *  both cases in the same place, and to catch cases where callers fail to pass
 *  a non-null filename string).
 *  \param[in] ss logging subsystem (e.g. \ref DLGLOBAL)
 *  \param[in] level logging level (e.g. \ref LOGL_NOTICE)
 *  \param[in] caller_file caller's source file string (e.g. __FILE__)
 *  \param[in] caller_line caller's source line nr (e.g. __LINE__)
 *  \param[in] cont continuation (1) or new line (0)
 *  \param[in] fmt format string
 *  \param[in] args variable argument list
 */
#ifndef LIBOSMOCORE_NO_LOGGING
#define LOGPSRCC(ss, level, caller_file, caller_line, cont, fmt, args...) \
	do { \
		if (!osmo_log_info) { \
			if (caller_file) \
				logp_stub(caller_file, caller_line, cont, fmt, ##args); \
			else \
				logp_stub(__FILE__, __LINE__, cont, fmt, ##args); \
			break; \
		} \
		if (log_check_level(ss, level)) {\
			if (caller_file) \
				logp2(ss, level, caller_file, caller_line, cont, fmt, ##args); \
			else \
				logp2(ss, level, __FILE__, __LINE__, cont, fmt, ##args); \
		}\
	} while(0)
#else
#define LOGPSRCC(ss, level, caller_file, caller_line, cont, fmt, args...)
#endif

/*! different log levels */
#define LOGL_DEBUG	1	/*!< debugging information */
#define LOGL_INFO	3	/*!< general information */
#define LOGL_NOTICE	5	/*!< abnormal/unexpected condition */
#define LOGL_ERROR	7	/*!< error condition, requires user action */
#define LOGL_FATAL	8	/*!< fatal, program aborted */

/* logging subsystems defined by the library itself */
#define DLGLOBAL	-1	/*!< global logging */
#define DLLAPD		-2	/*!< LAPD implementation */
#define DLINP		-3	/*!< (A-bis) Input sub-system */
#define DLMUX		-4	/*!< Osmocom Multiplex (Osmux) */
#define DLMI		-5	/*!< ISDN-layer below input sub-system */
#define DLMIB		-6	/*!< ISDN layer B-channel */
#define DLSMS		-7	/*!< SMS sub-system */
#define DLCTRL		-8	/*!< Control Interface */
#define DLGTP		-9	/*!< GTP (GPRS Tunneling Protocol */
#define DLSTATS		-10	/*!< Statistics */
#define DLGSUP		-11	/*!< Generic Subscriber Update Protocol */
#define DLOAP		-12	/*!< Osmocom Authentication Protocol */
#define DLSS7		-13	/*!< Osmocom SS7 */
#define DLSCCP		-14	/*!< Osmocom SCCP */
#define DLSUA		-15	/*!< Osmocom SUA */
#define DLM3UA		-16	/*!< Osmocom M3UA */
#define DLMGCP		-17	/*!< Osmocom MGCP */
#define DLJIBUF		-18	/*!< Osmocom Jitter Buffer */
#define DLRSPRO		-19	/*!< Osmocom Remote SIM Protocol */
#define DLNS		-20	/*!< Osmocom NS layer */
#define DLBSSGP		-21	/*!< Osmocom BSSGP layer */
#define DLNSDATA	-22	/*!< Osmocom NS layer data pdus */
#define DLNSSIGNAL	-23	/*!< Osmocom NS layer signal pdus */
#define DLIUUP		-24	/*!< Osmocom IuUP layer */
#define DLPFCP		-25	/*!< Osmocom Packet Forwarding Control Protocol */
#define DLCSN1		-26	/*!< CSN.1 (Concrete Syntax Notation 1) codec */
#define DLM2PA		-27	/*!< Osmocom M2PA (libosmo-sigtran) */
#define DLM2UA		-28	/*!< Reserved for future Osmocom M2UA (libosmo-sigtran) */
#define DLIO		-29	/*!< Osmocom IO sub-system */
#define OSMO_NUM_DLIB	29	/*!< Number of logging sub-systems in libraries */

/* Colors that can be used in log_info_cat.color */
#define OSMO_LOGCOLOR_NORMAL NULL
#define OSMO_LOGCOLOR_RED "\033[1;31m"
#define OSMO_LOGCOLOR_GREEN "\033[1;32m"
#define OSMO_LOGCOLOR_YELLOW "\033[1;33m"
#define OSMO_LOGCOLOR_BLUE "\033[1;34m"
#define OSMO_LOGCOLOR_PURPLE "\033[1;35m"
#define OSMO_LOGCOLOR_CYAN "\033[1;36m"
#define OSMO_LOGCOLOR_DARKRED "\033[31m"
#define OSMO_LOGCOLOR_DARKGREEN "\033[32m"
#define OSMO_LOGCOLOR_DARKYELLOW "\033[33m"
#define OSMO_LOGCOLOR_DARKBLUE "\033[34m"
#define OSMO_LOGCOLOR_DARKPURPLE "\033[35m"
#define OSMO_LOGCOLOR_DARKCYAN "\033[36m"
#define OSMO_LOGCOLOR_DARKGREY "\033[1;30m"
#define OSMO_LOGCOLOR_GREY "\033[37m"
#define OSMO_LOGCOLOR_BRIGHTWHITE "\033[1;37m"
#define OSMO_LOGCOLOR_END "\033[0;m"

/*! Configuration of single log category / sub-system */
struct log_category {
	uint8_t loglevel;	/*!< configured log-level */
	uint8_t enabled;	/*!< is logging enabled? */
};

/*! Information regarding one logging category */
struct log_info_cat {
	const char *name;		/*!< name of category */
	const char *color;		/*!< color string for cateyory */
	const char *description;	/*!< description text */
	uint8_t loglevel;		/*!< currently selected log-level */
	uint8_t enabled;		/*!< is this category enabled or not */
};

/*! Indexes to indicate the object currently acted upon.
 * Array indexes for the global \a log_context array. */
enum log_ctx_index {
	LOG_CTX_GB_NSVC,
	LOG_CTX_GB_BVC,
	LOG_CTX_BSC_SUBSCR,
	LOG_CTX_VLR_SUBSCR,
	LOG_CTX_L1_SAPI,
	LOG_CTX_GB_NSE,
	_LOG_CTX_COUNT
};

/*! Indexes to indicate objects that should be logged.
 * Array indexes to log_target->filter_data and bit indexes for
 * log_target->filter_map. */
enum log_filter_index {
	LOG_FLT_ALL,
	LOG_FLT_GB_NSVC,
	LOG_FLT_GB_BVC,
	LOG_FLT_BSC_SUBSCR,
	LOG_FLT_VLR_SUBSCR,
	LOG_FLT_L1_SAPI,
	LOG_FLT_GB_NSE,
	_LOG_FLT_COUNT
};

/*! Maximum number of logging contexts */
#define LOG_MAX_CTX	_LOG_CTX_COUNT
/*! Maximum number of logging filters */
#define LOG_MAX_FILTERS	_LOG_FLT_COUNT

/*! Log context information, passed to filter */
struct log_context {
	void *ctx[LOG_MAX_CTX+1];
};

/*! Compatibility with older libosmocore versions */
#define LOG_FILTER_ALL (1<<LOG_FLT_ALL)
/*! Compatibility with older libosmocore versions */
#define GPRS_CTX_NSVC LOG_CTX_GB_NSVC
/*! Compatibility with older libosmocore versions */
#define GPRS_CTX_BVC LOG_CTX_GB_BVC
/*! Indexes to indicate the object currently acted upon.
 * Array indexes for the global \a log_context array. */

struct log_target;

/*! Log filter function */
typedef int log_filter(const struct log_context *ctx,
		       struct log_target *target);

struct log_info;
struct vty;
struct gsmtap_inst;

typedef void log_print_filters(struct vty *vty,
			       const struct log_info *info,
			       const struct log_target *tgt);

typedef void log_save_filters(struct vty *vty,
			      const struct log_info *info,
			      const struct log_target *tgt);

/*! Logging configuration, passed to \ref log_init */
struct log_info {
	/* filter callback function */
	log_filter *filter_fn;

	/*! per-category information */
	const struct log_info_cat *cat;
	/*! total number of categories */
	unsigned int num_cat;
	/*! total number of user categories (not library) */
	unsigned int num_cat_user;

	/*! filter saving function */
	log_save_filters *save_fn;
	/*! filter saving function */
	log_print_filters *print_fn;
};

/*! Type of logging target */
enum log_target_type {
	LOG_TGT_TYPE_VTY,	/*!< VTY logging */
	LOG_TGT_TYPE_SYSLOG,	/*!< syslog based logging */
	LOG_TGT_TYPE_FILE,	/*!< text file logging */
	LOG_TGT_TYPE_STDERR,	/*!< stderr logging */
	LOG_TGT_TYPE_STRRB,	/*!< osmo_strrb-backed logging */
	LOG_TGT_TYPE_GSMTAP,	/*!< GSMTAP network logging */
	LOG_TGT_TYPE_SYSTEMD,	/*!< systemd journal logging */
};

/*! Whether/how to log the source filename (and line number). */
enum log_filename_type {
	LOG_FILENAME_NONE,
	LOG_FILENAME_PATH,
	LOG_FILENAME_BASENAME,
};

/*! Where on a log line source file and line should be logged. */
enum log_filename_pos {
	LOG_FILENAME_POS_HEADER_END,
	LOG_FILENAME_POS_LINE_END,
};

/*! structure representing a logging target */
struct log_target {
	struct llist_head entry;		/*!< linked list */

	/*! Internal data for filtering */
	int filter_map;
	/*! Internal data for filtering */
	void *filter_data[LOG_MAX_FILTERS+1];

	/*! logging categories */
	struct log_category *categories;

	/*! global log level */
	uint8_t loglevel;
	/*! should color be used when printing log messages? */
	unsigned int use_color:1;
	/*! should log messages be prefixed with a timestamp? */
	unsigned int print_timestamp:1;
	/*! should log messages be prefixed with the logger Thread ID? */
	unsigned int print_tid:1;
	/*! DEPRECATED: use print_filename2 instead. */
	unsigned int print_filename:1;
	/*! should log messages be prefixed with a category name? */
	unsigned int print_category:1;
	/*! should log messages be prefixed with an extended timestamp? */
	unsigned int print_ext_timestamp:1;

	/*! the type of this log taget */
	enum log_target_type type;

	union {
		struct {
			/* direct, blocking output via stdio */
			FILE *out;
			const char *fname;
			/* indirect output via write_queue and osmo_select_main() */
			struct osmo_wqueue *wqueue;
		} tgt_file;

		struct {
			int priority;
			int facility;
		} tgt_syslog;

		struct {
			void *vty;
		} tgt_vty;

		struct {
			void *rb;
		} tgt_rb;

		struct {
			struct gsmtap_inst *gsmtap_inst;
			const char *ident;
			const char *hostname;
		} tgt_gsmtap;

		struct {
			bool raw;
		} sd_journal;
	};

	/*! call-back function to be called when the logging framework
	 *	   wants to log a fully formatted string
	 *  \param[in] target logging target
	 *  \param[in] level log level of currnet message
	 *  \param[in] string the string that is to be written to the log
	 */
        void (*output) (struct log_target *target, unsigned int level,
			const char *string);

	/*! alternative call-back function to which the logging
	 *	   framework passes the unfortmatted input arguments,
	 *	   i.e. bypassing the internal string formatter
	 *  \param[in] target logging target
	 *  \param[in] subsys logging sub-system
	 *  \param[in] level logging level
	 *  \param[in] file soure code file name
	 *  \param[in] line source code file line number
	 *  \param[in] cont continuation of previous statement?
	 *  \param[in] format format string
	 *  \param[in] ap vararg list of printf arguments
	 */
	void (*raw_output)(struct log_target *target, int subsys,
			   unsigned int level, const char *file, int line,
			   int cont, const char *format, va_list ap);

	/* Should the log level be printed? */
	bool print_level;
	/* Should we print the subsys in hex like '<000b>'? */
	bool print_category_hex;
	/* Should we print the source file and line, and in which way? */
	enum log_filename_type print_filename2;
	/* Where on a log line to put the source file info. */
	enum log_filename_pos print_filename_pos;
};

/* use the above macros */
void logp2(int subsys, unsigned int level, const char *file,
	   int line, int cont, const char *format, ...)
				__attribute__ ((format (printf, 6, 7)));
void logp_stub(const char *file, int line, int cont, const char *format, ...);
int log_init(const struct log_info *inf, void *talloc_ctx);
void log_fini(void);
int log_check_level(int subsys, unsigned int level);

/* context management */
void log_reset_context(void);
int log_set_context(uint8_t ctx, void *value);

/* filter on the targets */
void log_set_all_filter(struct log_target *target, int);
int log_cache_enable(void);
void log_cache_update(int mapped_subsys, uint8_t enabled, uint8_t level);
void log_set_use_color(struct log_target *target, int);
void log_set_print_extended_timestamp(struct log_target *target, int);
void log_set_print_timestamp(struct log_target *target, int);
void log_set_print_tid(struct log_target *target, int);
void log_set_print_filename(struct log_target *target, int) OSMO_DEPRECATED("Use log_set_print_filename2() instead");
void log_set_print_filename2(struct log_target *target, enum log_filename_type lft);
void log_set_print_filename_pos(struct log_target *target, enum log_filename_pos pos);
void log_set_print_category(struct log_target *target, int);
void log_set_print_category_hex(struct log_target *target, int);
void log_set_print_level(struct log_target *target, int);
void log_set_log_level(struct log_target *target, int log_level);
void log_parse_category_mask(struct log_target *target, const char* mask);
const char* log_category_name(int subsys);
int log_parse_level(const char *lvl) OSMO_DEPRECATED_OUTSIDE_LIBOSMOCORE;
const char *log_level_str(unsigned int lvl) OSMO_DEPRECATED_OUTSIDE_LIBOSMOCORE;
int log_parse_category(const char *category);
void log_set_category_filter(struct log_target *target, int category,
			       int enable, int level);

/* management of the targets */
struct log_target *log_target_create(void);
void log_target_destroy(struct log_target *target);
struct log_target *log_target_create_stderr(void);
struct log_target *log_target_create_file(const char *fname);
struct log_target *log_target_create_syslog(const char *ident, int option,
					    int facility);
struct log_target *log_target_create_gsmtap(const char *host, uint16_t port,
					    const char *ident,
					    bool ofd_wq_mode,
					    bool add_sink);
struct log_target *log_target_create_systemd(bool raw);
void log_target_systemd_set_raw(struct log_target *target, bool raw);
int log_target_file_reopen(struct log_target *tgt);
int log_target_file_switch_to_stream(struct log_target *tgt);
int log_target_file_switch_to_wqueue(struct log_target *tgt);
int log_targets_reopen(void);

void log_add_target(struct log_target *target);
void log_del_target(struct log_target *target);

struct log_target *log_target_find(enum log_target_type type, const char *fname);

void log_enable_multithread(void);

void log_tgt_mutex_lock_impl(void);
void log_tgt_mutex_unlock_impl(void);
#define LOG_MTX_DEBUG 0
#if LOG_MTX_DEBUG
	#include <pthread.h>
	#define log_tgt_mutex_lock() do { fprintf(stderr, "[%lu] %s:%d [%s] lock\n", pthread_self(), __FILE__, __LINE__, __func__); log_tgt_mutex_lock_impl(); } while (0)
	#define log_tgt_mutex_unlock() do { fprintf(stderr, "[%lu] %s:%d [%s] unlock\n", pthread_self(), __FILE__, __LINE__, __func__); log_tgt_mutex_unlock_impl(); } while (0)
#else
	#define log_tgt_mutex_lock() log_tgt_mutex_lock_impl()
	#define log_tgt_mutex_unlock() log_tgt_mutex_unlock_impl()
#endif

/*! @} */
