#pragma once

/* (C) 2008 by Harald Welte <laforge@gnumonks.org>
 * All Rights Reserved
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

#include <stdint.h>
#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/defs.h>

/*! \defgroup msgb Message buffers
 *  @{
 * \file msgb.h */

#define MSGB_DEBUG

/*! Osmocom message buffer */
struct msgb {
	struct llist_head list; /*!< linked list header */


	/* Part of which TRX logical channel we were received / transmitted */
	/* FIXME: move them into the control buffer */
	union {
		void *dst; /*!< reference of origin/destination */
		struct gsm_bts_trx *trx;
	};
	struct gsm_lchan *lchan; /*!< logical channel */

	unsigned char *l1h; /*!< pointer to Layer1 header (if any) */
	unsigned char *l2h; /*!< pointer to A-bis layer 2 header: OML, RSL(RLL), NS */
	unsigned char *l3h; /*!< pointer to Layer 3 header. For OML: FOM; RSL: 04.08; GPRS: BSSGP */
	unsigned char *l4h; /*!< pointer to layer 4 header */

	unsigned long cb[5]; /*!< control buffer */

	uint16_t data_len;   /*!< length of underlying data array */
	uint16_t len;	     /*!< length of bytes used in msgb */

	unsigned char *head;	/*!< start of underlying memory buffer */
	unsigned char *tail;	/*!< end of message in buffer */
	unsigned char *data;	/*!< start of message in buffer */
	unsigned char _data[0]; /*!< optional immediate data array */
};

extern struct msgb *msgb_alloc_c(const void *ctx, uint16_t size, const char *name);
extern struct msgb *msgb_alloc(uint16_t size, const char *name);
extern void msgb_free(struct msgb *m);
extern void msgb_enqueue(struct llist_head *queue, struct msgb *msg);
extern struct msgb *msgb_dequeue(struct llist_head *queue);
extern void msgb_reset(struct msgb *m);
uint16_t msgb_length(const struct msgb *msg);
extern const char *msgb_hexdump(const struct msgb *msg);
char *msgb_hexdump_buf(char *buf, size_t buf_len, const struct msgb *msg);
char *msgb_hexdump_c(const void *ctx, const struct msgb *msg);
extern int msgb_resize_area(struct msgb *msg, uint8_t *area,
	int old_size, int new_size);
extern struct msgb *msgb_copy(const struct msgb *msg, const char *name);
extern struct msgb *msgb_copy_c(const void *ctx, const struct msgb *msg, const char *name);
extern struct msgb *msgb_copy_resize(const struct msgb *msg, uint16_t new_len, const char *name);
extern struct msgb *msgb_copy_resize_c(const void *ctx, const struct msgb *msg, uint16_t new_len, const char *name);
static int msgb_test_invariant(const struct msgb *msg) __attribute__((pure));

/*! Free all msgbs from a queue built with msgb_enqueue().
 * \param[in] queue  list head of a msgb queue.
 */
static inline void msgb_queue_free(struct llist_head *queue)
{
	struct msgb *msg;
	while ((msg = msgb_dequeue(queue))) msgb_free(msg);
}

/*! Enqueue message buffer to tail of a queue and increment queue size counter
 * \param[in] queue linked list header of queue
 * \param[in] msg message buffer to be added to the queue
 * \param[in] count pointer to variable holding size of the queue
 *
 * The function will append the specified message buffer \a msg to the queue
 * implemented by \ref llist_head \a queue using function \ref msgb_enqueue_count,
 * then increment \a count
 */
static inline void msgb_enqueue_count(struct llist_head *queue, struct msgb *msg,
					unsigned int *count)
{
	msgb_enqueue(queue, msg);
	(*count)++;
}

/*! Dequeue message buffer from head of queue and decrement queue size counter
 * \param[in] queue linked list header of queue
 * \param[in] count pointer to variable holding size of the queue
 * \returns message buffer (if any) or NULL if queue empty
 *
 * The function will remove the first message buffer from the queue
 * implemented by \ref llist_head \a queue using function \ref msgb_enqueue_count,
 * and decrement \a count, all if queue is not empty.
 */
static inline struct msgb *msgb_dequeue_count(struct llist_head *queue,
						unsigned int *count)
{
	struct msgb *msg = msgb_dequeue(queue);
	if (msg)
		(*count)--;
	return msg;
}

