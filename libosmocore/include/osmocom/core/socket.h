/*! \file socket.h
 *  Osmocom socket convenience functions. */

#pragma once
#if (!EMBEDDED)

/*! \defgroup socket Socket convenience functions
 *  @{
 *  \file socket.h */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <arpa/inet.h>

#include <osmocom/core/defs.h>

/*! maximum number of local or remote addresses supported by an osmo_sock instance */
#define OSMO_SOCK_MAX_ADDRS 32

/*! maximum length of a socket name ("r=1.2.3.4:123<->l=5.6.7.8:987") */
#define OSMO_SOCK_NAME_MAXLEN (2 + INET6_ADDRSTRLEN + 1 + 5 + 3 + 2 + INET6_ADDRSTRLEN + 1 + 5 + 1)

/*! maximum length of a multi-address socket peer (endpoint) name: (5.6.7.8|::9):987
 * char '(' + OSMO_STREAM_MAX_ADDRS - 1 addr separators + chars "):" + port buffer + char '\0'
 */
#define OSMO_SOCK_MULTIADDR_PEER_STR_MAXLEN (INET6_ADDRSTRLEN * OSMO_SOCK_MAX_ADDRS + INET6_ADDRSTRLEN + 2 + 6 + 1)
/*! maximum length of a multia-address socket name ("r=(::2|1.2.3.4):123<->l=(5.6.7.8|::9):987") */
#define OSMO_SOCK_MULTIADDR_NAME_MAXLEN (OSMO_SOCK_MULTIADDR_PEER_STR_MAXLEN + 7)


struct sockaddr_in;
struct sockaddr;
struct osmo_fd;
struct sctp_paddrinfo;

struct osmo_sockaddr {
	union {
		struct sockaddr sa;
		struct sockaddr_storage sas;
		struct sockaddr_in sin;
		struct sockaddr_in6 sin6;
	} u;
};

int osmo_sockaddr_is_local(struct sockaddr *addr, unsigned int addrlen);
int osmo_sockaddr_is_any(const struct osmo_sockaddr *addr);

/*! Return the size of the variant used in the address
 *  NOTE: This does not return the size of the in{,6}_addr, but rather the size of the
 *  surrounding sockaddr_in{,6}.
 *  \param[in] addr the osmo_sockaddr to get the size of
 *  \return the size of the struct variant being used. If the value in sa_family is unsupported it will return
 *  the size of struct osmo_sockaddr. Returns 0 if addr is NULL. This way it can simply be a wrapper for sendto()
 *  which can be called with NULL/0 for dest_addr / addrlen (and then behaves like a send() call).
 */
static inline socklen_t osmo_sockaddr_size(const struct osmo_sockaddr *addr)
{
	if (!addr)
		return 0;

	switch (addr->u.sa.sa_family) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
	default:
		return sizeof(struct osmo_sockaddr);
	}
}

unsigned int osmo_sockaddr_to_str_and_uint(char *addr, unsigned int addr_len, uint16_t *port,
					   const struct sockaddr *sa);
size_t osmo_sockaddr_in_to_str_and_uint(char *addr, unsigned int addr_len, uint16_t *port,
					const struct sockaddr_in *sin);

const char *osmo_sockaddr_ntop(const struct sockaddr *sa, char *dst);
uint16_t osmo_sockaddr_port(const struct sockaddr *sa);
void osmo_sockaddr_set_port(struct sockaddr *sa, uint16_t port);

int osmo_sockaddr_local_ip(struct osmo_sockaddr *local_ip,
			   const struct osmo_sockaddr *remote_ip);
int osmo_sockaddr_cmp(const struct osmo_sockaddr *a,
		      const struct osmo_sockaddr *b);

int osmo_sockaddr_to_octets(uint8_t *dst, size_t dst_maxlen, const struct osmo_sockaddr *os);
int osmo_sockaddr_from_octets(struct osmo_sockaddr *os, const void *src, size_t src_len);
int osmo_sockaddr_from_str_and_uint(struct osmo_sockaddr *osa_out, const char *ipstr, uint16_t port);

int osmo_sockaddr_netmask_to_prefixlen(const struct osmo_sockaddr *addr);

const char *osmo_sockaddr_to_str(const struct osmo_sockaddr *sockaddr);
char *osmo_sockaddr_to_str_buf(char *buf, size_t buf_len,
			       const struct osmo_sockaddr *sockaddr);
int osmo_sockaddr_to_str_buf2(char *buf, size_t buf_len, const struct osmo_sockaddr *sockaddr);
char *osmo_sockaddr_to_str_c(void *ctx, const struct osmo_sockaddr *sockaddr);

/* flags for osmo_sock_init. */
/*! connect the socket to a remote peer */
#define OSMO_SOCK_F_CONNECT	(1 << 0)
/*! bind the socket to a local address/port */
#define OSMO_SOCK_F_BIND	(1 << 1)
/*! switch socket to non-blocking mode */
#define OSMO_SOCK_F_NONBLOCK	(1 << 2)
/*! disable multiast loop (IP_MULTICAST_LOOP) */
#define OSMO_SOCK_F_NO_MCAST_LOOP (1 << 3)
/*! disable receiving all multiast even for non-subscribed groups */
#define OSMO_SOCK_F_NO_MCAST_ALL  (1 << 4)
/*! use SO_REUSEADDR on UDP ports (required for multicast) */
#define OSMO_SOCK_F_UDP_REUSEADDR (1 << 5)

/*! use OSMO_SOCK_F_DSCP(x) to set IP DSCP 'x' for packets transmitted on the socket */
#define OSMO_SOCK_F_DSCP(x)	(((x)&0x3f) << 24)
#define GET_OSMO_SOCK_F_DSCP(f)	(((f) >> 24) & 0x3f)

