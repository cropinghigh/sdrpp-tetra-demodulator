#pragma once
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
extern const char *osmo_environment_whitelist[];

int osmo_environment_filter(char **out, size_t out_len, char **in, const char **whitelist);
int osmo_environment_append(char **out, size_t out_len, char **in);
int osmo_close_all_fds_above(int last_fd_to_keep);
int osmo_system_nowait2(const char *command, const char **env_whitelist, char **addl_env, const char *user);
int osmo_system_nowait(const char *command, const char **env_whitelist, char **addl_env);
