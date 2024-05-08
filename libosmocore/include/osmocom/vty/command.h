/*! \file command.h
 * Zebra configuration command interface routine. */
/*
 * Copyright (C) 1997, 98 Kunihiro Ishiguro
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include "vector.h"

#include <osmocom/core/defs.h>
#include <osmocom/core/utils.h>

/*! \defgroup command VTY Command
 *  @{
 * \file command.h */

/*! Host configuration variable */
struct host {
	/*! Host name of this router. */
	char *name;

	/*! Password for vty interface. */
	char *password;
	char *password_encrypt;

	/*! Enable password */
	char *enable;
	char *enable_encrypt;

	/*! System wide terminal lines. */
	int lines;

	/*! Log filename. */
	char *logfile;

	/*! config file name of this host */
	char *config;

	/*! Flags for services */
	int advanced;
	int encrypt;

	/*! Banner configuration. */
	const char *motd;
	char *motdfile;

	/*! VTY application information */
	const struct vty_app_info *app_info;
};

/*! There are some command levels which called from command node. */
enum node_type {
	AUTH_NODE,		/*!< Authentication mode of vty interface. */
	VIEW_NODE,		/*!< View node. Default mode of vty interface. */
	AUTH_ENABLE_NODE,	/*!< Authentication mode for change enable. */
	ENABLE_NODE,		/*!< Enable node. */
	CONFIG_NODE,		/*!< Config node. Default mode of config file. */
	SERVICE_NODE,		/*!< Service node. */
	DEBUG_NODE,		/*!< Debug node. */
	CFG_LOG_NODE,		/*!< Configure the logging */
	CFG_STATS_NODE,		/*!< Configure the statistics */

	VTY_NODE,		/*!< Vty node. */

	L_E1INP_NODE,		/*!< E1 line in libosmo-abis. */
	L_IPA_NODE,		/*!< IPA proxying commands in libosmo-abis. */
	L_NS_NODE,		/*!< NS node in libosmo-gb. */
	L_BSSGP_NODE,		/*!< BSSGP node in libosmo-gb. */
	L_CTRL_NODE,		/*!< Control interface node. */

	L_CS7_NODE,		/*!< SS7 root node */
	L_CS7_AS_NODE,		/*!< SS7 Application Server */
	L_CS7_ASP_NODE,		/*!< SS7 Application Server Process */
	L_CS7_XUA_NODE,		/*!< SS7 xUA Listener */
	L_CS7_RTABLE_NODE,	/*!< SS7 Routing Table */
	L_CS7_LINK_NODE,	/*!< SS7 Link */
	L_CS7_LINKSET_NODE,	/*!< SS7 Linkset */
	L_CS7_SCCPADDR_NODE,	/*!< SS7 SCCP Address */
	L_CS7_SCCPADDR_GT_NODE,	/*!< SS7 SCCP Global Title */

	L_CPU_SCHED_NODE,	/*!< CPU Sched related options node */
	L_NS_BIND_NODE,		/*!< NS bind node */
	L_NS_NSE_NODE,		/*!< NS NSE node */
	/*
	 * When adding new nodes to the libosmocore project, these nodes can be
	 * used to avoid ABI changes for unrelated projects.
	 */
	RESERVED1_NODE,		/*!< Reserved for later extensions */
	RESERVED2_NODE,		/*!< Reserved for later extensions */
	RESERVED3_NODE,		/*!< Reserved for later extensions */
	RESERVED4_NODE,		/*!< Reserved for later extensions */
	RESERVED5_NODE,		/*!< Reserved for later extensions */
	RESERVED6_NODE,		/*!< Reserved for later extensions */
	RESERVED7_NODE,		/*!< Reserved for later extensions */
	RESERVED8_NODE,		/*!< Reserved for later extensions */

	_LAST_OSMOVTY_NODE
};

#include "vty.h"

/*! Node which has some commands and prompt string and
 * configuration function pointer . */
struct cmd_node {
	/*! Node index */
	int node;

