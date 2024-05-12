/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if using 'alloca.c'. */
/* #undef C_ALLOCA */

/* Select building for embedded use */
/* #undef EMBEDDED */

/* Enable netlink socket support via libmnl */
#define ENABLE_LIBMNL 0

/* Enable systemd-journal logging target */
/* #undef ENABLE_SYSTEMD_LOGGING */

/* Force the use of select() instead of poll() */
/* #undef FORCE_IO_SELECT */

/* Define to 1 if you have 'alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if <alloca.h> works. */
#define HAVE_ALLOCA_H 1

/* Support AVX2 (Advanced Vector Extensions 2) instructions */
#define HAVE_AVX2 /**/

/* Define if clock_gettime is available */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define to 1 if you have the declaration of 'SYS_getrandom', and to 0 if you
   don't. */
#define HAVE_DECL_SYS_GETRANDOM 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <execinfo.h> header file. */
#define HAVE_EXECINFO_H 1

/* Define to 1 if you have the 'gettid' function. */
#define HAVE_GETTID 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define 1 to enable SCTP support */
#define HAVE_LIBSCTP 0

/* Define to 1 if you have the 'localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Support ARM NEON instructions */
/* #undef HAVE_NEON */

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the <netinet/tcp.h> header file. */
#define HAVE_NETINET_TCP_H 1

/* Build with PC/SC support */
#define HAVE_PCSC 1

/* Define to 1 if you have the <poll.h> header file. */
#define HAVE_POLL_H 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Have function pthread_setname_np(const char*) */
#define HAVE_PTHREAD_GETNAME_NP 1

/* Have PTHREAD_PRIO_INHERIT. */
#define HAVE_PTHREAD_PRIO_INHERIT 1

/* Support SSE4.1 (Streaming SIMD Extensions 4.1) instructions */
#define HAVE_SSE4_1 /**/

/* Support SSSE3 (Supplemental Streaming SIMD Extensions 3) instructions */
#define HAVE_SSSE3 /**/

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if using SystemTap probes. */
/* #undef HAVE_SYSTEMTAP */

/* Define to 1 if you have the <sys/eventfd.h> header file. */
#define HAVE_SYS_EVENTFD_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/signalfd.h> header file. */
#define HAVE_SYS_SIGNALFD_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/timerfd.h> header file. */
#define HAVE_SYS_TIMERFD_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if your <linux/tcp.h> header file have the tcpi_notsent_bytes
   member in struct tcp_info */
#define HAVE_TCP_INFO_TCPI_NOTSENT_BYTES 1

/* Define to 1 if your <linux/tcp.h> header file have the tcpi_reord_seen
   member in struct tcp_info */
#define HAVE_TCP_INFO_TCPI_REORD_SEEN 1

/* Define to 1 if your <linux/tcp.h> header file have the tcpi_rwnd_limited
   member in struct tcp_info */
#define HAVE_TCP_INFO_TCPI_RWND_LIMITED 1

/* Define to 1 if your <linux/tcp.h> header file have the tcpi_sndbuf_limited
   member in struct tcp_info */
#define HAVE_TCP_INFO_TCPI_SNDBUF_LIMITED 1

/* Define if struct tm has tm_gmtoff member. */
#define HAVE_TM_GMTOFF_IN_TM 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Build with io_uring support */
#define HAVE_URING 0

/* Define to 1 if compiler has the '__builtin_cpu_supports' built-in function
   */
#define HAVE___BUILTIN_CPU_SUPPORTS 1

/* Disable logging macros */
/* #undef LIBOSMOCORE_NO_LOGGING */

/* Define to the sub-directory where libtool stores uninstalled libraries. */
#define LT_OBJDIR ".libs/"

/* Instrument the osmo_fd_register */
/* #undef OSMO_FD_CHECK */

/* Name of package */
#define PACKAGE "libosmocore"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "openbsc@lists.osmocom.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "libosmocore"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "libosmocore UNKNOWN"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "libosmocore"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "UNKNOWN"

/* Use infinite loop on panic rather than fprintf/abort */
/* #undef PANIC_INFLOOP */

/* Define to necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Use GnuTLS as a fallback for missing getrandom() */
#define USE_GNUTLS 1

/* Version number of package */
#define VERSION "UNKNOWN"

/* Define as 'unsigned int' if <stddef.h> doesn't define. */
/* #undef size_t */
