/*! \file osmo_io_uring.c
 * io_uring backend for osmo_io.
 *
 * (C) 2022-2023 by sysmocom s.f.m.c.
 * Author: Daniel Willmann <daniel@sysmocom.de>
 * (C) 2023-2024 by Harald Welte <laforge@osmocom.org>
 *
 * All Rights Reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

/* TODO:
 * Parameters:
 * - number of simultaneous read/write in uring for given fd
 *
 */

#include "osmo_config.h"
#if defined(__linux__)

#include <stdio.h>
#include <talloc.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>

#include <netinet/in.h>
#include <netinet/sctp.h>
#include <sys/eventfd.h>
#include <liburing.h>

#include <osmocom/core/osmo_io.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/msgb.h>
#include <osmocom/core/select.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/socket.h>

#include "osmo_io_internal.h"

#define IOFD_URING_ENTRIES 4096

struct osmo_io_uring {
	struct osmo_fd event_ofd;
	struct io_uring ring;
};

static __thread struct osmo_io_uring g_ring;

static void iofd_uring_cqe(struct io_uring *ring);

/*! read call-back for eventfd notifying us if entries are in the completion queue */
static int iofd_uring_poll_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct io_uring *ring = ofd->data;
	eventfd_t val;
	int rc;

	if (what & OSMO_FD_READ) {
		rc = eventfd_read(ofd->fd, &val);
		if (rc < 0) {
			LOGP(DLIO, LOGL_ERROR, "eventfd_read() returned error\n");
			return rc;
		}

		iofd_uring_cqe(ring);
	}
	if (what & OSMO_FD_WRITE)
		OSMO_ASSERT(0);

	return 0;
}

/*! initialize the uring and tie it into our event loop */
void osmo_iofd_uring_init(void)
{
	int rc, evfd;

	rc = io_uring_queue_init(IOFD_URING_ENTRIES, &g_ring.ring, 0);
	if (rc < 0)
		osmo_panic("failure during io_uring_queue_init(): %s\n", strerror(-rc));

	rc = eventfd(0, 0);
	if (rc < 0) {
		io_uring_queue_exit(&g_ring.ring);
		osmo_panic("failure creating eventfd(0, 0) for io_uring: %s\n", strerror(-rc));
	}
	evfd = rc;

	osmo_fd_setup(&g_ring.event_ofd, evfd, OSMO_FD_READ, iofd_uring_poll_cb, &g_ring.ring, 0);
	rc = osmo_fd_register(&g_ring.event_ofd);
	if (rc < 0) {
		close(evfd);
		io_uring_queue_exit(&g_ring.ring);
		osmo_panic("failure registering io_uring-eventfd as osmo_fd: %d\n", rc);
	}
	rc = io_uring_register_eventfd(&g_ring.ring, evfd);
	if (rc < 0) {
		osmo_fd_unregister(&g_ring.event_ofd);
		close(evfd);
		io_uring_queue_exit(&g_ring.ring);
		osmo_panic("failure registering eventfd with io_uring: %s\n", strerror(-rc));
	}
}