	/*! Prompt character at vty interface. */
	const char *prompt;

	/*! Is this node's configuration goes to vtysh ? */
	int vtysh;

	/*! Node's configuration write function */
	int (*func) (struct vty *);

	/*! Vector of this node's command list. */
	vector cmd_vector;

	/*! Human-readable ID of this node. Should only contain alphanumeric
	 * plus '-' and '_' characters (is used as XML ID for 'show
	 * online-help'). If left NUL, this is derived from the prompt.*/
	char name[64];
};

/*! Attributes (flags) for \ref cmd_element */
enum {
	CMD_ATTR_DEPRECATED	= (1 << 0),
	CMD_ATTR_HIDDEN		= (1 << 1),
	CMD_ATTR_IMMEDIATE	= (1 << 2),
	CMD_ATTR_NODE_EXIT	= (1 << 3),
	CMD_ATTR_LIB_COMMAND	= (1 << 4),
};

/*! Attributes shared between libraries (up to 32 entries). */
enum {
	/* The entries of this enum shall conform the following requirements:
	 * 1. Naming format: 'OSMO_' + <LIBNAME> + '_LIB_ATTR_' + <ATTRNAME>,
	 *    where LIBNAME is a short name of the library, e.g. 'ABIS', 'MGCP',
	 *    and ATTRNAME is a brief name of the attribute, e.g. RTP_CONN_EST;
	 *    for example: 'OSMO_ABIS_LIB_ATTR_RSL_LINK_UP'.
	 * 2. Brevity: shortenings and abbreviations are welcome!
	 * 3. Values are not flags but indexes, unlike CMD_ATTR_*.
	 * 4. Ordering: new entries added before _OSMO_CORE_LIB_ATTR_COUNT. */
	OSMO_SCCP_LIB_ATTR_RSTRT_ASP,
	OSMO_ABIS_LIB_ATTR_IPA_NEW_LNK,
	OSMO_ABIS_LIB_ATTR_LINE_UPD,

	/* Keep this floating entry last, it's needed for count check. */
	_OSMO_CORE_LIB_ATTR_COUNT
};

/*! Structure of a command element */
struct cmd_element {
	const char *string;	/*!< Command specification by string. */
	int (*func) (struct cmd_element *, struct vty *, int, const char *[]);
	const char *doc;	/*!< Documentation of this command. */
	int daemon;		/*!< Daemon to which this command belong. */
	vector strvec;		/*!< Pointing out each description vector. */
	unsigned int cmdsize;	/*!< Command index count. */
	char *config;		/*!< Configuration string */
	vector subconfig;	/*!< Sub configuration string */
	unsigned char attr;	/*!< Command attributes (global) */
	unsigned int usrattr;	/*!< Command attributes (program specific) */
};

/*! Command description structure. */
struct desc {
	const char *cmd;	/*!< Command string. */
	const char *str;	/*!< Command's description. */
};

/*! Return value of the commands. */
#define CMD_SUCCESS              0
#define CMD_WARNING              1
#define CMD_ERR_NO_MATCH         2
#define CMD_ERR_AMBIGUOUS        3
#define CMD_ERR_INCOMPLETE       4
#define CMD_ERR_EXEED_ARGC_MAX   5
#define CMD_ERR_NOTHING_TODO     6
#define CMD_COMPLETE_FULL_MATCH  7
#define CMD_COMPLETE_MATCH       8
#define CMD_COMPLETE_LIST_MATCH  9
#define CMD_SUCCESS_DAEMON      10
#define CMD_ERR_INVALID_INDENT  11

/* Argc max counts. */
#define CMD_ARGC_MAX   256

/* Turn off these macros when uisng cpp with extract.pl */
#ifndef VTYSH_EXTRACT_PL

/* helper defines for end-user DEFUN* macros */
#define DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attrs, dnum) \
  static struct cmd_element cmdname = \
  { \
    .string = cmdstr, \
    .func = funcname, \
    .doc = helpstr, \
    .attr = attrs, \
    .daemon = dnum, \
  };