#ifdef MSGB_DEBUG
#include <osmocom/core/panic.h>
#define MSGB_ABORT(msg, fmt, args ...) do {		\
	osmo_panic("msgb(%p): " fmt, msg, ## args);	\
	} while(0)
#else
#define MSGB_ABORT(msg, fmt, args ...)
#endif

/*! obtain L1 header of msgb */
#define msgb_l1(m)	((void *)((m)->l1h))
/*! obtain L2 header of msgb */
#define msgb_l2(m)	((void *)((m)->l2h))
/*! obtain L3 header of msgb */
#define msgb_l3(m)	((void *)((m)->l3h))
/*! obtain L4 header of msgb */
#define msgb_l4(m)	((void *)((m)->l4h))
/*! obtain SMS header of msgb */
#define msgb_sms(m)	msgb_l4(m)

/*! determine length of L1 message
 *  \param[in] msgb message buffer
 *  \returns size of L1 message in bytes
 *
 * This function computes the number of bytes between the tail of the
 * message and the layer 1 header.
 */
static inline unsigned int msgb_l1len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l1h);
	return msgb->tail - (uint8_t *)msgb_l1(msgb);
}

/*! determine length of L2 message
 *  \param[in] msgb message buffer
 *  \returns size of L2 message in bytes
 *
 * This function computes the number of bytes between the tail of the
 * message and the layer 2 header.
 */
static inline unsigned int msgb_l2len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l2h);
	return msgb->tail - (uint8_t *)msgb_l2(msgb);
}

/*! determine length of L3 message
 *  \param[in] msgb message buffer
 *  \returns size of L3 message in bytes
 *
 * This function computes the number of bytes between the tail of the
 * message and the layer 3 header.
 */
static inline unsigned int msgb_l3len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l3h);
	return msgb->tail - (uint8_t *)msgb_l3(msgb);
}

/*! determine length of L4 message
 *  \param[in] msgb message buffer
 *  \returns size of L4 message in bytes
 *
 * This function computes the number of bytes between the tail of the
 * message and the layer 4 header.
 */
static inline unsigned int msgb_l4len(const struct msgb *msgb)
{
	OSMO_ASSERT(msgb->l4h);
	return msgb->tail - (uint8_t *)msgb_l4(msgb);
}

/*! determine the length of the header
 *  \param[in] msgb message buffer
 *  \returns number of bytes between start of buffer and start of msg
 *
 * This function computes the length difference between the underlying
 * data buffer and the used section of the \a msgb.
 */
static inline unsigned int msgb_headlen(const struct msgb *msgb)
{
	return msgb->len - msgb->data_len;
}

/*! determine how much tail room is left in msgb
 *  \param[in] msgb message buffer
 *  \returns number of bytes remaining at end of msgb
 *
 * This function computes the amount of octets left in the underlying
 * data buffer after the end of the message.
 */
static inline int msgb_tailroom(const struct msgb *msgb)
{
	return (msgb->head + msgb->data_len) - msgb->tail;
}

/*! determine the amount of headroom in msgb
 *  \param[in] msgb message buffer
 *  \returns number of bytes left ahead of message start in msgb
 *
 * This function computes the amount of bytes left in the underlying
 * data buffer before the start of the actual message.
 */
static inline int msgb_headroom(const struct msgb *msgb)
{
	return (msgb->data - msgb->head);
}

/*! append data to end of message buffer
 *  \param[in] msgb message buffer
 *  \param[in] len number of bytes to append to message
 *  \returns pointer to start of newly-appended data
 *
 * This function will move the \a tail pointer of the message buffer \a
 * len bytes further, thus enlarging the message by \a len bytes.
 *
 * The return value is a pointer to start of the newly added section at
 * the end of the message and can be used for actually filling/copying
 * data into it.
 */
static inline unsigned char *msgb_put(struct msgb *msgb, unsigned int len)
{
	unsigned char *tmp = msgb->tail;
	if (OSMO_UNLIKELY(msgb_tailroom(msgb) < (int) len))
		MSGB_ABORT(msgb, "Not enough tailroom msgb_put"
			   " (allocated %u, head at %u, len %u, tailroom %u < want tailroom %u)\n",
			   msgb->data_len - sizeof(struct msgb),
			   msgb->head - msgb->_data,
			   msgb->len,
			   msgb_tailroom(msgb), len);
	msgb->tail += len;
	msgb->len += len;
	return tmp;
}