static void iofd_uring_submit_recv(struct osmo_io_fd *iofd, enum iofd_msg_action action)
{
	struct msgb *msg;
	struct iofd_msghdr *msghdr;
	struct io_uring_sqe *sqe;

	msg = iofd_msgb_pending_or_alloc(iofd);
	if (!msg) {
		LOGPIO(iofd, LOGL_ERROR, "Could not allocate msgb for reading\n");
		OSMO_ASSERT(0);
	}

	msghdr = iofd_msghdr_alloc(iofd, action, msg, iofd->cmsg_size);
	if (!msghdr) {
		LOGPIO(iofd, LOGL_ERROR, "Could not allocate msghdr for reading\n");
		OSMO_ASSERT(0);
	}

	msghdr->iov[0].iov_base = msg->tail;
	msghdr->iov[0].iov_len = msgb_tailroom(msg);

	switch (action) {
	case IOFD_ACT_READ:
		break;
	case IOFD_ACT_RECVMSG:
		msghdr->hdr.msg_control = msghdr->cmsg;
		msghdr->hdr.msg_controllen = iofd->cmsg_size;
		/* fall-through */
	case IOFD_ACT_RECVFROM:
		msghdr->hdr.msg_iov = &msghdr->iov[0];
		msghdr->hdr.msg_iovlen = 1;
		msghdr->hdr.msg_name = &msghdr->osa.u.sa;
		msghdr->hdr.msg_namelen = osmo_sockaddr_size(&msghdr->osa);
		break;
	default:
		OSMO_ASSERT(0);
	}

	sqe = io_uring_get_sqe(&g_ring.ring);
	if (!sqe) {
		LOGPIO(iofd, LOGL_ERROR, "Could not get io_uring_sqe\n");
		OSMO_ASSERT(0);
	}

	switch (action) {
	case IOFD_ACT_READ:
		io_uring_prep_readv(sqe, iofd->fd, msghdr->iov, 1, 0);
		break;
	case IOFD_ACT_RECVMSG:
	case IOFD_ACT_RECVFROM:
		io_uring_prep_recvmsg(sqe, iofd->fd, &msghdr->hdr, msghdr->flags);
		break;
	default:
		OSMO_ASSERT(0);
	}
	io_uring_sqe_set_data(sqe, msghdr);

	io_uring_submit(&g_ring.ring);
	/* NOTE: This only works if we have one read per fd */
	iofd->u.uring.read_msghdr = msghdr;
}

/*! completion call-back for READ/RECVFROM */
static void iofd_uring_handle_recv(struct iofd_msghdr *msghdr, int rc)
{
	struct osmo_io_fd *iofd = msghdr->iofd;
	struct msgb *msg = msghdr->msg;

	if (rc > 0)
		msgb_put(msg, rc);

	if (!IOFD_FLAG_ISSET(iofd, IOFD_FLAG_CLOSED))
		iofd_handle_recv(iofd, msg, rc, msghdr);

	if (iofd->u.uring.read_enabled && !IOFD_FLAG_ISSET(iofd, IOFD_FLAG_CLOSED))
		iofd_uring_submit_recv(iofd, msghdr->action);
	else
		iofd->u.uring.read_msghdr = NULL;


	iofd_msghdr_free(msghdr);
}

static int iofd_uring_submit_tx(struct osmo_io_fd *iofd);

/*! completion call-back for WRITE/SENDTO */
static void iofd_uring_handle_tx(struct iofd_msghdr *msghdr, int rc)
{
	struct osmo_io_fd *iofd = msghdr->iofd;

	/* Detach msghdr from iofd. It might get freed here or it is freed during iofd_handle_send_completion().
	 * If there is pending data to send, iofd_uring_submit_tx() will attach it again.
	 * iofd_handle_send_completion() will invoke a callback function to signal the possibility of write/send.
	 * This callback function might close iofd, leading to the potential freeing of iofd->u.uring.write_msghdr if
	 * still attached. Since iofd_handle_send_completion() frees msghdr at the end of the function, detaching
	 * msghdr here prevents a double-free bug. */
	if (iofd->u.uring.write_msghdr == msghdr)
		iofd->u.uring.write_msghdr = NULL;

	if (OSMO_UNLIKELY(IOFD_FLAG_ISSET(iofd, IOFD_FLAG_CLOSED))) {
		msgb_free(msghdr->msg);
		iofd_msghdr_free(msghdr);
	} else {
		iofd_handle_send_completion(iofd, rc, msghdr);
	}

	/* submit the next to-be-transmitted message for this file descriptor */
	if (iofd->u.uring.write_enabled && !IOFD_FLAG_ISSET(iofd, IOFD_FLAG_CLOSED))
		iofd_uring_submit_tx(iofd);
}

