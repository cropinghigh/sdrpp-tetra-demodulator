/*! \file sockaddr_str.c
 * Common implementation to store an IP address and port.
 */
/*
 * (C) 2019 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
 * Author: neels@hofmeyr.de
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

#ifdef HAVE_NETINET_IN_H
#include <string.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <osmocom/core/sockaddr_str.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/byteswap.h>
#include <osmocom/core/socket.h>

/*! \addtogroup sockaddr_str
 *
 * Common operations to store IP address as a char string along with a uint16_t port number.
 *
 * Convert IP address string to/from in_addr and in6_addr, with bounds checking and basic housekeeping.
 *
 * The initial purpose is to store and translate IP address info between GSM CC and MGCP protocols -- GSM mostly using
 * 32-bit IPv4 addresses, and MGCP forwarding addresses as ASCII character strings.
 *
 * (At the time of writing, there are no immediate IPv6 users that come to mind, but it seemed appropriate to
 * accommodate both address families from the start.)
 *
 * @{
 * \file sockaddr_str.c
 */

/*! Return true if all elements of the osmo_sockaddr_str instance are set.
 * \param[in] sockaddr_str  The instance to examine.
 * \return True iff ip is nonempty, port is not 0 and af is set to either AF_INET or AF_INET6.
 */
bool osmo_sockaddr_str_is_set(const struct osmo_sockaddr_str *sockaddr_str)
{
	return sockaddr_str
		&& *sockaddr_str->ip
		&& sockaddr_str->port
		&& (sockaddr_str->af == AF_INET || sockaddr_str->af == AF_INET6);
}

/*! Return true if IP and port are valid and nonzero.
 * \param[in] sockaddr_str  The instance to examine.
 * \return True iff ip can be converted to a nonzero IP address, and port is not 0.
 */
bool osmo_sockaddr_str_is_nonzero(const struct osmo_sockaddr_str *sockaddr_str)
{
	uint32_t ipv4;
	struct in6_addr ipv6_zero = {};
	struct in6_addr ipv6;

	if (!osmo_sockaddr_str_is_set(sockaddr_str))
		return false;

	switch (sockaddr_str->af) {
	case AF_INET:
		if (osmo_sockaddr_str_to_32(sockaddr_str, &ipv4))
			return false;
		return ipv4 != 0;

	case AF_INET6:
		if (osmo_sockaddr_str_to_in6_addr(sockaddr_str, &ipv6))
			return false;
		return memcmp(&ipv6, &ipv6_zero, sizeof(ipv6)) != 0;

	default:
		return false;
	}
}

/*! Compare two osmo_sockaddr_str instances by string comparison.
 * Compare by strcmp() for the address and compare port numbers, ignore the AF_INET/AF_INET6 value.
 * \param[in] a  left side of comparison.
 * \param[in] b  right side of comparison.
 * \return -1 if a < b, 0 if a == b, 1 if a > b.
 */
static int osmo_sockaddr_str_cmp_by_string(const struct osmo_sockaddr_str *a, const struct osmo_sockaddr_str *b)
{
	int cmp;
	if (a == b)
		return 0;
	if (!a)
		return -1;
	if (!b)
		return 1;
	cmp = strncmp(a->ip, b->ip, sizeof(a->ip));
	if (cmp)
		return cmp;
	return OSMO_CMP(a->port, b->port);
}

/*! Compare two osmo_sockaddr_str instances by resulting IP address.
 * Compare IP versions (AF_INET vs AF_INET6), compare resulting IP address bytes and compare port numbers.
 * If the IP address strings cannot be parsed successfully / if the 'af' is neither AF_INET nor AF_INET6, fall back to
 * pure string comparison of the ip address.
 * \param[in] a  left side of comparison.
 * \param[in] b  right side of comparison.
 * \return -1 if a < b, 0 if a == b, 1 if a > b.
 */
