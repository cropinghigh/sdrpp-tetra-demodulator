/*! \file osmo_io_internal.h */

#pragma once

#include <unistd.h>
#include <stdbool.h>
// #include <netinet/sctp.h>

#include <osmocom/core/osmo_io.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/select.h>
#include <osmocom/core/socket.h>

#include "osmo_config.h"

#define OSMO_IO_DEFAULT_MSGB_SIZE 1024
#define OSMO_IO_DEFAULT_MSGB_HEADROOM 128

extern const struct iofd_backend_ops iofd_poll_ops;
#define OSMO_IO_BACKEND_DEFAULT "POLL"

#if defined(HAVE_URING)
extern const struct iofd_backend_ops iofd_uring_ops;
#endif

struct iofd_backend_ops {
	int (*register_fd)(struct osmo_io_fd *iofd);
	int (*unregister_fd)(struct osmo_io_fd *iofd);
	int (*close)(struct osmo_io_fd *iofd);
	void (*write_enable)(struct osmo_io_fd *iofd);
	void (*write_disable)(struct osmo_io_fd *iofd);
	void (*read_enable)(struct osmo_io_fd *iofd);
	void (*read_disable)(struct osmo_io_fd *iofd);
	void (*notify_connected)(struct osmo_io_fd *iofd);
};

#define IOFD_FLAG_CLOSED (1<<0)
#define IOFD_FLAG_IN_CALLBACK (1<<1)
#define IOFD_FLAG_TO_FREE (1<<2)
#define IOFD_FLAG_NOTIFY_CONNECTED (1<<3)
#define IOFD_FLAG_FD_REGISTERED (1<<4)

#define IOFD_FLAG_SET(iofd, flag) \
	(iofd)->flags |= (flag)

#define IOFD_FLAG_UNSET(iofd, flag) \
	(iofd)->flags &= ~(flag)

#define IOFD_FLAG_ISSET(iofd, flag) ((iofd)->flags & (flag))

struct osmo_io_fd {
	/*! linked list for internal management */
	struct llist_head list;
	/*! actual operating-system level file decriptor */
	int fd;
	/*! type of read/write mode to use */
	enum osmo_io_fd_mode mode;

	/*! flags to guard closing/freeing of iofd */
	uint32_t flags;

	/*! human-readable name to associte with fd */
	char *name;

	/*! send/recv (msg) callback functions */
	struct osmo_io_ops io_ops;
	/*! Pending msgb to keep partial data during segmentation */
	struct msgb *pending;

	/*! data pointer passed through to call-back function */
	void *data;
	/*! private number, extending \a data */
	unsigned int priv_nr;

	/*! size of iofd_msghdr.cmsg[] when allocated in recvmsg path */
	size_t cmsg_size;

	struct {
		/*! talloc context from which to allocate msgb when reading */
		const void *ctx;
		/*! size of msgb to allocate (excluding headroom) */
		unsigned int size;
		/*! headroom to allocate when allocating msgb's */
		unsigned int headroom;
	} msgb_alloc;

	struct {
		/*! maximum length of write queue */
		unsigned int max_length;
		/*! current length of write queue */
		unsigned int current_length;
		/*! actual linked list implementing the transmit queue */
		struct llist_head msg_queue;
	} tx_queue;

	union {
		struct {
			struct osmo_fd ofd;
		} poll;
		struct {
			bool read_enabled;
			bool write_enabled;
			void *read_msghdr;
			void *write_msghdr;
			/* TODO: index into array of registered fd's? */
			/* osmo_fd for non-blocking connect handling */
			struct osmo_fd connect_ofd;
		} uring;
	} u;
};

enum iofd_msg_action {
	IOFD_ACT_READ,
	IOFD_ACT_WRITE,
	IOFD_ACT_RECVFROM,
	IOFD_ACT_SENDTO,
	IOFD_ACT_RECVMSG,
	IOFD_ACT_SENDMSG,
};


/*! serialized version of 'struct msghdr' employed by sendmsg/recvmsg */
struct iofd_msghdr {
	/*! entry into osmo_io_fd.tx_queue.msg_queue */
	struct llist_head list;
	enum iofd_msg_action action;
	/*! the 'struct msghdr' we are wrapping/ecapsulating here */
	struct msghdr hdr;
	/*! socket address of the remote peer */
	struct osmo_sockaddr osa;
	/*! io-vector we need to pass as argument to sendmsg/recvmsg; is set up
	 * to point into msg below */
	struct iovec iov[1];
	/*! flags we pass as argument to sendmsg / recvmsg */
	int flags;

	/*! message-buffer containing data for this I/O operation */
	struct msgb *msg;
	/*! I/O file descriptor on which we perform this I/O operation */
	struct osmo_io_fd *iofd;

	/*! control message buffer for passing sctp_sndrcvinfo along */
	char cmsg[0]; /* size is determined by iofd->cmsg_size on recvmsg, and by mcghdr->msg_controllen on sendmsg */
};

enum iofd_seg_act {
	IOFD_SEG_ACT_HANDLE_ONE,
	IOFD_SEG_ACT_HANDLE_MORE,
	IOFD_SEG_ACT_DEFER,
};

struct iofd_msghdr *iofd_msghdr_alloc(struct osmo_io_fd *iofd, enum iofd_msg_action action, struct msgb *msg, size_t cmsg_size);
void iofd_msghdr_free(struct iofd_msghdr *msghdr);

struct msgb *iofd_msgb_alloc(struct osmo_io_fd *iofd);
struct msgb *iofd_msgb_pending(struct osmo_io_fd *iofd);
struct msgb *iofd_msgb_pending_or_alloc(struct osmo_io_fd *iofd);

void iofd_handle_recv(struct osmo_io_fd *iofd, struct msgb *msg, int rc, struct iofd_msghdr *msghdr);
void iofd_handle_send_completion(struct osmo_io_fd *iofd, int rc, struct iofd_msghdr *msghdr);
void iofd_handle_segmented_read(struct osmo_io_fd *iofd, struct msgb *msg, int rc);

int iofd_txqueue_enqueue(struct osmo_io_fd *iofd, struct iofd_msghdr *msghdr);
void iofd_txqueue_enqueue_front(struct osmo_io_fd *iofd, struct iofd_msghdr *msghdr);
struct iofd_msghdr *iofd_txqueue_dequeue(struct osmo_io_fd *iofd);