/*! handle completion of a single I/O message */
static void iofd_uring_handle_completion(struct iofd_msghdr *msghdr, int res)
{
	struct osmo_io_fd *iofd = msghdr->iofd;

	IOFD_FLAG_SET(iofd, IOFD_FLAG_IN_CALLBACK);

	switch (msghdr->action) {
	case IOFD_ACT_READ:
	case IOFD_ACT_RECVFROM:
	case IOFD_ACT_RECVMSG:
		iofd_uring_handle_recv(msghdr, res);
		break;
	case IOFD_ACT_WRITE:
	case IOFD_ACT_SENDTO:
	case IOFD_ACT_SENDMSG:
		iofd_uring_handle_tx(msghdr, res);
		break;
	default:
		OSMO_ASSERT(0)
	}

	IOFD_FLAG_UNSET(iofd, IOFD_FLAG_IN_CALLBACK);

	if (IOFD_FLAG_ISSET(iofd, IOFD_FLAG_TO_FREE) && !iofd->u.uring.read_msghdr && !iofd->u.uring.write_msghdr)
		talloc_free(iofd);
}

/*! process all pending completion queue entries in given io_uring */
static void iofd_uring_cqe(struct io_uring *ring)
{
	int rc;
	struct io_uring_cqe *cqe;
	struct iofd_msghdr *msghdr;

	while (io_uring_peek_cqe(ring, &cqe) == 0) {

		msghdr = io_uring_cqe_get_data(cqe);
		if (!msghdr) {
			LOGP(DLIO, LOGL_DEBUG, "Cancellation returned\n");
			io_uring_cqe_seen(ring, cqe);
			continue;
		}
		if (!msghdr->iofd) {
			io_uring_cqe_seen(ring, cqe);
			iofd_msghdr_free(msghdr);
			continue;
		}

		rc = cqe->res;
		/* Hand the entry back to the kernel before */
		io_uring_cqe_seen(ring, cqe);

		iofd_uring_handle_completion(msghdr, rc);

	}
}

/*! will submit the next to-be-transmitted message for given iofd */
static int iofd_uring_submit_tx(struct osmo_io_fd *iofd)
{
	struct io_uring_sqe *sqe;
	struct iofd_msghdr *msghdr;

	msghdr = iofd_txqueue_dequeue(iofd);
	if (!msghdr)
		return -ENODATA;

	sqe = io_uring_get_sqe(&g_ring.ring);
	if (!sqe) {
		LOGPIO(iofd, LOGL_ERROR, "Could not get io_uring_sqe\n");
		OSMO_ASSERT(0);
	}

	io_uring_sqe_set_data(sqe, msghdr);

	switch (msghdr->action) {
	case IOFD_ACT_WRITE:
	case IOFD_ACT_SENDTO:
	case IOFD_ACT_SENDMSG:
		io_uring_prep_sendmsg(sqe, msghdr->iofd->fd, &msghdr->hdr, msghdr->flags);
		break;
	default:
		OSMO_ASSERT(0);
	}

	io_uring_submit(&g_ring.ring);
	iofd->u.uring.write_msghdr = msghdr;

	return 0;
}

static void iofd_uring_write_enable(struct osmo_io_fd *iofd);
static void iofd_uring_read_enable(struct osmo_io_fd *iofd);

static int iofd_uring_register(struct osmo_io_fd *iofd)
{
	return 0;
}