int osmo_sockaddr_str_cmp(const struct osmo_sockaddr_str *a, const struct osmo_sockaddr_str *b)
{
	int cmp;
	uint32_t ipv4_a, ipv4_b;
	struct in6_addr ipv6_a = {}, ipv6_b = {};

	if (a == b)
		return 0;
	if (!a)
		return -1;
	if (!b)
		return 1;
	cmp = OSMO_CMP(a->af, b->af);
	if (cmp)
		return cmp;
	switch (a->af) {
	case AF_INET:
		if (osmo_sockaddr_str_to_32(a, &ipv4_a)
		    || osmo_sockaddr_str_to_32(b, &ipv4_b))
			goto fallback_to_strcmp;
		cmp = OSMO_CMP(ipv4_a, ipv4_b);
		break;

	case AF_INET6:
		if (osmo_sockaddr_str_to_in6_addr(a, &ipv6_a)
		    || osmo_sockaddr_str_to_in6_addr(b, &ipv6_b))
			goto fallback_to_strcmp;
		cmp = memcmp(&ipv6_a, &ipv6_b, sizeof(ipv6_a));
		break;

	default:
		goto fallback_to_strcmp;
	}
	if (cmp)
		return cmp;

	cmp = OSMO_CMP(a->port, b->port);
	if (cmp)
		return cmp;
	return 0;

fallback_to_strcmp:
	return osmo_sockaddr_str_cmp_by_string(a, b);
}

/*! Distinguish between valid IPv4 and IPv6 strings.
 * This does not verify whether the string is a valid IP address; it assumes that the input is a valid IP address, and
 * on that premise returns whether it is an IPv4 or IPv6 string, by looking for '.' and ':' characters.  It is safe to
 * feed invalid address strings, but the return value is only guaranteed to be meaningful if the input was valid.
 * \param[in] ip  Valid IP address string.
 * \return AF_INET or AF_INET6, or AF_UNSPEC if neither '.' nor ':' are found in the string.
 */
int osmo_ip_str_type(const char *ip)
{
	if (!ip)
		return AF_UNSPEC;
	/* Could also be IPv4-mapped IPv6 format with both colons and dots: x:x:x:x:x:x:d.d.d.d */
	if (strchr(ip, ':'))
		return AF_INET6;
	if (strchr(ip, '.'))
		return AF_INET;
	return AF_UNSPEC;
}

/*! Safely copy the given ip string to sockaddr_str, classify to AF_INET or AF_INET6.
 * Data will be written to sockaddr_str even if an error is returned.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] ip  Valid IP address string.
 * \return 0 on success, negative if copying the address string failed (e.g. too long), if the address family could
 *         not be detected (i.e. if osmo_ip_str_type() returned AF_UNSPEC), or if sockaddr_str is NULL.
 */
int osmo_sockaddr_str_from_str2(struct osmo_sockaddr_str *sockaddr_str, const char *ip)
{
	int rc;
	if (!sockaddr_str)
		return -ENOSPC;
	if (!ip)
		ip = "";
	sockaddr_str->af = osmo_ip_str_type(ip);
	/* to be compatible with previous behaviour, zero the full IP field.
	 * Allow the usage of memcmp(&sockaddr_str, ...) */
	memset(sockaddr_str->ip, 0x0, sizeof(sockaddr_str->ip));
	rc = osmo_strlcpy(sockaddr_str->ip, ip, sizeof(sockaddr_str->ip));
	if (rc <= 0)
		return -EIO;
	if (rc >= sizeof(sockaddr_str->ip))
		return -ENOSPC;
	if (sockaddr_str->af == AF_UNSPEC)
		return -EINVAL;
	return 0;
}

/*! Safely copy the given ip string to sockaddr_str, classify to AF_INET or AF_INET6, and set the port.
 * Data will be written to sockaddr_str even if an error is returned.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] ip  Valid IP address string.
 * \param[in] port  Port number.
 * \return 0 on success, negative if copying the address string failed (e.g. too long), if the address family could
 *         not be detected (i.e. if osmo_ip_str_type() returned AF_UNSPEC), or if sockaddr_str is NULL.
 */
int osmo_sockaddr_str_from_str(struct osmo_sockaddr_str *sockaddr_str, const char *ip, uint16_t port)
{
	int rc;
	if (!sockaddr_str)
		return -ENOSPC;

	rc = osmo_sockaddr_str_from_str2(sockaddr_str, ip);
	sockaddr_str->port = port;

	return rc;
}

/*! Convert IPv4 address to osmo_sockaddr_str, and set port.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] addr  IPv4 address data.
 * \param[in] port  Port number.
 * \return 0 on success, negative on error.
 */