/*! use OSMO_SOCK_F_PRIO(x) to set priority 'x' for packets transmitted on the socket */
#define OSMO_SOCK_F_PRIO(x)	(((x)&0xff) << 16)
#define GET_OSMO_SOCK_F_PRIO(f)	(((f) >> 16) & 0xff)


int osmo_sock_init(uint16_t family, uint16_t type, uint8_t proto,
		   const char *host, uint16_t port, unsigned int flags);

int osmo_sock_init2(uint16_t family, uint16_t type, uint8_t proto,
		   const char *local_host, uint16_t local_port,
		   const char *remote_host, uint16_t remote_port, unsigned int flags);

struct osmo_sock_init2_multiaddr_pars {
	union {
		struct {
			uint8_t version; /* set to 0 */
			struct {
				bool set;
				bool abort_on_failure;
				uint32_t value;
			} sockopt_auth_supported;
			struct {
				bool set;
				bool abort_on_failure;
				uint32_t value;
			} sockopt_asconf_supported;
			struct {
				bool set;
				bool abort_on_failure;
				bool num_ostreams_present;
				bool max_instreams_present;
				bool max_attempts_present;
				bool max_init_timeo_present;
				uint16_t num_ostreams_value;
				uint16_t max_instreams_value;
				uint16_t max_attempts_value;
				uint16_t max_init_timeo_value;
			} sockopt_initmsg;
		} sctp;
	};
};
int osmo_sock_init2_multiaddr(uint16_t family, uint16_t type, uint8_t proto,
		   const char **local_hosts, size_t local_hosts_cnt, uint16_t local_port,
		   const char **remote_hosts, size_t remote_hosts_cnt, uint16_t remote_port, unsigned int flags)
		   OSMO_DEPRECATED_OUTSIDE("Use osmo_sock_init2_multiaddr2() instead");
int osmo_sock_init2_multiaddr2(uint16_t family, uint16_t type, uint8_t proto,
		   const char **local_hosts, size_t local_hosts_cnt, uint16_t local_port,
		   const char **remote_hosts, size_t remote_hosts_cnt, uint16_t remote_port,
		   unsigned int flags, struct osmo_sock_init2_multiaddr_pars *pars);

int osmo_sock_init_osa(uint16_t type, uint8_t proto,
		    const struct osmo_sockaddr *local,
		    const struct osmo_sockaddr *remote,
		    unsigned int flags);

int osmo_sock_init_ofd(struct osmo_fd *ofd, int family, int type, int proto,
			const char *host, uint16_t port, unsigned int flags);

int osmo_sock_init2_ofd(struct osmo_fd *ofd, int family, int type, int proto,
			const char *local_host, uint16_t local_port,
			const char *remote_host, uint16_t remote_port, unsigned int flags);

int osmo_sock_init_osa_ofd(struct osmo_fd *ofd, int type, int proto,
			   const struct osmo_sockaddr *local,
			   const struct osmo_sockaddr *remote,
			   unsigned int flags);

int osmo_sock_init_sa(struct sockaddr *ss, uint16_t type,
		      uint8_t proto, unsigned int flags);

int osmo_sock_unix_init(uint16_t type, uint8_t proto,
			const char *socket_path, unsigned int flags);

int osmo_sock_unix_init_ofd(struct osmo_fd *ofd, uint16_t type, uint8_t proto,
			    const char *socket_path, unsigned int flags);

char *osmo_sock_get_name(const void *ctx, int fd);
const char *osmo_sock_get_name2(int fd);
char *osmo_sock_get_name2_c(const void *ctx, int fd);
int osmo_sock_get_name_buf(char *str, size_t str_len, int fd);
int osmo_sock_get_ip_and_port(int fd, char *ip, size_t ip_len, char *port, size_t port_len, bool local);
int osmo_sock_get_local_ip(int fd, char *host, size_t len);
int osmo_sock_get_local_ip_port(int fd, char *port, size_t len);
int osmo_sock_get_remote_ip(int fd, char *host, size_t len);
int osmo_sock_get_remote_ip_port(int fd, char *port, size_t len);

int osmo_sock_multiaddr_get_ip_and_port(int fd, int ip_proto, char *ip, size_t *ip_cnt, size_t ip_len,
					char *port, size_t port_len, bool local);
int osmo_multiaddr_ip_and_port_snprintf(char *str, size_t str_len,
					const char *ip, size_t ip_cnt, size_t ip_len,
					const char *portbuf);
int osmo_sock_multiaddr_get_name_buf(char *str, size_t str_len, int fd, int sk_proto);
int osmo_sock_multiaddr_add_local_addr(int sfd, const char **addrs, size_t addrs_cnt);
int osmo_sock_multiaddr_del_local_addr(int sfd, const char **addrs, size_t addrs_cnt);

int osmo_sock_mcast_loop_set(int fd, bool enable);
int osmo_sock_mcast_ttl_set(int fd, uint8_t ttl);
int osmo_sock_mcast_all_set(int fd, bool enable);
int osmo_sock_mcast_iface_set(int fd, const char *ifname);
int osmo_sock_mcast_subscribe(int fd, const char *grp_addr);

int osmo_sock_local_ip(char *local_ip, const char *remote_ip);

int osmo_sock_set_dscp(int fd, uint8_t dscp);
int osmo_sock_set_priority(int fd, int prio);

int osmo_sock_sctp_get_peer_addr_info(int fd, struct sctp_paddrinfo *pinfo, size_t *pinfo_cnt);

#endif /* (!EMBEDDED) */
/*! @} */
