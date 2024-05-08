/*
 * (C) 2010-2016 by Holger Hans Peter Freyther
 * (C) 2010 by On-Waves
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

#include <errno.h>
#include <osmocom/core/write_queue.h>
#include <osmocom/core/logging.h>

/*! \addtogroup write_queue
 *  @{
 *  Write queue for writing \ref msgb to sockets/fds.
 *
 * \file write_queue.c */

/*! Select loop function for write queue handling
 *  \param[in] fd osmocom file descriptor
 *  \param[in] what bit-mask of events that have happened
 *  \returns 0 on success; negative on error
 *
 * This function is provided so that it can be registered with the
 * select loop abstraction code (\ref osmo_fd::cb).
 */
int osmo_wqueue_bfd_cb(struct osmo_fd *fd, unsigned int what)
{
	struct osmo_wqueue *queue;
	int rc;

	queue = container_of(fd, struct osmo_wqueue, bfd);

	if (what & OSMO_FD_READ) {
		rc = queue->read_cb(fd);
		if (rc == -EBADF)
			goto err_badfd;
	}

	if (what & OSMO_FD_EXCEPT) {
		rc = queue->except_cb(fd);
		if (rc == -EBADF)
			goto err_badfd;
	}

	if (what & OSMO_FD_WRITE) {
		struct msgb *msg;

		fd->when &= ~OSMO_FD_WRITE;

		msg = msgb_dequeue_count(&queue->msg_queue, &queue->current_length);
		/* the queue might have been emptied */
		if (msg) {
			rc = queue->write_cb(fd, msg);
			if (rc == -EBADF) {
				msgb_free(msg);
				goto err_badfd;
			} else if (rc == -EAGAIN) {
				/* re-enqueue the msgb to the head of the queue */
				llist_add(&msg->list, &queue->msg_queue);
				queue->current_length++;
			} else
				msgb_free(msg);

			if (!llist_empty(&queue->msg_queue))
				fd->when |= OSMO_FD_WRITE;
		}
	}

err_badfd:
	/* Return value is not checked in osmo_select_main() */
	return 0;
}

/*! Initialize a \ref osmo_wqueue structure
 *  \param[in] queue Write queue to operate on
 *  \param[in] max_length Maximum length of write queue
 */
void osmo_wqueue_init(struct osmo_wqueue *queue, int max_length)
{
	queue->max_length = max_length;
	queue->current_length = 0;
	queue->read_cb = NULL;
	queue->write_cb = NULL;
	queue->except_cb = NULL;
	queue->bfd.cb = osmo_wqueue_bfd_cb;
	INIT_LLIST_HEAD(&queue->msg_queue);
}

/*! Enqueue a new \ref msgb into a write queue (without logging full queue events)
 *  \param[in] queue Write queue to be used
 *  \param[in] data to-be-enqueued message buffer
 *  \returns 0 on success; negative on error (MESSAGE NOT FREED IN CASE OF ERROR).
 */
int osmo_wqueue_enqueue_quiet(struct osmo_wqueue *queue, struct msgb *data)
{
	if (queue->current_length >= queue->max_length)
		return -ENOSPC;

	msgb_enqueue_count(&queue->msg_queue, data, &queue->current_length);
	queue->bfd.when |= OSMO_FD_WRITE;

	return 0;
}

/*! Enqueue a new \ref msgb into a write queue
 *  \param[in] queue Write queue to be used
 *  \param[in] data to-be-enqueued message buffer
 *  \returns 0 on success; negative on error (MESSAGE NOT FREED IN CASE OF ERROR).
 */
int osmo_wqueue_enqueue(struct osmo_wqueue *queue, struct msgb *data)
{
	if (queue->current_length >= queue->max_length) {
		LOGP(DLGLOBAL, LOGL_ERROR,
			"wqueue(%p) is full. Rejecting msgb\n", queue);
		return -ENOSPC;
	}

	return osmo_wqueue_enqueue_quiet(queue, data);
}

/*! Clear a \ref osmo_wqueue
 *  \param[in] queue Write queue to be cleared
 *
 * This function will clear (remove/release) all messages in it.
 */
void osmo_wqueue_clear(struct osmo_wqueue *queue)
{
	while (!llist_empty(&queue->msg_queue)) {
		struct msgb *msg = msgb_dequeue(&queue->msg_queue);
		msgb_free(msg);
	}

	queue->current_length = 0;
	queue->bfd.when &= ~OSMO_FD_WRITE;
}

/*! Update write queue length & drop excess messages.
 * \param[in] queue linked list header of message queue
 * \param[in] len new max. wqueue length
 * \returns Number of messages dropped.
 *
 * Messages beyond the new maximum message queue size will be dropped.
 */
size_t osmo_wqueue_set_maxlen(struct osmo_wqueue *queue, unsigned int len)
{
	size_t dropped_msgs = 0;
	struct msgb *msg;
	queue->max_length = len;
	while (queue->current_length > queue->max_length) {
		msg = msgb_dequeue_count(&queue->msg_queue, &queue->current_length);
		msgb_free(msg);
		dropped_msgs++;
	}
	return dropped_msgs;
}

/*! @} */
