/*! \file sockaddr_str.h
 * Common API to store an IP address and port.
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

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <osmocom/core/defs.h>

struct in_addr;
struct in6_addr;
struct sockaddr_storage;
struct sockaddr_in;
struct sockaddr_in6;
struct osmo_sockaddr;

/*! \defgroup sockaddr_str  IP address/port utilities.
 * @{
 * \file sockaddr_str.h
 */

int osmo_ip_str_type(const char *ip);

struct osmo_sockaddr_str {
	/*! AF_INET for IPv4 address, or AF_INET6 for IPv6 address. */
	int af;
	/*! NUL terminated string of the IPv4 or IPv6 address. */
	char ip[INET6_ADDRSTRLEN];
	/*! Port number */
	uint16_t port;
};

/*! Format string to print an osmo_sockaddr_str.
 *
 * For example:
 *
 *     struct osmo_sockaddr_str *my_sockaddr_str = ...;
 *     printf("got " OSMO_SOCKADDR_STR_FMT "\n", OSMO_SOCKADDR_STR_FMT_ARGS(my_sockaddr_str));
 */
#define OSMO_SOCKADDR_STR_FMT "%s%s%s:%u"
#define OSMO_SOCKADDR_STR_FMT_ARGS_NOT_NULL(R) \
	((R)->af == AF_INET6) ? "[" : "", \
	(R)->ip, \
	((R)->af == AF_INET6) ? "]" : "", \
	(R)->port
#define OSMO_SOCKADDR_STR_FMT_ARGS(R) \
	((R) && (R)->af == AF_INET6) ? "[" : "", \
	(R) ? (R)->ip : "NULL", \
	((R) && (R)->af == AF_INET6) ? "]" : "", \
	(R) ? (R)->port : 0

bool osmo_sockaddr_str_is_set(const struct osmo_sockaddr_str *sockaddr_str);
bool osmo_sockaddr_str_is_nonzero(const struct osmo_sockaddr_str *sockaddr_str);
int osmo_sockaddr_str_cmp(const struct osmo_sockaddr_str *a, const struct osmo_sockaddr_str *b);

int osmo_sockaddr_str_from_str(struct osmo_sockaddr_str *sockaddr_str, const char *ip, uint16_t port);
int osmo_sockaddr_str_from_str2(struct osmo_sockaddr_str *sockaddr_str, const char *ip);

int osmo_sockaddr_str_from_in_addr(struct osmo_sockaddr_str *sockaddr_str, const struct in_addr *addr, uint16_t port);
int osmo_sockaddr_str_from_in6_addr(struct osmo_sockaddr_str *sockaddr_str, const struct in6_addr *addr, uint16_t port);
int osmo_sockaddr_str_from_32(struct osmo_sockaddr_str *sockaddr_str, uint32_t ip, uint16_t port);
int osmo_sockaddr_str_from_32h(struct osmo_sockaddr_str *sockaddr_str, uint32_t ip, uint16_t port);
int osmo_sockaddr_str_from_sockaddr_in(struct osmo_sockaddr_str *sockaddr_str, const struct sockaddr_in *src);
int osmo_sockaddr_str_from_sockaddr_in6(struct osmo_sockaddr_str *sockaddr_str, const struct sockaddr_in6 *src);
int osmo_sockaddr_str_from_sockaddr(struct osmo_sockaddr_str *sockaddr_str, const struct sockaddr_storage *src);
int osmo_sockaddr_str_from_osa(struct osmo_sockaddr_str *sockaddr_str, const struct osmo_sockaddr *src);

int osmo_sockaddr_str_to_in_addr(const struct osmo_sockaddr_str *sockaddr_str, struct in_addr *dst);
int osmo_sockaddr_str_to_in6_addr(const struct osmo_sockaddr_str *sockaddr_str, struct in6_addr *dst);
int osmo_sockaddr_str_to_32(const struct osmo_sockaddr_str *sockaddr_str, uint32_t *ip);
int osmo_sockaddr_str_to_32h(const struct osmo_sockaddr_str *sockaddr_str, uint32_t *ip);
int osmo_sockaddr_str_to_sockaddr_in(const struct osmo_sockaddr_str *sockaddr_str, struct sockaddr_in *dst);
int osmo_sockaddr_str_to_sockaddr_in6(const struct osmo_sockaddr_str *sockaddr_str, struct sockaddr_in6 *dst);
int osmo_sockaddr_str_to_sockaddr(const struct osmo_sockaddr_str *sockaddr_str, struct sockaddr_storage *dst);
int osmo_sockaddr_str_to_osa(const struct osmo_sockaddr_str *sockaddr_str, struct osmo_sockaddr *dst);

int osmo_sockaddr_str_from_32n(struct osmo_sockaddr_str *sockaddr_str, uint32_t ip, uint16_t port)
	OSMO_DEPRECATED("osmo_sockaddr_str_from_32n() actually uses *host* byte order. Use osmo_sockaddr_str_from_32h() instead");
int osmo_sockaddr_str_to_32n(const struct osmo_sockaddr_str *sockaddr_str, uint32_t *ip)
	OSMO_DEPRECATED("osmo_sockaddr_str_to_32n() actually uses *host* byte order. Use osmo_sockaddr_str_to_32h() instead");

/*! @} */
