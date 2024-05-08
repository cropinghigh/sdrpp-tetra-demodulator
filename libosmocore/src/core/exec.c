/* (C) 2019 by Harald Welte <laforge@gnumonks.org>
 *
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

#include "osmo_config.h"
#ifndef EMBEDDED

#define _GNU_SOURCE
#include <unistd.h>

#include <errno.h>
#include <string.h>

#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <pwd.h>

#include <osmocom/core/logging.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/exec.h>

/*! suggested list of environment variables to pass (if they exist) to a sub-process/script */
const char *osmo_environment_whitelist[] = {
	"USER", "LOGNAME", "HOME",
	"LANG", "LC_ALL", "LC_COLLATE", "LC_CTYPE", "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC", "LC_TIME",
	"PATH",
	"PWD",
	"SHELL",
	"TERM",
	"TMPDIR",
	"LD_LIBRARY_PATH",
	"LD_PRELOAD",
	"POSIXLY_CORRECT",
	"HOSTALIASES",
	"TZ", "TZDIR",
	"TERMCAP",
	"COLUMNS", "LINES",
	NULL
};

static bool str_in_list(const char **list, const char *key)
{
	const char **ent;

	for (ent = list; *ent; ent++) {
		if (!strcmp(*ent, key))
			return true;
	}
	return false;
}

/*! filtered a process environment by whitelist; only copying pointers, no actual strings.
 *
 *  This function is useful if you'd like to generate an environment to pass exec*e()
 *  functions.  It will create a new environment containing only those entries whose
 *  keys (as per environment convention KEY=VALUE) are contained in the whitelist.  The
 *  function will not copy the actual strings, but just create a new pointer array, pointing
 *  to the same memory as the input strings.
 *
 *  Constraints: Keys up to a maximum length of 255 characters are supported.
 *
 *  \param[out] out caller-allocated array of pointers for the generated output
 *  \param[in] out_len size of out (number of pointers)
 *  \param[in] in input environment (NULL-terminated list of pointers like **environ)
 *  \param[in] whitelist whitelist of permitted keys in environment (like **environ)
 *  \returns number of entries filled in 'out'; negtive on error */
int osmo_environment_filter(char **out, size_t out_len, char **in, const char **whitelist)
{
	char tmp[256];
	char **ent;
	size_t out_used = 0;

	/* invalid calls */
	if (!out || out_len == 0 || !whitelist)
		return -EINVAL;

	/* legal, but unusual: no input to filter should generate empty, terminated out */
	if (!in) {
		out[0] = NULL;
		return 1;
	}

	/* iterate over input entries */
	for (ent = in; *ent; ent++) {
		char *eq = strchr(*ent, '=');
		unsigned long eq_pos;
		if (!eq) {
			/* no '=' in string, skip it */
			continue;
		}
		eq_pos = eq - *ent;
		if (eq_pos >= ARRAY_SIZE(tmp))
			continue;
		strncpy(tmp, *ent, eq_pos);
		tmp[eq_pos] = '\0';
		if (str_in_list(whitelist, tmp)) {
			if (out_used == out_len-1)
				break;
			/* append to output */
			out[out_used++] = *ent;
		}
	}
	OSMO_ASSERT(out_used < out_len);
	out[out_used++] = NULL;
	return out_used;
}

/*! append one environment to another; only copying pointers, not actual strings.
 *
 *  This function is useful if you'd like to append soem entries to an environment
 *  befoer passing it to exec*e() functions.
 *
 *  It will append all entries from 'in' to the environment in 'out', as long as
 *  'out' has space (determined by 'out_len').
 *
 *  Constraints: If the same key exists in 'out' and 'in', duplicate keys are
 *  generated.  It is a simple append, without any duplicate checks.
 *
 *  \param[out] out caller-allocated array of pointers for the generated output
 *  \param[in] out_len size of out (number of pointers)
 *  \param[in] in input environment (NULL-terminated list of pointers like **environ)
 *  \returns number of entries filled in 'out'; negative on error */
int osmo_environment_append(char **out, size_t out_len, char **in)
{
	size_t out_used = 0;

	if (!out || out_len == 0)
		return -EINVAL;

	/* seek to end of existing output */
	for (out_used = 0; out[out_used]; out_used++) {}

	if (!in) {
		if (out_used == 0)
			out[out_used++] = NULL;
		return out_used;
	}

	for (; *in && out_used < out_len-1; in++)
		out[out_used++] = *in;

	OSMO_ASSERT(out_used < out_len);
	out[out_used++] = NULL;

	return out_used;
}