int osmo_sockaddr_str_from_in_addr(struct osmo_sockaddr_str *sockaddr_str, const struct in_addr *addr, uint16_t port)
{
	if (!sockaddr_str)
		return -ENOSPC;
	*sockaddr_str = (struct osmo_sockaddr_str){
		.af = AF_INET,
		.port = port,
	};
	if (!inet_ntop(AF_INET, addr, sockaddr_str->ip, sizeof(sockaddr_str->ip)))
		return -ENOSPC;
	return 0;
}

/*! Convert IPv6 address to osmo_sockaddr_str, and set port.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] addr  IPv6 address data.
 * \param[in] port  Port number.
 * \return 0 on success, negative on error.
 */
int osmo_sockaddr_str_from_in6_addr(struct osmo_sockaddr_str *sockaddr_str, const struct in6_addr *addr, uint16_t port)
{
	if (!sockaddr_str)
		return -ENOSPC;
	*sockaddr_str = (struct osmo_sockaddr_str){
		.af = AF_INET6,
		.port = port,
	};
	if (!inet_ntop(AF_INET6, addr, sockaddr_str->ip, sizeof(sockaddr_str->ip)))
		return -ENOSPC;
	return 0;
}

/*! Convert IPv4 address from 32bit network-byte-order to osmo_sockaddr_str, and set port.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] addr  32bit IPv4 address data.
 * \param[in] port  Port number.
 * \return 0 on success, negative on error.
 */
int osmo_sockaddr_str_from_32(struct osmo_sockaddr_str *sockaddr_str, uint32_t ip, uint16_t port)
{
	struct in_addr addr;
	if (!sockaddr_str)
		return -ENOSPC;
	addr.s_addr = ip;
	return osmo_sockaddr_str_from_in_addr(sockaddr_str, &addr, port);
}

/*! Convert IPv4 address from 32bit host-byte-order to osmo_sockaddr_str, and set port.
 * For legacy reasons, this function has a misleading 'n' in its name.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] addr  32bit IPv4 address data.
 * \param[in] port  Port number.
 * \return 0 on success, negative on error.
 */
int osmo_sockaddr_str_from_32h(struct osmo_sockaddr_str *sockaddr_str, uint32_t ip, uint16_t port)
{
	if (!sockaddr_str)
		return -ENOSPC;
	return osmo_sockaddr_str_from_32(sockaddr_str, osmo_ntohl(ip), port);
}

/*! DEPRECATED: the name suggests a conversion from network byte order, but actually converts from host byte order. Use
 * osmo_sockaddr_str_from_32 for network byte order and osmo_sockaddr_str_from_32h for host byte order. */
int osmo_sockaddr_str_from_32n(struct osmo_sockaddr_str *sockaddr_str, uint32_t ip, uint16_t port)
{
	return osmo_sockaddr_str_from_32h(sockaddr_str, ip, port);
}

/*! Convert IPv4 address and port to osmo_sockaddr_str.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] src  IPv4 address and port data.
 * \return 0 on success, negative on error.
 */
int osmo_sockaddr_str_from_sockaddr_in(struct osmo_sockaddr_str *sockaddr_str, const struct sockaddr_in *src)
{
	if (!sockaddr_str)
		return -ENOSPC;
	if (!src)
		return -EINVAL;
	if (src->sin_family != AF_INET)
		return -EINVAL;
	return osmo_sockaddr_str_from_in_addr(sockaddr_str, &src->sin_addr, osmo_ntohs(src->sin_port));
}

/*! Convert IPv6 address and port to osmo_sockaddr_str.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] src  IPv6 address and port data.
 * \return 0 on success, negative on error.
 */
int osmo_sockaddr_str_from_sockaddr_in6(struct osmo_sockaddr_str *sockaddr_str, const struct sockaddr_in6 *src)
{
	if (!sockaddr_str)
		return -ENOSPC;
	if (!src)
		return -EINVAL;
	if (src->sin6_family != AF_INET6)
		return -EINVAL;
	return osmo_sockaddr_str_from_in6_addr(sockaddr_str, &src->sin6_addr, osmo_ntohs(src->sin6_port));
}

/*! Convert IPv4 or IPv6 address and port to osmo_sockaddr_str.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] src  IPv4 or IPv6 address and port data.
 * \return 0 on success, negative if src does not indicate AF_INET nor AF_INET6 (or if the conversion fails, which
 *         should not be possible in practice).
 */
