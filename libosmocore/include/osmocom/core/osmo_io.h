/*! \file osmo_io.h
 *  io(_uring) abstraction osmo fd compatibility
 */

#pragma once

#include <sys/socket.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/utils.h>

/*! \defgroup osmo_io Osmocom I/O interface
 *  @{
 *
 *  osmo_io is the new (2023) interface for performing asynchronous I/O.
 *  osmo_io encapsulates asynchronous, non-blocking I/O to sockets or other file descriptors
 *  with a submission/completion model.
 *
 *  For writes, the API user submits write requests, and receives
 *  completion call-backs once the write completes.
 *
 *  For reads, the API user specifies the size (and headroom) for message buffers, and osmo_io
 *  internally allocates msgb's accordingly.  Whenever data arrives at the socket/file descriptor,
 *  osmo_io reads the data into such a msgb and hands it to a read-completion call-back provided
 *  by the API user.
 *
 *  A given socket/file descriptor is represented by struct osmo_io_fd.  osmo_io_fd are named,
 *  i.e. the API user can provide a meaningful name describing the purpose (such as protocol/interface or the
 *  name of the remote peer).  This allows osmo_io to log any related [error] messages using this name as
 *  context.
 *
 *  When implementing some SOCK_STREAM / SOCK_SEQPACKET based client/server transports (such as those on top
 *  of TCP or SCTP), you are most likely better off using the osmo_stream_cli / osmo_stream_srv abstractions
 *  provided by libosmo-netif.  They in turn can be used in an osmo_io mode, see the respective documentation.
 *
 *  If you use osmo_io_fd directly, the life-cycle usually will look as follows:
 *
 *  1. open some socket and bind and/or connect it
 *  2. Allocate an osmo_io_fd using osmo_iofd_setup(), configuring the mode and specifying the call-backs
 *  3. Registering it with osmo_iofd_register(), which enables reading
 *  4. Handle inbound data via {read,recvfrom,recvmsg} call-backs; write to it using
 *  osmo_iofd_{write,sendto_sendmsg}_msg()
 *  5. Eventually un-register it using osmo_iofd_unregister(). Afterwards, you can re-cycle the iofd by
 *  calling osmo_iofd_register() with a new file-descriptor, or free it using osmo_iofd_free().
 *
 *  \file osmo_io.h */

/*! log macro used for logging information related to the osmo_io_fd.
 *  \param[in] iofd osmo_io_fd about which we're logging
 *  \param[in] level log-level (LOGL_DEBUG, LOGL_INFO, LOGL_NOTICE, LOGL_ERROR, LOGL_FATAL)
 *  \param[in] fmt printf-style format string
 *  \param[in] args arguments to the format string
 */
