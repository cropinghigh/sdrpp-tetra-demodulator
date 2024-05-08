/*! \file macaddr.c
 *  MAC address utility routines. */
/*
 * (C) 2013-2014 by Harald Welte <laforge@gnumonks.org>
 * (C) 2014 by Holger Hans Peter Freyther
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
 * \file macaddr.c */

#include "osmo_config.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

/*! Parse a MAC address from human-readable notation
 *  This function parses an ethernet MAC address in the commonly-used
 *  hex/colon notation (00:00:00:00:00:00) and generates the binary
 *  representation from it.
 *  \param[out] out pointer to caller-allocated buffer of 6 bytes
 *  \param[in] in pointer to input data as string with hex/colon notation
 */
int osmo_macaddr_parse(uint8_t *out, const char *in)
{
	/* 00:00:00:00:00:00 */
	char tmp[18];
	char *tok;
	unsigned int i = 0;

	if (strlen(in) < 17)
		return -1;

	strncpy(tmp, in, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';

	for (tok = strtok(tmp, ":"); tok && (i < 6); tok = strtok(NULL, ":")) {
		unsigned long ul = strtoul(tok, NULL, 16);
		out[i++] = ul & 0xff;
	}

	return 0;
}

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/socket.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if_dl.h>
#include <net/if_types.h>

/*! Obtain the MAC address of a given network device
 *  \param[out] mac_out pointer to caller-allocated buffer of 6 bytes
 *  \param[in] dev_name string name of the network device
 *  \returns 0 in case of success; negative otherwise
 */
int osmo_get_macaddr(uint8_t *mac_out, const char *dev_name)
{
	struct ifaddrs *ifa, *ifaddr;
	int rc = -ENODEV;

	if (getifaddrs(&ifaddr) != 0)
		return -errno;

	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		struct sockaddr_dl *sdl;

		sdl = (struct sockaddr_dl *) ifa->ifa_addr;
		if (!sdl)
			continue;
		if (sdl->sdl_family != AF_LINK)
			continue;
		if (sdl->sdl_type != IFT_ETHER)
			continue;
		if (strcmp(ifa->ifa_name, dev_name) != 0)
			continue;

		memcpy(mac_out, LLADDR(sdl), 6);
		rc = 0;
		break;
	}

	freeifaddrs(ifaddr);
	return rc;
}

#else

#if (!EMBEDDED)

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>

/*! Obtain the MAC address of a given network device
 *  \param[out] mac_out pointer to caller-allocated buffer of 6 bytes
 *  \param[in] dev_name string name of the network device
 *  \returns 0 in case of success; negative otherwise
 */
int osmo_get_macaddr(uint8_t *mac_out, const char *dev_name)
{
	int fd, rc, dev_len;
	struct ifreq ifr;

	dev_len = strlen(dev_name);
	if (dev_len >= sizeof(ifr.ifr_name))
		return -EINVAL;

	fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (fd < 0)
		return fd;

	memset(&ifr, 0, sizeof(ifr));
	memcpy(&ifr.ifr_name, dev_name, dev_len + 1);
	rc = ioctl(fd, SIOCGIFHWADDR, &ifr);
	close(fd);

	if (rc < 0)
		return rc;

	memcpy(mac_out, ifr.ifr_hwaddr.sa_data, 6);

	return 0;
}
#endif /* !EMBEDDED */

#endif

/*! @} */