int osmo_sockaddr_str_from_sockaddr(struct osmo_sockaddr_str *sockaddr_str, const struct sockaddr_storage *src)
{
	const struct sockaddr_in *sin = (void*)src;
	const struct sockaddr_in6 *sin6 = (void*)src;
	if (!sockaddr_str)
		return -ENOSPC;
	if (!src)
		return -EINVAL;
	if (sin->sin_family == AF_INET)
		return osmo_sockaddr_str_from_sockaddr_in(sockaddr_str, sin);
	if (sin6->sin6_family == AF_INET6)
		return osmo_sockaddr_str_from_sockaddr_in6(sockaddr_str, sin6);
	return -EINVAL;
}

/*! Convert IPv4 or IPv6 address and port to osmo_sockaddr_str.
 * \param[out] sockaddr_str  The instance to copy to.
 * \param[in] src  IPv4 or IPv6 address and port data.
 * \return 0 on success, negative if src does not indicate AF_INET nor AF_INET6 (or if the conversion fails, which
 *         should not be possible in practice).
 */
int osmo_sockaddr_str_from_osa(struct osmo_sockaddr_str *sockaddr_str, const struct osmo_sockaddr *src)
{
	return osmo_sockaddr_str_from_sockaddr(sockaddr_str, &src->u.sas);
}

/*! Convert osmo_sockaddr_str address string to IPv4 address data.
 * \param[in] sockaddr_str  The instance to convert the IP of.
 * \param[out] dst  IPv4 address data to write to.
 * \return 0 on success, negative on error (e.g. invalid IPv4 address string).
 */
int osmo_sockaddr_str_to_in_addr(const struct osmo_sockaddr_str *sockaddr_str, struct in_addr *dst)
{
	int rc;
	if (!sockaddr_str)
		return -EINVAL;
	if (!dst)
		return -ENOSPC;
	if (sockaddr_str->af != AF_INET)
		return -EAFNOSUPPORT;
	rc = inet_pton(AF_INET, sockaddr_str->ip, dst);
	if (rc != 1)
		return -EINVAL;
	return 0;
}

/*! Convert osmo_sockaddr_str address string to IPv6 address data.
 * \param[in] sockaddr_str  The instance to convert the IP of.
 * \param[out] dst  IPv6 address data to write to.
 * \return 0 on success, negative on error (e.g. invalid IPv6 address string).
 */
int osmo_sockaddr_str_to_in6_addr(const struct osmo_sockaddr_str *sockaddr_str, struct in6_addr *dst)
{
	int rc;
	if (!sockaddr_str)
		return -EINVAL;
	if (!dst)
		return -ENOSPC;
	if (sockaddr_str->af != AF_INET6)
		return -EINVAL;
	rc = inet_pton(AF_INET6, sockaddr_str->ip, dst);
	if (rc != 1)
		return -EINVAL;
	return 0;
}

/*! Convert osmo_sockaddr_str address string to IPv4 address data in network-byte-order.
 * \param[in] sockaddr_str  The instance to convert the IP of.
 * \param[out] dst  IPv4 address data in 32bit network-byte-order format to write to.
 * \return 0 on success, negative on error (e.g. invalid IPv4 address string).
 */
int osmo_sockaddr_str_to_32(const struct osmo_sockaddr_str *sockaddr_str, uint32_t *ip)
{
	int rc;
	struct in_addr addr;
	if (!sockaddr_str)
		return -EINVAL;
	if (!ip)
		return -ENOSPC;
	rc = osmo_sockaddr_str_to_in_addr(sockaddr_str, &addr);
	if (rc)
		return rc;
	*ip = addr.s_addr;
	return 0;
}

/*! Convert osmo_sockaddr_str address string to IPv4 address data in host-byte-order.
 * For legacy reasons, this function has a misleading 'n' in its name.
 * \param[in] sockaddr_str  The instance to convert the IP of.
 * \param[out] dst  IPv4 address data in 32bit host-byte-order format to write to.
 * \return 0 on success, negative on error (e.g. invalid IPv4 address string).
 */