static int iofd_uring_unregister(struct osmo_io_fd *iofd)
{
	struct io_uring_sqe *sqe;
	struct iofd_msghdr *msghdr;

	if (iofd->u.uring.read_msghdr) {
		msghdr = iofd->u.uring.read_msghdr;
		sqe = io_uring_get_sqe(&g_ring.ring);
		OSMO_ASSERT(sqe != NULL);
		io_uring_sqe_set_data(sqe, NULL);
		LOGPIO(iofd, LOGL_DEBUG, "Cancelling read\n");
		iofd->u.uring.read_msghdr = NULL;
		talloc_steal(OTC_GLOBAL, msghdr);
		msghdr->iofd = NULL;
		io_uring_prep_cancel(sqe, msghdr, 0);
	}

	if (iofd->u.uring.write_msghdr) {
		msghdr = iofd->u.uring.write_msghdr;
		sqe = io_uring_get_sqe(&g_ring.ring);
		OSMO_ASSERT(sqe != NULL);
		io_uring_sqe_set_data(sqe, NULL);
		LOGPIO(iofd, LOGL_DEBUG, "Cancelling write\n");
		iofd->u.uring.write_msghdr = NULL;
		talloc_steal(OTC_GLOBAL, msghdr);
		msgb_free(msghdr->msg);
		msghdr->iofd = NULL;
		io_uring_prep_cancel(sqe, msghdr, 0);
	}
	io_uring_submit(&g_ring.ring);

	if (IOFD_FLAG_ISSET(iofd, IOFD_FLAG_NOTIFY_CONNECTED)) {
		osmo_fd_unregister(&iofd->u.uring.connect_ofd);
		IOFD_FLAG_UNSET(iofd, IOFD_FLAG_NOTIFY_CONNECTED);
	}

	return 0;
}

static void iofd_uring_write_enable(struct osmo_io_fd *iofd)
{
	iofd->u.uring.write_enabled = true;

	if (iofd->u.uring.write_msghdr)
		return;

	/* This function is called again, once the socket is connected. */
	if (IOFD_FLAG_ISSET(iofd, IOFD_FLAG_NOTIFY_CONNECTED))
		return;

	if (osmo_iofd_txqueue_len(iofd) > 0)
		iofd_uring_submit_tx(iofd);
	else if (iofd->mode == OSMO_IO_FD_MODE_READ_WRITE) {
		/* Empty write request to check when the socket is connected */
		struct iofd_msghdr *msghdr;
		struct io_uring_sqe *sqe;
		struct msgb *msg = msgb_alloc_headroom(0, 0, "io_uring write dummy");
		if (!msg) {
			LOGPIO(iofd, LOGL_ERROR, "Could not allocate msgb for writing\n");
			OSMO_ASSERT(0);
		}
		msghdr = iofd_msghdr_alloc(iofd, IOFD_ACT_WRITE, msg, 0);
		if (!msghdr) {
			LOGPIO(iofd, LOGL_ERROR, "Could not allocate msghdr for writing\n");
			OSMO_ASSERT(0);
		}

		msghdr->iov[0].iov_base = msgb_data(msg);
		msghdr->iov[0].iov_len = msgb_length(msg);

		sqe = io_uring_get_sqe(&g_ring.ring);
		if (!sqe) {
			LOGPIO(iofd, LOGL_ERROR, "Could not get io_uring_sqe\n");
			OSMO_ASSERT(0);
		}
		io_uring_prep_writev(sqe, iofd->fd, msghdr->iov, 1, 0);
		io_uring_sqe_set_data(sqe, msghdr);

		io_uring_submit(&g_ring.ring);
		iofd->u.uring.write_msghdr = msghdr;
	}
}

static void iofd_uring_write_disable(struct osmo_io_fd *iofd)
{
	iofd->u.uring.write_enabled = false;
}

static void iofd_uring_read_enable(struct osmo_io_fd *iofd)
{
	iofd->u.uring.read_enabled = true;

	if (iofd->u.uring.read_msghdr)
		return;

	/* This function is called again, once the socket is connected. */
	if (IOFD_FLAG_ISSET(iofd, IOFD_FLAG_NOTIFY_CONNECTED))
		return;

	switch (iofd->mode) {
	case OSMO_IO_FD_MODE_READ_WRITE:
		iofd_uring_submit_recv(iofd, IOFD_ACT_READ);
		break;
	case OSMO_IO_FD_MODE_RECVFROM_SENDTO:
		iofd_uring_submit_recv(iofd, IOFD_ACT_RECVFROM);
		break;
	case OSMO_IO_FD_MODE_RECVMSG_SENDMSG:
		iofd_uring_submit_recv(iofd, IOFD_ACT_RECVMSG);
		break;
	default:
		OSMO_ASSERT(0);
	}
}

static void iofd_uring_read_disable(struct osmo_io_fd *iofd)
{
	iofd->u.uring.read_enabled = false;
}