/* Iterate over files in /proc/self/fd and close all above lst_fd_to_keep */
int osmo_close_all_fds_above(int last_fd_to_keep)
{
	struct dirent *ent;
	DIR *dir;
	int rc;

	dir = opendir("/proc/self/fd");
	if (!dir) {
		LOGP(DLGLOBAL, LOGL_ERROR, "Cannot open /proc/self/fd: %s\n", strerror(errno));
		return -ENODEV;
	}

	while ((ent = readdir(dir))) {
		int fd = atoi(ent->d_name);
		if (fd <= last_fd_to_keep)
			continue;
		if (fd == dirfd(dir))
			continue;
		rc = close(fd);
		if (rc)
			LOGP(DLGLOBAL, LOGL_ERROR, "Error closing fd=%d: %s\n", fd, strerror(errno));
	}
	closedir(dir);
	return 0;
}

/* Seems like POSIX has no header file for this, and even glibc + __USE_GNU doesn't help */
extern char **environ;

/*! call an external shell command as 'user' without waiting for it.
 *
 *  This mimics the behavior of system(3), with the following differences:
 *  - it doesn't wait for completion of the child process
 *  - it closes all non-stdio file descriptors by iterating /proc/self/fd
 *  - it constructs a reduced environment where only whitelisted keys survive
 *  - it (optionally) appends additional variables to the environment
 *  - it (optionally) changes the user ID to that of 'user' (requires execution as root)
 *
 *  \param[in] command the shell command to be executed, see system(3)
 *  \param[in] env_whitelist A white-list of keys for environment variables
 *  \param[in] addl_env any additional environment variables to be appended
 *  \param[in] user name of the user to which we should switch before executing the command
 *  \returns PID of generated child process; negative on error
 */
int osmo_system_nowait2(const char *command, const char **env_whitelist, char **addl_env, const char *user)
{
	struct passwd _pw;
	struct passwd *pw = NULL;
	int getpw_buflen = sysconf(_SC_GETPW_R_SIZE_MAX);
	int rc;

	if (user) {
		if (getpw_buflen == -1) /* Value was indeterminate */
			getpw_buflen = 16384; /* Should be more than enough */
		char buf[getpw_buflen];
		rc = getpwnam_r(user, &_pw, buf, sizeof(buf), &pw);
		if (rc < 0) {
			LOGP(DLGLOBAL, LOGL_ERROR, "getpwnam_r(\"%s\") failed: %s\n", user, strerror(-rc));
			return rc;
		}
		if (!pw) {
			LOGP(DLGLOBAL, LOGL_ERROR, "getpwnam_r(\"%s\"): user not found!\n", user);
			return -EINVAL;
		}
	}

	rc = fork();
	if (rc == 0) {
		/* we are in the child */
		char *new_env[1024];

		/* close all file descriptors above stdio */
		osmo_close_all_fds_above(2);

		/* man execle: "an array of pointers *must* be terminated by a null pointer" */
		new_env[0] = NULL;

		/* build the new environment */
		if (env_whitelist) {
			rc = osmo_environment_filter(new_env, ARRAY_SIZE(new_env), environ, env_whitelist);
			if (rc < 0)
				return rc;
		}
		if (addl_env) {
			rc = osmo_environment_append(new_env, ARRAY_SIZE(new_env), addl_env);
			if (rc < 0)
				return rc;
		}

		/* drop privileges */
		if (pw) {
			if (setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) < 0) {
				perror("setresgid() during privilege drop");
				exit(1);
			}

			if (setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid) < 0) {
				perror("setresuid() during privilege drop");
				exit(1);
			}

		}

		/* if we want to behave like system(3), we must go via the shell */
		execle("/bin/sh", "sh", "-c", command, (char *) NULL, new_env);
		/* only reached in case of error */
		LOGP(DLGLOBAL, LOGL_ERROR, "Error executing command '%s' after fork: %s\n",
			command, strerror(errno));
		return -EIO;
	} else {
		/* we are in the parent */
		if (rc == -1)
			LOGP(DLGLOBAL, LOGL_ERROR, "fork() error executing command '%s': %s\n",
			     command, strerror(errno));
		return rc;
	}
}

/*! call an external shell command without waiting for it.
 *
 *  This mimics the behavior of system(3), with the following differences:
 *  - it doesn't wait for completion of the child process
 *  - it closes all non-stdio file descriptors by iterating /proc/self/fd
 *  - it constructs a reduced environment where only whitelisted keys survive
 *  - it (optionally) appends additional variables to the environment
 *
 *  \param[in] command the shell command to be executed, see system(3)
 *  \param[in] env_whitelist A white-list of keys for environment variables
 *  \param[in] addl_env any additional environment variables to be appended
 *  \returns PID of generated child process; negative on error
 */
int osmo_system_nowait(const char *command, const char **env_whitelist, char **addl_env)
{
	return osmo_system_nowait2(command, env_whitelist, addl_env, NULL);
}


#endif /* EMBEDDED */
