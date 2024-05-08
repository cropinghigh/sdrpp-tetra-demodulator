/*! \file plugin.c
 *  Routines for loading and managing shared library plug-ins. */
/*
 * (C) 2010 by Harald Welte <laforge@gnumonks.org>
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

/*! \addtogroup utils
 *  @{
 * \file plugin.c */

#include "osmo_config.h"

#if HAVE_DLFCN_H

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

#include <osmocom/core/plugin.h>

/*! Load all plugins available in given directory
 *  \param[in] directory full path name of directory containing plug-ins
 *  \returns number of plugins loaded in case of success, negative in case of error
 */
int osmo_plugin_load_all(const char *directory)
{
	unsigned int num = 0;
	char fname[PATH_MAX];
	DIR *dir;
	struct dirent *entry;

	dir = opendir(directory);
	if (!dir)
		return -errno;

	while ((entry = readdir(dir))) {
		snprintf(fname, sizeof(fname), "%s/%s", directory,
			entry->d_name);
		if (dlopen(fname, RTLD_NOW))
			num++;
	}

	closedir(dir);

	return num;
}
#else
int osmo_plugin_load_all(const char *directory)
{
	return 0;
}
#endif /* HAVE_DLFCN_H */

/*! @} */