static int iofd_uring_close(struct osmo_io_fd *iofd)
{
	iofd_uring_read_disable(iofd);
	iofd_uring_write_disable(iofd);
	iofd_uring_unregister(iofd);
	return close(iofd->fd);
}

/* called via osmocom poll/select main handling once outbound non-blocking connect() completes */
static int iofd_uring_connected_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct osmo_io_fd *iofd = ofd->data;

	LOGPIO(iofd, LOGL_DEBUG, "Socket connected or failed.\n");

	if (!(what & OSMO_FD_WRITE))
		return 0;

	/* Unregister from poll/select handling. */
	osmo_fd_unregister(ofd);
	IOFD_FLAG_UNSET(iofd, IOFD_FLAG_NOTIFY_CONNECTED);

	/* Notify the application about this via a zero-length write completion call-back. */
	IOFD_FLAG_SET(iofd, IOFD_FLAG_IN_CALLBACK);
	switch (iofd->mode) {
	case OSMO_IO_FD_MODE_READ_WRITE:
		iofd->io_ops.write_cb(iofd, 0, NULL);
		break;
	case OSMO_IO_FD_MODE_RECVFROM_SENDTO:
		iofd->io_ops.sendto_cb(iofd, 0, NULL, NULL);
		break;
	case OSMO_IO_FD_MODE_RECVMSG_SENDMSG:
		iofd->io_ops.sendmsg_cb(iofd, 0, NULL);
		break;
	}
	IOFD_FLAG_UNSET(iofd, IOFD_FLAG_IN_CALLBACK);

	/* If write/read notifications are pending, enable it now. */
	if (iofd->u.uring.write_enabled && !IOFD_FLAG_ISSET(iofd, IOFD_FLAG_CLOSED))
		iofd_uring_write_enable(iofd);
	if (iofd->u.uring.read_enabled && !IOFD_FLAG_ISSET(iofd, IOFD_FLAG_CLOSED))
		iofd_uring_read_enable(iofd);

	if (IOFD_FLAG_ISSET(iofd, IOFD_FLAG_TO_FREE) && !iofd->u.uring.read_msghdr && !iofd->u.uring.write_msghdr)
		talloc_free(iofd);
	return 0;
}

static void iofd_uring_notify_connected(struct osmo_io_fd *iofd)
{
	if (iofd->mode == OSMO_IO_FD_MODE_RECVMSG_SENDMSG) {
		/* Don't call this function after enabling read or write. */
		OSMO_ASSERT(!iofd->u.uring.write_enabled && !iofd->u.uring.read_enabled);

		/* Use a temporary osmo_fd which we can use to notify us once the connection is established
		 * or failed (indicated by FD becoming writable).
		 * This is needed as (at least for SCTP sockets) one cannot submit a zero-length writev/sendmsg
		 * in order to get notification when the socekt is writable.*/
		if (!IOFD_FLAG_ISSET(iofd, IOFD_FLAG_NOTIFY_CONNECTED)) {
			osmo_fd_setup(&iofd->u.uring.connect_ofd, iofd->fd, OSMO_FD_WRITE,
				      iofd_uring_connected_cb, iofd, 0);
			if (osmo_fd_register(&iofd->u.uring.connect_ofd) < 0)
				LOGPIO(iofd, LOGL_ERROR, "Failed to register FD for connect event.\n");
			else
				IOFD_FLAG_SET(iofd, IOFD_FLAG_NOTIFY_CONNECTED);
		}
	} else
		iofd_uring_write_enable(iofd);
}

const struct iofd_backend_ops iofd_uring_ops = {
	.register_fd = iofd_uring_register,
	.unregister_fd = iofd_uring_unregister,
	.close = iofd_uring_close,
	.write_enable = iofd_uring_write_enable,
	.write_disable = iofd_uring_write_disable,
	.read_enable = iofd_uring_read_enable,
	.read_disable = iofd_uring_read_disable,
	.notify_connected = iofd_uring_notify_connected,
};

#endif /* defined(__linux__) */
