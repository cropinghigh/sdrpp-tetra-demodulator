#pragma once

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/select.h>
#include <pthread.h>

/*! \defgroup osmo_it_q Inter-Thread Queue
 *  @{
 *  \file osmo_it_q.h */

/*! One instance of an inter-thread queue.  The user can use this to queue messages
 *  between different threads.  The enqueue operation is non-blocking (but of course
 *  grabs a mutex for the actual list operations to safeguard against races).  The
 *  receiving thread is woken up by an event_fd which can be registered in the libosmocore
 *  select loop handling. */
struct osmo_it_q {
	/* entry in global list of message queues */
	struct llist_head entry;

	/* the actual list of user structs. HEAD: first in queue; TAIL: last in queue */
	struct llist_head list;
	/* A pthread mutex to safeguard accesses to the queue. No rwlock as we always write. */
	pthread_mutex_t mutex;
	/* Current count of messages in the queue */
	unsigned int current_length;
	/* osmo-fd wrapped eventfd */
	struct osmo_fd event_ofd;

	/* a user-defined name for this queue */
	const char *name;
	/* maximum permitted length of queue */
	unsigned int max_length;
	/* read call-back, called for each de-queued message */
	void (*read_cb)(struct osmo_it_q *q, struct llist_head *item);
	/* opaque data pointer passed through to call-back function */
	void *data;
};

struct osmo_it_q *osmo_it_q_by_name(const char *name);

int _osmo_it_q_enqueue(struct osmo_it_q *queue, struct llist_head *item);
#define osmo_it_q_enqueue(queue, item, member) \
	_osmo_it_q_enqueue(queue, &(item)->member)

struct llist_head *_osmo_it_q_dequeue(struct osmo_it_q *queue);
#define osmo_it_q_dequeue(queue, item, member) do {			\
	struct llist_head *l = _osmo_it_q_dequeue(queue);		\
	if (!l)								\
		*item = NULL;						\
	else								\
		*item = llist_entry(l, typeof(**item), member);		\
} while (0)


struct osmo_it_q *osmo_it_q_alloc(void *ctx, const char *name, unsigned int max_length,

					void (*read_cb)(struct osmo_it_q *q, struct llist_head *item),
					void *data);
void osmo_it_q_destroy(struct osmo_it_q *q);
void osmo_it_q_flush(struct osmo_it_q *q);

/*! @} */