/* global (non static) cmd_element */
#define gDEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attrs, dnum) \
  struct cmd_element cmdname = \
  { \
    .string = cmdstr, \
    .func = funcname, \
    .doc = helpstr, \
    .attr = attrs, \
    .daemon = dnum, \
  };

#define DEFUN_CMD_ELEMENT_ATTR_USRATTR(funcname, cmdname, cmdstr, helpstr, attrs, usrattrs) \
  static struct cmd_element cmdname = \
  { \
    .string = cmdstr, \
    .func = funcname, \
    .doc = helpstr, \
    .attr = attrs, \
    .usrattr = usrattrs, \
  };

#define DEFUN_CMD_FUNC_DECL(funcname) \
  static int funcname (struct cmd_element *, struct vty *, int, const char *[]); \

#define DEFUN_CMD_FUNC_TEXT(funcname) \
  static int funcname \
    (struct cmd_element *self, struct vty *vty, int argc, const char *argv[])

/*! Macro for defining a VTY node and function
 *  \param[in] funcname Name of the function implementing the node
 *  \param[in] cmdname Name of the command node
 *  \param[in] cmdstr String with syntax of node
 *  \param[in] helpstr String with help message of node
 */
#define DEFUN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0) \
  DEFUN_CMD_FUNC_TEXT(funcname)

/*! Macro for defining a non-static (global) VTY node and function
 *  \param[in] funcname Name of the function implementing the node
 *  \param[in] cmdname Name of the command node
 *  \param[in] cmdstr String with syntax of node
 *  \param[in] helpstr String with help message of node
 */
#define gDEFUN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  gDEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_ATTR(funcname, cmdname, cmdstr, helpstr, attr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, 0) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_HIDDEN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_ATTR (funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN)

#define DEFUN_DEPRECATED(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_ATTR (funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED)

/*! Macro for defining a VTY node and function with global & program specific attributes.
 *  \param[in] funcname Name of the function implementing the node.
 *  \param[in] cmdname Name of the command node.
 *  \param[in] attr Global attributes (see CMD_ATTR_*).
 *  \param[in] usrattr Program specific attributes.
 *  \param[in] cmdstr String with syntax of node.
 *  \param[in] helpstr String with help message of node.
 */
#define DEFUN_ATTR_USRATTR(funcname, cmdname, attr, usrattr, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT_ATTR_USRATTR(funcname, cmdname, cmdstr, helpstr, attr, usrattr) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUN_USRATTR(funcname, cmdname, usrattr, cmdstr, helpstr) \
  DEFUN_ATTR_USRATTR(funcname, cmdname, 0, usrattr, cmdstr, helpstr)

/* DEFUN_NOSH for commands that vtysh should ignore */
#define DEFUN_NOSH(funcname, cmdname, cmdstr, helpstr) \
  DEFUN(funcname, cmdname, cmdstr, helpstr)

/* DEFSH for vtysh. */
#define DEFSH(daemon, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(NULL, cmdname, cmdstr, helpstr, 0, daemon) \

/* DEFUN + DEFSH */
#define DEFUNSH(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, daemon) \
  DEFUN_CMD_FUNC_TEXT(funcname)

/* DEFUN + DEFSH with attributes */
#define DEFUNSH_ATTR(daemon, funcname, cmdname, cmdstr, helpstr, attr) \
  DEFUN_CMD_FUNC_DECL(funcname) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, daemon) \
  DEFUN_CMD_FUNC_TEXT(funcname)

#define DEFUNSH_HIDDEN(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUNSH_ATTR (daemon, funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN)

#define DEFUNSH_DEPRECATED(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUNSH_ATTR (daemon, funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED)

/* ALIAS macro which define existing command's alias. */
#define ALIAS(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0)

/* global (non static) cmd_element */
#define gALIAS(funcname, cmdname, cmdstr, helpstr) \
  gDEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, 0)

#define ALIAS_ATTR(funcname, cmdname, cmdstr, helpstr, attr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, attr, 0)