#define LOGPIO(iofd, level, fmt, args...) \
	LOGP(DLIO, level, "iofd(%s)" fmt, iofd->name, ## args)

struct osmo_io_fd;

/*! The _mode_ of an osmo_io_fd determines if read/write, recvfrom/sendmsg or recvmsg/sendmsg semantics are
 * used. */
enum osmo_io_fd_mode {
	/*! use read() / write() semantics with read_cb/write_cb in osmo_io_ops */
	OSMO_IO_FD_MODE_READ_WRITE,
	/*! use recvfrom() / sendto() semantics with recvfrom_cb/sendto_cb in osmo_io_ops */
	OSMO_IO_FD_MODE_RECVFROM_SENDTO,
	/*! emulate recvmsg() / sendmsg() semantics with recvmsg_cb/sendto_cb in osmo_io_ops */
	OSMO_IO_FD_MODE_RECVMSG_SENDMSG,
};

/*! The back-end used by osmo_io.  There can be multiple different back-ends available on a given system;
 * only one of it is used for all I/O performed via osmo_io in one given process. */
enum osmo_io_backend {
	/*! classic back-end using poll(2) and direct read/write/recvfrom/sendto/recvmsg/sendmsg syscalls */
	OSMO_IO_BACKEND_POLL,
	/*! back-end using io_uring to perform efficient I/O and reduce syscall overhead */
	OSMO_IO_BACKEND_IO_URING,
};

extern const struct value_string osmo_io_backend_names[];
/*! return the string name of an osmo_io_backend */
static inline const char *osmo_io_backend_name(enum osmo_io_backend val)
{ return get_value_string(osmo_io_backend_names, val); }

extern const struct value_string osmo_iofd_mode_names[];
/*! return the string name of an osmo_io_mode */
static inline const char *osmo_iofd_mode_name(enum osmo_io_fd_mode val)
{ return get_value_string(osmo_iofd_mode_names, val); }

/*! I/O operations (call-back functions) related to an osmo_io_fd */
struct osmo_io_ops {
	/* mode OSMO_IO_FD_MODE_READ_WRITE: */
	struct {
		/*! completion call-back function when something was read from fd. Only valid in
		 * OSMO_IO_FD_MODE_READ_WRITE.
		 *  \param[in] iofd osmo_io_fd for which read() has completed.
		 *  \param[in] res return value of the read() call, or -errno in case of error.
		 *  \param[in] msg message buffer containing the read data. Ownership is transferred to the
		 *  call-back, and it must make sure to msgb_free() it eventually! */
		void (*read_cb)(struct osmo_io_fd *iofd, int res, struct msgb *msg);

		/*! completion call-back function when write issued via osmo_iofd_write_msgb() has completed
		 * on fd. Only valid in OSMO_IO_FD_MODE_READ_WRITE.
		 *  \param[in] iofd on which a write() has completed.
		 *  \param[in] res return value of the write() call, or -errno in case of error.
		 *  \param[in] msg message buffer whose write has completed. Ownership is *not* transferred to the
		 *  call-back; it is automatically freed after the call-back terminates! */
		void (*write_cb)(struct osmo_io_fd *iofd, int res,
				 struct msgb *msg);

		/*! optional call-back function to segment the data at message boundaries.
		 *  \param[in] msg message buffer whose data is to be segmented
		 *  \returns See full function description.
		 *
		 *  This is useful when message boundaries are to be preserved over a SOCK_STREAM transport
		 *  socket like TCP.  Can be NULL for any application not requiring de-segmentation of
		 *  received data.
		 *
		 *  The call-back needs to return the size of the next message. If it returns
		 *  -EAGAIN or a value larger than msgb_length() (message is incomplete)
		 *  osmo_io will wait for more data to be read. Other negative values
		 *  cause the msg to be discarded.
		 *  If a full message was received (segmentation_cb() returns a value <= msgb_length())
		 *  the msgb will be trimmed to size by osmo_io and forwarded to the read call-back. Any
		 *  parsing done to the msgb by segmentation_cb() will be preserved for the read_cb()
		 *  (e.g. setting lxh or msgb->cb).
		 *
		 * Only one (or none) of both segmentation_cb and segmentation_cb2 shall be set.
		 * Having both set will be considered an error during iofd setup. */
		int (*segmentation_cb)(struct msgb *msg);

		/*! optional call-back function to segment the data at message boundaries.
		 *  \param[in] iofd handling msg
		 *  \param[in] msg message buffer whose data is to be segmented
		 *  \returns See full function description.
		 *
		 *  Same as segmentation_cb above, with an extra parameter to have access to the iofd and its
		 *  related functionalities (eg data pointer). This is useful for users requiring to store
		 *  global state or access external objects while segmenting.
		 *
		 * The provided iofd shall not be freed by the user during the callback.
		 *
		 * Only one (or none) of both segmentation_cb and segmentation_cb2 shall be set.
		 * Having both set will be considered an error during iofd setup. */
		int (*segmentation_cb2)(struct osmo_io_fd *iofd, struct msgb *msg);
	};

	/* mode OSMO_IO_FD_MODE_RECVFROM_SENDTO: */
	struct {
		/*! completion call-back function when recvfrom(2) has completed.
		 *  Only valid in OSMO_IO_FD_MODE_RECVFROM_SENDTO.
		 *  \param[in] iofd osmo_io_fd for which recvfrom() has completed.
		 *  \param[in] res return value of the recvfrom() call, or -errno in case of error.
		 *  \param[in] msg message buffer containing the read data. Ownership is transferred to the
		 *  call-back, and it must make sure to msgb_free() it eventually!
		 *  \param[in] saddr socket-address of sender from which data was received. */
		void (*recvfrom_cb)(struct osmo_io_fd *iofd, int res,
				    struct msgb *msg,
				    const struct osmo_sockaddr *saddr);
		/*! completion call-back function when sendto() issued via osmo_iofd_sendto_msgb() has
		 * completed on fd. Only valid in OSMO_IO_FD_MODE_RECVFROM_SENDTO.
		 *  \param[in] iofd on which a sendto() has completed.
		 *  \param[in] res return value of the sendto() call, or -errno in case of error.
		 *  \param[in] msg message buffer whose write has completed. Ownership is *not* transferred to the
		 *  call-back; it is automatically freed after the call-back terminates!
		 *  \param[in] daddr socket-address of destination to which data was sent. */
		void (*sendto_cb)(struct osmo_io_fd *iofd, int res,
				  struct msgb *msg,
				  const struct osmo_sockaddr *daddr);
	};

	/* mode OSMO_IO_FD_MODE_RECVMSG_SENDMSG: */
	struct {
		/*! completion call-back function when recvmsg(2) has completed.
		 *  Only valid in OSMO_IO_FD_MODE_RECVMSG_SENDMSG.
		 *  \param[in] iofd osmo_io_fd for which recvmsg() has completed.
		 *  \param[in] res return value of the recvmsg() call, or -errno in case of error.
		 *  \param[in] msg message buffer containing the read data. Ownership is transferred to the
		 *  call-back, and it must make sure to msgb_free() it eventually!
		 *  \param[in] msgh msghdr containing metadata related to the recvmsg call. Only valid until
		 *  call-back ends. */
		void (*recvmsg_cb)(struct osmo_io_fd *iofd, int res,
				   struct msgb *msg, const struct msghdr *msgh);
		/*! completion call-back function when sendmsg() issued via osmo_iofd_sendmsg_msgb() has
		 * completed on fd. Only valid in Only valid in OSMO_IO_FD_MODE_RECVMSG_SENDMSG.
		 *  \param[in] iofd on which a sendmsg() has completed.
		 *  \param[in] res return value of the sendmsg() call, or -errno in case of error.
		 *  \param[in] msg message buffer whose write has completed. Ownership is *not* transferred to the
		 *  call-back; it is automatically freed after the call-back terminates! */
		void (*sendmsg_cb)(struct osmo_io_fd *iofd, int res, struct msgb *msg);
	};
};

void osmo_iofd_init(void);

struct osmo_io_fd *osmo_iofd_setup(const void *ctx, int fd, const char *name,
		  enum osmo_io_fd_mode mode, const struct osmo_io_ops *ioops, void *data);
int osmo_iofd_set_cmsg_size(struct osmo_io_fd *iofd, size_t cmsg_size);
int osmo_iofd_register(struct osmo_io_fd *iofd, int fd);
int osmo_iofd_unregister(struct osmo_io_fd *iofd);
unsigned int osmo_iofd_txqueue_len(struct osmo_io_fd *iofd);
void osmo_iofd_txqueue_clear(struct osmo_io_fd *iofd);
int osmo_iofd_close(struct osmo_io_fd *iofd);
void osmo_iofd_free(struct osmo_io_fd *iofd);

void osmo_iofd_notify_connected(struct osmo_io_fd *iofd);

int osmo_iofd_write_msgb(struct osmo_io_fd *iofd, struct msgb *msg);
int osmo_iofd_sendto_msgb(struct osmo_io_fd *iofd, struct msgb *msg, int sendto_flags,
			  const struct osmo_sockaddr *dest);
int osmo_iofd_sendmsg_msgb(struct osmo_io_fd *iofd, struct msgb *msg, int sendmsg_flags,
			   const struct msghdr *msgh);

void osmo_iofd_set_alloc_info(struct osmo_io_fd *iofd, unsigned int size, unsigned int headroom);
void osmo_iofd_set_txqueue_max_length(struct osmo_io_fd *iofd, unsigned int size);
void *osmo_iofd_get_data(const struct osmo_io_fd *iofd);
void osmo_iofd_set_data(struct osmo_io_fd *iofd, void *data);

unsigned int osmo_iofd_get_priv_nr(const struct osmo_io_fd *iofd);
void osmo_iofd_set_priv_nr(struct osmo_io_fd *iofd, unsigned int priv_nr);

int osmo_iofd_get_fd(const struct osmo_io_fd *iofd);
const char *osmo_iofd_get_name(const struct osmo_io_fd *iofd);
void osmo_iofd_set_name(struct osmo_io_fd *iofd, const char *name);

int osmo_iofd_set_ioops(struct osmo_io_fd *iofd, const struct osmo_io_ops *ioops);
void osmo_iofd_get_ioops(struct osmo_io_fd *iofd, struct osmo_io_ops *ioops);

/*! @} */