/*! append a uint8 value to the end of the message
 *  \param[in] msgb message buffer
 *  \param[in] word unsigned 8bit byte to be appended
 */
static inline void msgb_put_u8(struct msgb *msgb, uint8_t word)
{
	uint8_t *space = msgb_put(msgb, 1);
	space[0] = word & 0xFF;
}

/*! append a uint16 value to the end of the message
 *  \param[in] msgb message buffer
 *  \param[in] word unsigned 16bit byte to be appended
 */
static inline void msgb_put_u16(struct msgb *msgb, uint16_t word)
{
	uint8_t *space = msgb_put(msgb, 2);
	osmo_store16be(word, space);
}

/*! append a uint32 value to the end of the message
 *  \param[in] msgb message buffer
 *  \param[in] word unsigned 32bit byte to be appended
 */
static inline void msgb_put_u32(struct msgb *msgb, uint32_t word)
{
	uint8_t *space = msgb_put(msgb, 4);
	osmo_store32be(word, space);
}

/*! remove data from end of message
 *  \param[in] msgb message buffer
 *  \param[in] len number of bytes to remove from end
 */
static inline unsigned char *msgb_get(struct msgb *msgb, unsigned int len)
{
	if (OSMO_UNLIKELY(msgb_length(msgb) < len))
		MSGB_ABORT(msgb, "msgb too small to get %u (len %u)\n",
			   len, msgb_length(msgb));
	msgb->tail -= len;
	msgb->len -= len;
	return msgb->tail;
}

/*! remove uint8 from end of message
 *  \param[in] msgb message buffer
 *  \returns 8bit value taken from end of msgb
 */
static inline uint8_t msgb_get_u8(struct msgb *msgb)
{
	uint8_t *space = msgb_get(msgb, 1);
	return space[0];
}

/*! remove uint16 from end of message
 *  \param[in] msgb message buffer
 *  \returns 16bit value taken from end of msgb
 */
static inline uint16_t msgb_get_u16(struct msgb *msgb)
{
	uint8_t *space = msgb_get(msgb, 2);
	return osmo_load16be(space);
}

/*! remove uint32 from end of message
 *  \param[in] msgb message buffer
 *  \returns 32bit value taken from end of msgb
 */
static inline uint32_t msgb_get_u32(struct msgb *msgb)
{
	uint8_t *space = msgb_get(msgb, 4);
	return osmo_load32be(space);
}

/*! prepend (push) some data to start of message
 *  \param[in] msgb message buffer
 *  \param[in] len number of bytes to pre-pend
 *  \returns pointer to newly added portion at start of \a msgb
 *
 * This function moves the \a data pointer of the \ref msgb further
 * to the front (by \a len bytes), thereby enlarging the message by \a
 * len bytes.
 *
 * The return value is a pointer to the newly added section in the
 * beginning of the message.  It can be used to fill/copy data into it.
 */
static inline unsigned char *msgb_push(struct msgb *msgb, unsigned int len)
{
	if (OSMO_UNLIKELY(msgb_headroom(msgb) < (int) len))
		MSGB_ABORT(msgb, "Not enough headroom msgb_push"
			   " (allocated %u, head at %u < want headroom %u, len %u, tailroom %u)\n",
			   msgb->data_len - sizeof(struct msgb),
			   msgb->head - msgb->_data,
			   len,
			   msgb->len,
			   msgb_tailroom(msgb));
	msgb->data -= len;
	msgb->len += len;
	return msgb->data;
}

/*! prepend a uint8 value to the head of the message
 *  \param[in] msg message buffer
 *  \param[in] word unsigned 8bit byte to be prepended
 */
static inline void msgb_push_u8(struct msgb *msg, uint8_t word)
{
	uint8_t *space = msgb_push(msg, 1);
	space[0] = word;
}

/*! prepend a uint16 value to the head of the message
 *  \param[in] msg message buffer
 *  \param[in] word unsigned 16bit byte to be prepended
 */
static inline void msgb_push_u16(struct msgb *msg, uint16_t word)
{
	uint16_t *space = (uint16_t *) msgb_push(msg, 2);
	osmo_store16be(word, space);
}

/*! prepend a uint32 value to the head of the message
 *  \param[in] msg message buffer
 *  \param[in] word unsigned 32bit byte to be prepended
 */
static inline void msgb_push_u32(struct msgb *msg, uint32_t word)
{
	uint32_t *space = (uint32_t *) msgb_push(msg, 4);
	osmo_store32be(word, space);
}