#define ALIAS_HIDDEN(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN, 0)

#define ALIAS_DEPRECATED(funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED, 0)

#define ALIAS_SH(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, 0, daemon)

#define ALIAS_SH_HIDDEN(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_HIDDEN, daemon)

#define ALIAS_SH_DEPRECATED(daemon, funcname, cmdname, cmdstr, helpstr) \
  DEFUN_CMD_ELEMENT(funcname, cmdname, cmdstr, helpstr, CMD_ATTR_DEPRECATED, daemon)

#endif				/* VTYSH_EXTRACT_PL */

/* Some macroes */
#define CMD_OPTION(S)   ((S[0]) == '[')
#define CMD_VARIABLE(S) (((S[0]) >= 'A' && (S[0]) <= 'Z') || ((S[0]) == '<'))
#define CMD_VARARG(S)   ((S[0]) == '.')
#define CMD_RANGE(S)	((S[0] == '<'))

#define CMD_IPV4(S)	   ((strcmp ((S), "A.B.C.D") == 0))
#define CMD_IPV4_PREFIX(S) ((strcmp ((S), "A.B.C.D/M") == 0))
#define CMD_IPV6(S)        ((strcmp ((S), "X:X::X:X") == 0))
#define CMD_IPV6_PREFIX(S) ((strcmp ((S), "X:X::X:X/M") == 0))

#define VTY_IPV4_CMD "A.B.C.D"
#define VTY_IPV6_CMD "X:X::X:X"
#define VTY_IPV46_CMD "(" VTY_IPV4_CMD "|" VTY_IPV6_CMD ")"

/* Common descriptions. */
#define SHOW_STR "Show running system information\n"
#define IP_STR "IP information\n"
#define IPV6_STR "IPv6 information\n"
#define NO_STR "Negate a command or set its defaults\n"
#define CLEAR_STR "Reset functions\n"
#define RIP_STR "RIP information\n"
#define BGP_STR "BGP information\n"
#define OSPF_STR "OSPF information\n"
#define NEIGHBOR_STR "Specify neighbor router\n"
#define DEBUG_STR "Debugging functions (see also 'undebug')\n"
#define UNDEBUG_STR "Disable debugging functions (see also 'debug')\n"
#define ROUTER_STR "Enable a routing process\n"
#define AS_STR "AS number\n"
#define MBGP_STR "MBGP information\n"
#define MATCH_STR "Match values from routing table\n"
#define SET_STR "Set values in destination routing protocol\n"
#define OUT_STR "Filter outgoing routing updates\n"
#define IN_STR  "Filter incoming routing updates\n"
#define V4NOTATION_STR "specify by IPv4 address notation(e.g. 0.0.0.0)\n"
#define OSPF6_NUMBER_STR "Specify by number\n"
#define INTERFACE_STR "Interface infomation\n"
#define IFNAME_STR "Interface name(e.g. ep0)\n"
#define IP6_STR "IPv6 Information\n"
#define OSPF6_STR "Open Shortest Path First (OSPF) for IPv6\n"
#define OSPF6_ROUTER_STR "Enable a routing process\n"
#define OSPF6_INSTANCE_STR "<1-65535> Instance ID\n"
#define SECONDS_STR "<1-65535> Seconds\n"
#define ROUTE_STR "Routing Table\n"
#define PREFIX_LIST_STR "Build a prefix list\n"
#define OSPF6_DUMP_TYPE_LIST \
"(neighbor|interface|area|lsa|zebra|config|dbex|spf|route|lsdb|redistribute|hook|asbr|prefix|abr)"
#define ISIS_STR "IS-IS information\n"
#define AREA_TAG_STR "[area tag]\n"

#define CONF_BACKUP_EXT ".sav"

/* IPv4 only machine should not accept IPv6 address for peer's IP
   address.  So we replace VTY command string like below. */
