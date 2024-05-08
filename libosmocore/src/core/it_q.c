/*! \file it_q.c
 * Osmocom Inter-Thread queue implementation */
/* (C) 2019 by Harald Welte <laforge@gnumonks.org>
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

/*! \addtogroup it_q
 *  @{
 *  Inter-Thread Message Queue.
 *
 * This implements a general-purpose queue between threads. It uses
 * user-provided data types (containing a llist_head as initial member)
 * as elements in the queue and an eventfd-based notification mechanism.
 * Hence, it can be used for pretty much anything, including but not
 * limited to msgbs, including msgb-wrapped osmo_prim.
 *
 * The idea is that the sending thread simply calls osmo_it_q_enqueue().
 * The receiving thread is woken up from its osmo_select_main() loop by eventfd,
 * and a general osmo_fd callback function for the eventfd will dequeue each item
 * and call a queue-specific callback function.
 */

#include "osmo_config.h"

#ifdef HAVE_SYS_EVENTFD_H

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/eventfd.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/it_q.h>

/* "increment" the eventfd by specified 'inc' */
static int eventfd_increment(int fd, uint64_t inc)
{
	int rc;

	rc = write(fd, &inc, sizeof(inc));
	if (rc != sizeof(inc))
		return -1;

	return 0;
}

/* global (for all threads) list of message queues in a program + associated lock */
static LLIST_HEAD(it_queues);
static pthread_rwlock_t it_queues_rwlock = PTHREAD_RWLOCK_INITIALIZER;

/* resolve it-queue by its [globally unique] name; must be called with rwlock held */
static struct osmo_it_q *_osmo_it_q_by_name(const char *name)
{
	struct osmo_it_q *q;
	llist_for_each_entry(q, &it_queues, entry) {
		if (!strcmp(q->name, name))
			return q;
	}
	return NULL;
}

/*! resolve it-queue by its [globally unique] name */
struct osmo_it_q *osmo_it_q_by_name(const char *name)
{
	struct osmo_it_q *q;
	pthread_rwlock_rdlock(&it_queues_rwlock);
	q = _osmo_it_q_by_name(name);
	pthread_rwlock_unlock(&it_queues_rwlock);
	return q;
}

/* osmo_fd call-back when eventfd is readable */
static int osmo_it_q_fd_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct osmo_it_q *q = (struct osmo_it_q *) ofd->data;
	uint64_t val;
	int i, rc;

	if (!(what & OSMO_FD_READ))
		return 0;

	rc = read(ofd->fd, &val, sizeof(val));
	if (rc < sizeof(val))
		return rc;

	for (i = 0; i < val; i++) {
		struct llist_head *item = _osmo_it_q_dequeue(q);
		/* in case the user might have called osmo_it_q_flush() we may
		 * end up in the eventfd-dispatch but without any messages left in the queue,
		 * otherwise I'd have loved to OSMO_ASSERT(msg) here. */
		if (!item)
			break;
		q->read_cb(q, item);
	}

	return 0;
}

/*! Allocate a new inter-thread message queue.
 *  \param[in] ctx talloc context from which to allocate the queue
 *  \param[in] name human-readable string name of the queue; function creates a copy.
 *  \param[in] read_cb call-back function to be called for each de-queued message; may be
 *  			NULL in case you don't want eventfd/osmo_select integration and
 *  			will manually take care of noticing if and when to dequeue.
 *  \returns a newly-allocated inter-thread message queue; NULL in case of error */