int osmo_sockaddr_str_to_32h(const struct osmo_sockaddr_str *sockaddr_str, uint32_t *ip)
{
	int rc;
	uint32_t ip_h;
	if (!sockaddr_str)
		return -EINVAL;
	if (!ip)
		return -ENOSPC;
	rc = osmo_sockaddr_str_to_32(sockaddr_str, &ip_h);
	if (rc)
		return rc;
	*ip = osmo_htonl(ip_h);
	return 0;
}

/*! DEPRECATED: the name suggests a conversion to network byte order, but actually converts to host byte order. Use
 * osmo_sockaddr_str_to_32() for network byte order and osmo_sockaddr_str_to_32h() for host byte order. */
int osmo_sockaddr_str_to_32n(const struct osmo_sockaddr_str *sockaddr_str, uint32_t *ip)
{
	return osmo_sockaddr_str_to_32h(sockaddr_str, ip);
}

/*! Convert osmo_sockaddr_str address string and port to IPv4 address and port data.
 * \param[in] sockaddr_str  The instance to convert the IP and port of.
 * \param[out] dst  IPv4 address and port data to write to.
 * \return 0 on success, negative on error (e.g. invalid IPv4 address string).
 */
int osmo_sockaddr_str_to_sockaddr_in(const struct osmo_sockaddr_str *sockaddr_str, struct sockaddr_in *dst)
{
	if (!sockaddr_str)
		return -EINVAL;
	if (!dst)
		return -ENOSPC;
	if (sockaddr_str->af != AF_INET)
		return -EINVAL;
	*dst = (struct sockaddr_in){
		.sin_family = sockaddr_str->af,
		.sin_port = osmo_htons(sockaddr_str->port),
	};
	return osmo_sockaddr_str_to_in_addr(sockaddr_str, &dst->sin_addr);
}

/*! Convert osmo_sockaddr_str address string and port to IPv6 address and port data.
 * \param[in] sockaddr_str  The instance to convert the IP and port of.
 * \param[out] dst  IPv6 address and port data to write to.
 * \return 0 on success, negative on error (e.g. invalid IPv6 address string).
 */
int osmo_sockaddr_str_to_sockaddr_in6(const struct osmo_sockaddr_str *sockaddr_str, struct sockaddr_in6 *dst)
{
	if (!sockaddr_str)
		return -EINVAL;
	if (!dst)
		return -ENOSPC;
	if (sockaddr_str->af != AF_INET6)
		return -EINVAL;
	*dst = (struct sockaddr_in6){
		.sin6_family = sockaddr_str->af,
		.sin6_port = osmo_htons(sockaddr_str->port),
	};
	return osmo_sockaddr_str_to_in6_addr(sockaddr_str, &dst->sin6_addr);
}

/*! Convert osmo_sockaddr_str address string and port to IPv4 or IPv6 address and port data.
 * Depending on sockaddr_str->af, dst will be handled as struct sockaddr_in or struct sockaddr_in6.
 * \param[in] sockaddr_str  The instance to convert the IP and port of.
 * \param[out] dst  IPv4/IPv6 address and port data to write to.
 * \return 0 on success, negative on error (e.g. invalid IP address string for the family indicated by sockaddr_str->af).
 */
int osmo_sockaddr_str_to_sockaddr(const struct osmo_sockaddr_str *sockaddr_str, struct sockaddr_storage *dst)
{
	if (!sockaddr_str)
		return -EINVAL;
	if (!dst)
		return -ENOSPC;
	switch (sockaddr_str->af) {
	case AF_INET:
		return osmo_sockaddr_str_to_sockaddr_in(sockaddr_str, (void*)dst);
	case AF_INET6:
		return osmo_sockaddr_str_to_sockaddr_in6(sockaddr_str, (void*)dst);
	default:
		return -EINVAL;
	}
}

/*! Convert osmo_sockaddr_str address string and port to IPv4 or IPv6 address and port data.
 * \param[in] sockaddr_str  The instance to convert the IP and port of.
 * \param[out] dst  IPv4/IPv6 address and port data to write to.
 * \return 0 on success, negative on error (e.g. invalid IP address string for the family indicated by sockaddr_str->af).
 */
int osmo_sockaddr_str_to_osa(const struct osmo_sockaddr_str *sockaddr_str, struct osmo_sockaddr *dst)
{
	return osmo_sockaddr_str_to_sockaddr(sockaddr_str, &dst->u.sas);
}

/*! @} */
#endif // HAVE_NETINET_IN_H