#ifdef HAVE_IPV6
#define NEIGHBOR_CMD       "neighbor (A.B.C.D|X:X::X:X) "
#define NO_NEIGHBOR_CMD    "no neighbor (A.B.C.D|X:X::X:X) "
#define NEIGHBOR_ADDR_STR  "Neighbor address\nIPv6 address\n"
#define NEIGHBOR_CMD2      "neighbor (A.B.C.D|X:X::X:X|WORD) "
#define NO_NEIGHBOR_CMD2   "no neighbor (A.B.C.D|X:X::X:X|WORD) "
#define NEIGHBOR_ADDR_STR2 "Neighbor address\nNeighbor IPv6 address\nNeighbor tag\n"
#else
#define NEIGHBOR_CMD       "neighbor A.B.C.D "
#define NO_NEIGHBOR_CMD    "no neighbor A.B.C.D "
#define NEIGHBOR_ADDR_STR  "Neighbor address\n"
#define NEIGHBOR_CMD2      "neighbor (A.B.C.D|WORD) "
#define NO_NEIGHBOR_CMD2   "no neighbor (A.B.C.D|WORD) "
#define NEIGHBOR_ADDR_STR2 "Neighbor address\nNeighbor tag\n"
#endif				/* HAVE_IPV6 */

/* Prototypes. */
void install_node(struct cmd_node *, int (*)(struct vty *));
void install_default(int node_type) OSMO_DEPRECATED("Now happens implicitly with install_node()");
void install_element(int node_type, struct cmd_element *);
void install_lib_element(int node_type, struct cmd_element *);
void install_element_ve(struct cmd_element *cmd);
void install_lib_element_ve(struct cmd_element *cmd);
void sort_node(void);

void vty_install_default(int node_type) OSMO_DEPRECATED("Now happens implicitly with install_node()");

/* Concatenates argv[shift] through argv[argc-1] into a single NUL-terminated
   string with a space between each element (allocated using
   XMALLOC(MTYPE_TMP)).  Returns NULL if shift >= argc. */
char *argv_concat(const char **argv, int argc, int shift);

vector cmd_make_strvec(const char *);
int cmd_make_strvec2(const char *string, char **indent, vector *strvec_p);
void cmd_free_strvec(vector);
vector cmd_describe_command(vector vline, struct vty *vty, int *status);
char **cmd_complete_command(vector vline, struct vty *vty, int *status);
const char *cmd_prompt(enum node_type);
int config_from_file(struct vty *, FILE *);
enum node_type node_parent(enum node_type);
int cmd_execute_command(vector, struct vty *, struct cmd_element **, int);
int cmd_execute_command_strict(vector, struct vty *, struct cmd_element **);
void config_replace_string(struct cmd_element *, char *, ...);
void cmd_init(int);

/* Export typical functions. */
extern struct cmd_element config_exit_cmd;
extern struct cmd_element config_help_cmd;
extern struct cmd_element config_list_cmd;
extern struct cmd_element config_end_cmd;
const char *host_config_file(void);
void host_config_set(const char *);

char *osmo_asciidoc_escape(const char *inp);

/* This is called from main when a daemon is invoked with -v or --version. */
void print_version(int print_copyright);

extern void *tall_vty_cmd_ctx;

/*! VTY reference generation mode. */
enum vty_ref_gen_mode {
	/*! Default mode: all commands except deprecated and hidden. */
	VTY_REF_GEN_MODE_DEFAULT = 0,
	/*! Expert mode: all commands including hidden, excluding deprecated. */
	VTY_REF_GEN_MODE_EXPERT,
	/*! "Inverse" mode: only hidden commands. */
	VTY_REF_GEN_MODE_HIDDEN,
};

extern const struct value_string vty_ref_gen_mode_names[];
extern const struct value_string vty_ref_gen_mode_desc[];

int vty_dump_xml_ref_mode(FILE *stream, enum vty_ref_gen_mode mode);
int vty_dump_xml_ref(FILE *stream) OSMO_DEPRECATED("Use vty_dump_xml_ref_mode() instead");

int vty_cmd_range_match(const char *range, const char *str);

/*! @} */