static inline unsigned char *msgb_push_tl(struct msgb *msgb, uint8_t tag)
{
	uint8_t *data = msgb_push(msgb, 2);

	data[0] = tag;
	data[1] = msgb->len - 2;
	return data;
}

/*! remove (pull) a header from the front of the message buffer
 *  \param[in] msgb message buffer
 *  \param[in] len number of octets to be pulled
 *  \returns pointer to new start of msgb
 *
 * This function moves the \a data pointer of the \ref msgb further back
 * in the message, thereby shrinking the size of the message by \a len
 * bytes.
 */
static inline unsigned char *msgb_pull(struct msgb *msgb, unsigned int len)
{
	if (OSMO_UNLIKELY(msgb_length(msgb) < len))
		MSGB_ABORT(msgb, "msgb too small to pull %u (len %u)\n",
			   len, msgb_length(msgb));
	msgb->len -= len;
	return msgb->data += len;
}

/*! remove (pull) all headers in front of l3h from the message buffer.
 *  \param[in] msg message buffer with a valid l3h
 *  \returns pointer to new start of msgb (l3h)
 *
 * This function moves the \a data pointer of the \ref msgb further back
 * in the message, thereby shrinking the size of the message.
 * l1h and l2h will be cleared.
 */
static inline unsigned char *msgb_pull_to_l3(struct msgb *msg)
{
	unsigned char *ret = msgb_pull(msg, msg->l3h - msg->data);
	msg->l1h = msg->l2h = NULL;
	return ret;
}

/*! remove (pull) all headers in front of l2h from the message buffer.
 *  \param[in] msg message buffer with a valid l2h
 *  \returns pointer to new start of msgb (l2h)
 *
 * This function moves the \a data pointer of the \ref msgb further back
 * in the message, thereby shrinking the size of the message.
 * l1h will be cleared.
 */
static inline unsigned char *msgb_pull_to_l2(struct msgb *msg)
{
	unsigned char *ret = msgb_pull(msg, msg->l2h - msg->data);
	msg->l1h = NULL;
	return ret;
}

/*! remove uint8 from front of message
 *  \param[in] msgb message buffer
 *  \returns 8bit value taken from the front of msgb
 */
static inline uint8_t msgb_pull_u8(struct msgb *msgb)
{
	uint8_t *space = msgb_pull(msgb, 1) - 1;
	return space[0];
}

/*! remove uint16 from front of message
 *  \param[in] msgb message buffer
 *  \returns 16bit value taken from the front of msgb
 */
static inline uint16_t msgb_pull_u16(struct msgb *msgb)
{
	uint8_t *space = msgb_pull(msgb, 2) - 2;
	return osmo_load16be(space);
}

/*! remove uint32 from front of message
 *  \param[in] msgb message buffer
 *  \returns 32bit value taken from the front of msgb
 */
static inline uint32_t msgb_pull_u32(struct msgb *msgb)
{
	uint8_t *space = msgb_pull(msgb, 4) - 4;
	return osmo_load32be(space);
}

/*! Increase headroom of empty msgb, reducing the tailroom
 *  \param[in] msg message buffer
 *  \param[in] len amount of extra octets to be reserved as headroom
 *
 * This function reserves some memory at the beginning of the underlying
 * data buffer.  The idea is to reserve space in case further headers
 * have to be pushed to the \ref msgb during further processing.
 *
 * Calling this function leads to undefined reusults if it is called on
 * a non-empty \ref msgb.
 */
static inline void msgb_reserve(struct msgb *msg, int len)
{
	msg->data += len;
	msg->tail += len;
}

/*! Trim the msgb to a given absolute length
 *  \param[in] msg message buffer
 *  \param[in] len new total length of buffer
 *  \returns 0 in case of success, negative in case of error
 */
static inline int msgb_trim(struct msgb *msg, int len)
{
	if (OSMO_UNLIKELY(len < 0))
		MSGB_ABORT(msg, "Negative length is not allowed\n");
	if (OSMO_UNLIKELY(len > msg->data_len))
		return -1;

	msg->len = len;
	msg->tail = msg->data + len;

	return 0;
}

/*! Trim the msgb to a given layer3 length
 *  \param[in] msg message buffer
 *  \param[in] l3len new layer3 length
 *  \returns 0 in case of success, negative in case of error
 */
static inline int msgb_l3trim(struct msgb *msg, int l3len)
{
	return msgb_trim(msg, (msg->l3h - msg->data) + l3len);
}