struct osmo_it_q *osmo_it_q_alloc(void *ctx, const char *name, unsigned int max_length,
					void (*read_cb)(struct osmo_it_q *q, struct llist_head *item),
					void *data)
{
	struct osmo_it_q *q;
	int fd;

	q = talloc_zero(ctx, struct osmo_it_q);
	if (!q)
		return NULL;
	q->data = data;
	q->name = talloc_strdup(q, name);
	q->current_length = 0;
	q->max_length = max_length;
	q->read_cb = read_cb;
	INIT_LLIST_HEAD(&q->list);
	pthread_mutex_init(&q->mutex, NULL);
	q->event_ofd.fd = -1;

	if (q->read_cb) {
		/* create eventfd *if* the user has provided a read_cb function */
		fd = eventfd(0, 0);
		if (fd < 0) {
			talloc_free(q);
			return NULL;
		}

		/* initialize BUT NOT REGISTER the osmo_fd. The receiving thread must
		 * take are to select/poll/read/... on it */
		osmo_fd_setup(&q->event_ofd, fd, OSMO_FD_READ, osmo_it_q_fd_cb, q, 0);
	}

	/* add to global list of queues, checking for duplicate names */
	pthread_rwlock_wrlock(&it_queues_rwlock);
	if (_osmo_it_q_by_name(q->name)) {
		pthread_rwlock_unlock(&it_queues_rwlock);
		if (q->event_ofd.fd >= 0)
			osmo_fd_close(&q->event_ofd);
		talloc_free(q);
		return NULL;
	}
	llist_add_tail(&q->entry, &it_queues);
	pthread_rwlock_unlock(&it_queues_rwlock);

	return q;
}

static void *item_dequeue(struct llist_head *queue)
{
	struct llist_head *lh;

	if (llist_empty(queue))
		return NULL;

	lh = queue->next;
	if (lh) {
		llist_del(lh);
		return lh;
	} else
		return NULL;
}

/*! Flush all messages currently present in queue */
static void _osmo_it_q_flush(struct osmo_it_q *q)
{
	void *item;
	while ((item = item_dequeue(&q->list))) {
		talloc_free(item);
	}
	q->current_length = 0;
}

/*! Flush all messages currently present in queue */
void osmo_it_q_flush(struct osmo_it_q *q)
{
	OSMO_ASSERT(q);

	pthread_mutex_lock(&q->mutex);
	_osmo_it_q_flush(q);
	pthread_mutex_unlock(&q->mutex);
}

/*! Destroy a message queue */
void osmo_it_q_destroy(struct osmo_it_q *q)
{
	OSMO_ASSERT(q);

	/* first remove from global list of queues */
	pthread_rwlock_wrlock(&it_queues_rwlock);
	llist_del(&q->entry);
	pthread_rwlock_unlock(&it_queues_rwlock);
	/* next, close the eventfd */
	if (q->event_ofd.fd >= 0)
		osmo_fd_close(&q->event_ofd);
	/* flush all messages still present */
	osmo_it_q_flush(q);
	pthread_mutex_destroy(&q->mutex);
	/* and finally release memory */
	talloc_free(q);
}

/*! Thread-safe en-queue to an inter-thread message queue.
 *  \param[in] queue Inter-thread queue on which to enqueue
 *  \param[in] item Item to enqueue. Must have llist_head as first member!
 *  \returns 0 on success; negative on error */
int _osmo_it_q_enqueue(struct osmo_it_q *queue, struct llist_head *item)
{
	OSMO_ASSERT(queue);
	OSMO_ASSERT(item);

	pthread_mutex_lock(&queue->mutex);
	if (queue->current_length+1 > queue->max_length) {
		pthread_mutex_unlock(&queue->mutex);
		return -ENOSPC;
	}
	llist_add_tail(item, &queue->list);
	queue->current_length++;
	pthread_mutex_unlock(&queue->mutex);
	/* increment eventfd counter by one */
	if (queue->event_ofd.fd >= 0)
		eventfd_increment(queue->event_ofd.fd, 1);
	return 0;
}


/*! Thread-safe de-queue from an inter-thread message queue.
 *  \param[in] queue Inter-thread queue from which to dequeue
 *  \returns llist_head of dequeued message; NULL if none available
 */
struct llist_head *_osmo_it_q_dequeue(struct osmo_it_q *queue)
{
	struct llist_head *l;
	OSMO_ASSERT(queue);

	pthread_mutex_lock(&queue->mutex);

	l = item_dequeue(&queue->list);
	if (l != NULL)
		queue->current_length--;

	pthread_mutex_unlock(&queue->mutex);

	return l;
}


#endif /* HAVE_SYS_EVENTFD_H */

/*! @} */