/*! Allocate message buffer with specified headroom from specified talloc context.
 *  \param[in] ctx talloc context from which to allocate
 *  \param[in] size size in bytes, including headroom
 *  \param[in] headroom headroom in bytes
 *  \param[in] name human-readable name
 *  \returns allocated message buffer with specified headroom
 *
 * This function is a convenience wrapper around \ref msgb_alloc
 * followed by \ref msgb_reserve in order to create a new \ref msgb with
 * user-specified amount of headroom.
 */
static inline struct msgb *msgb_alloc_headroom_c(const void *ctx, uint16_t size, uint16_t headroom,
						 const char *name)
{
	OSMO_ASSERT(size >= headroom);

	struct msgb *msg = msgb_alloc_c(ctx, size, name);
	if (OSMO_LIKELY(msg))
		msgb_reserve(msg, headroom);
	return msg;
}


/*! Allocate message buffer with specified headroom
 *  \param[in] size size in bytes, including headroom
 *  \param[in] headroom headroom in bytes
 *  \param[in] name human-readable name
 *  \returns allocated message buffer with specified headroom
 *
 * This function is a convenience wrapper around \ref msgb_alloc
 * followed by \ref msgb_reserve in order to create a new \ref msgb with
 * user-specified amount of headroom.
 */
static inline struct msgb *msgb_alloc_headroom(uint16_t size, uint16_t headroom,
						const char *name)
{
	OSMO_ASSERT(size >= headroom);

	struct msgb *msg = msgb_alloc(size, name);
	if (OSMO_LIKELY(msg))
		msgb_reserve(msg, headroom);
	return msg;
}

/*! Check a message buffer for consistency
 *  \param[in] msg message buffer
 *  \returns 0 (false) if inconsistent, != 0 (true) otherwise
 */
static inline int msgb_test_invariant(const struct msgb *msg)
{
	const unsigned char *lbound;
	if (!msg || !msg->data || !msg->tail ||
	    (msg->data + msg->len != msg->tail) ||
	    (msg->data < msg->head) ||
	    (msg->tail > msg->head + msg->data_len))
		return 0;

	lbound = msg->head;

	if (msg->l1h) {
		if (msg->l1h < lbound)
			return 0;
		lbound = msg->l1h;
	}
	if (msg->l2h) {
		if (msg->l2h < lbound)
			return 0;
		lbound = msg->l2h;
	}
	if (msg->l3h) {
		if (msg->l3h < lbound)
			return 0;
		lbound = msg->l3h;
	}
	if (msg->l4h) {
		if (msg->l4h < lbound)
			return 0;
		lbound = msg->l4h;
	}

	return lbound <= msg->head +  msg->data_len;
}


/* msgb data comparison helpers */

/*! Compare: check data in msgb against given data
 *  \param[in] msg message buffer
 *  \param[in] data expected data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb content is equal to the given data
 */
#define msgb_eq_data(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 0, msg, data, len, false)

/*! Compare: check L1 data in msgb against given data
 *  \param[in] msg message buffer
 *  \param[in] data expected L1 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L1 content is equal to the given data
 */
#define msgb_eq_l1_data(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 1, msg, data, len, false)

/*! Compare: check L2 data in msgb against given data
 *  \param[in] msg message buffer
 *  \param[in] data expected L2 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L2 content is equal to the given data
 */
#define msgb_eq_l2_data(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 2, msg, data, len, false)

/*! Compare: check L3 data in msgb against given data
 *  \param[in] msg message buffer
 *  \param[in] data expected L3 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L3 content is equal to the given data
 */
#define msgb_eq_l3_data(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 3, msg, data, len, false)

/*! Compare: check L4 data in msgb against given data
 *  \param[in] msg message buffer
 *  \param[in] data expected L4 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L4 content is equal to the given data
 */
#define msgb_eq_l4_data(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 4, msg, data, len, false)


/* msgb test/debug helpers */

/*! Compare and print: check data in msgb against given data and print errors if any
 *  \param[in] msg message buffer
 *  \param[in] data expected data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb content is equal to the given data
 */
#define msgb_eq_data_print(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 0, msg, data, len, true)

/*! Compare and print: check L1 data in msgb against given data and print errors if any
 *  \param[in] msg message buffer
 *  \param[in] data expected L1 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L1 content is equal to the given data
 */
#define msgb_eq_l1_data_print(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 1, msg, data, len, true)

/*! Compare and print: check L2 data in msgb against given data and print errors if any
 *  \param[in] msg message buffer
 *  \param[in] data expected L2 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L2 content is equal to the given data
 */
#define msgb_eq_l2_data_print(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 2, msg, data, len, true)

/*! Compare and print: check L3 data in msgb against given data and print errors if any
 *  \param[in] msg message buffer
 *  \param[in] data expected L3 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L3 content is equal to the given data
 */
#define msgb_eq_l3_data_print(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 3, msg, data, len, true)


/*! Compare and print: check L4 data in msgb against given data and print errors if any
 *  \param[in] msg message buffer
 *  \param[in] data expected L4 data
 *  \param[in] len length of data
 *  \returns boolean indicating whether msgb L4 content is equal to the given data
 */
#define msgb_eq_l4_data_print(msg, data, len)				\
	_msgb_eq(__FILE__, __LINE__, __func__, 4, msg, data, len, true)

bool _msgb_eq(const char *file, size_t line, const char *func, uint8_t level,
	      const struct msgb *msg, const uint8_t *data, size_t len, bool print);


/* msgb data comparison */

/*! Compare msgbs
 *  \param[in] msg1 message buffer
 *  \param[in] msg2 reference message buffer
 *  \returns boolean indicating whether msgb content is equal
 */
#define msgb_eq(msg1, msg2) msgb_eq_data(msg1, msgb_data(msg2), msgb_length(msg2))

/*! Compare msgbs L1 content
 *  \param[in] msg1 message buffer
 *  \param[in] msg2 reference message buffer
 *  \returns boolean indicating whether msgb L1 content is equal
 */
#define msgb_eq_l1(msg1, msg2) msgb_eq_l1_data(msg1, msgb_l1(msg2), msgb_l1len(msg2))

/*! Compare msgbs L2 content
 *  \param[in] msg1 message buffer
 *  \param[in] msg2 reference message buffer
 *  \returns boolean indicating whether msgb L2 content is equal
 */
#define msgb_eq_l2(msg1, msg2) msgb_eq_l2_data(msg1, msgb_l2(msg2), msgb_l2len(msg2))

/*! Compare msgbs L3 content
 *  \param[in] msg1 message buffer
 *  \param[in] msg2 reference message buffer
 *  \returns boolean indicating whether msgb L3 content is equal
 */
#define msgb_eq_l3(msg1, msg2) msgb_eq_l3_data(msg1, msgb_l3(msg2), msgb_l3len(msg2))

/*! Compare msgbs L4 content
 *  \param[in] msg1 message buffer
 *  \param[in] msg2 reference message buffer
 *  \returns boolean indicating whether msgb L4 content is equal
 */
#define msgb_eq_l4(msg1, msg2) msgb_eq_l4_data(msg1, msgb_l4(msg2), msgb_l4len(msg2))


/* non inline functions to ease binding */

uint8_t *msgb_data(const struct msgb *msg);

void *msgb_talloc_ctx_init(void *root_ctx, unsigned int pool_size);
void msgb_set_talloc_ctx(void *ctx) OSMO_DEPRECATED("Use msgb_talloc_ctx_init() instead");
int msgb_printf(struct msgb *msgb, const char *format, ...);

static inline const char *msgb_hexdump_l1(const struct msgb *msg)
{
	if (!msgb_l1(msg) || !(msgb_l1len(msg)))
		return "[]";
	return osmo_hexdump((const unsigned char *) msgb_l1(msg), msgb_l1len(msg));
}

static inline const char *msgb_hexdump_l2(const struct msgb *msg)
{
	if (!msgb_l2(msg) || !(msgb_l2len(msg)))
		return "[]";
	return osmo_hexdump((const unsigned char *) msgb_l2(msg), msgb_l2len(msg));
}

static inline const char *msgb_hexdump_l3(const struct msgb *msg)
{
	if (!msgb_l3(msg) || !(msgb_l3len(msg)))
		return "[]";
	return osmo_hexdump((const unsigned char*) msgb_l3(msg), msgb_l3len(msg));
}

static inline const char *msgb_hexdump_l4(const struct msgb *msg)
{
	if (!msgb_l4(msg) || !(msgb_l4len(msg)))
		return "[]";
	return osmo_hexdump((const unsigned char*) msgb_l4(msg), msgb_l4len(msg));
}

/*! @} */
