/* (C) 2008 by Harald Welte <laforge@gnumonks.org>
 * (C) 2010 by Holger Hans Peter Freyther <zecke@selfish.org>
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

/*! \addtogroup msgb
 *  @{
 *
 *  libosmocore message buffers, inspired by Linux kernel skbuff
 *
 *  Inspired by the 'struct skbuff' of the Linux kernel, we implement a
 *  'struct msgb' which we use for handling network
 *  packets aka messages aka PDUs.
 *
 *  A msgb consists of
 *  	* a header with some metadata, such as
 *  	  * a linked list header for message queues or the like
 *  	  * pointers to the headers of various protocol layers inside
 *  	    the packet
 *  	* a data section consisting of
 *  	  * headroom, i.e. space in front of the message, to allow
 *  	    for additional headers being pushed in front of the current
 *  	    data
 *  	  * the currently occupied data for the message
 *  	  * tailroom, i.e. space at the end of the message, to
 *  	    allow more data to be added after the end of the current
 *  	    data
 *
 *  We have plenty of utility functions around the \ref msgb:
 *  	* allocation / release
 *  	* enqueue / dequeue from/to message queues
 *  	* prepending (pushing) and appending (putting) data
 *  	* copying / resizing
 *  	* hex-dumping to a string for debug purposes
 *
 * \file msgb.c
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/logging.h>

/*! Allocate a new message buffer from given talloc context
 * \param[in] ctx talloc context from which to allocate
 * \param[in] size Length in octets, including headroom
 * \param[in] name Human-readable name to be associated with msgb
 * \returns dynamically-allocated \ref msgb
 *
 * This function allocates a 'struct msgb' as well as the underlying
 * memory buffer for the actual message data (size specified by \a size)
 * using the talloc memory context previously set by \ref msgb_set_talloc_ctx
 */
struct msgb *msgb_alloc_c(const void *ctx, uint16_t size, const char *name)
{
	struct msgb *msg;

	msg = talloc_named_const(ctx, sizeof(*msg) + size, name);
	if (!msg) {
		LOGP(DLGLOBAL, LOGL_FATAL, "Unable to allocate a msgb: "
			"name='%s', size=%u\n", name, size);
		return NULL;
	}

	/* Manually zero-initialize allocated memory */
	memset(msg, 0x00, sizeof(*msg) + size);

	msg->data_len = size;
	msg->len = 0;
	msg->data = msg->_data;
	msg->head = msg->_data;
	msg->tail = msg->_data;

	return msg;
}

/* default msgb allocation context for msgb_alloc() */
void *tall_msgb_ctx = NULL;

/*! Allocate a new message buffer from tall_msgb_ctx
 * \param[in] size Length in octets, including headroom
 * \param[in] name Human-readable name to be associated with msgb
 * \returns dynamically-allocated \ref msgb
 *
 * This function allocates a 'struct msgb' as well as the underlying
 * memory buffer for the actual message data (size specified by \a size)
 * using the talloc memory context previously set by \ref msgb_set_talloc_ctx
 */
struct msgb *msgb_alloc(uint16_t size, const char *name)
{
	return msgb_alloc_c(tall_msgb_ctx, size, name);
}


/*! Release given message buffer
 * \param[in] m Message buffer to be freed
 */
void msgb_free(struct msgb *m)
{
	talloc_free(m);
}

/*! Enqueue message buffer to tail of a queue
 * \param[in] queue linked list header of queue
 * \param[in] msg message buffer to be added to the queue
 *
 * The function will append the specified message buffer \a msg to the
 * queue implemented by \ref llist_head \a queue
 */
void msgb_enqueue(struct llist_head *queue, struct msgb *msg)
{
	llist_add_tail(&msg->list, queue);
}

/*! Dequeue message buffer from head of queue
 * \param[in] queue linked list header of queue
 * \returns message buffer (if any) or NULL if queue empty
 *
 * The function will remove the first message buffer from the queue
 * implemented by \ref llist_head \a queue.
 */
struct msgb *msgb_dequeue(struct llist_head *queue)
{
	struct llist_head *lh;

	if (llist_empty(queue))
		return NULL;

	lh = queue->next;

	if (lh) {
		llist_del(lh);
		return llist_entry(lh, struct msgb, list);
	} else
		return NULL;
}

/*! Re-set all message buffer pointers
 *  \param[in] msg message buffer that is to be resetted
 *
 * This will re-set the various internal pointers into the underlying
 * message buffer, i.e. remove all headroom and treat the msgb as
 * completely empty.  It also initializes the control buffer to zero.
 */
void msgb_reset(struct msgb *msg)
{
	msg->len = 0;
	msg->data = msg->_data;
	msg->head = msg->_data;
	msg->tail = msg->_data;

	msg->trx = NULL;
	msg->lchan = NULL;
	msg->l2h = NULL;
	msg->l3h = NULL;
	msg->l4h = NULL;

	memset(&msg->cb, 0, sizeof(msg->cb));
}

/*! get pointer to data section of message buffer
 *  \param[in] msg message buffer
 *  \returns pointer to data section of message buffer
 */
uint8_t *msgb_data(const struct msgb *msg)
{
	return msg->data;
}

/*! Compare and print: check data in msgb against given data and print errors if any
 *  \param[in] file text prefix, usually __FILE__, ignored if print == false
 *  \param[in] line numeric prefix, usually __LINE__, ignored if print == false
 *  \param[in] func text prefix, usually __func__, ignored if print == false
 *  \param[in] level while layer (L1, L2 etc) data should be compared against
 *  \param[in] msg message buffer
 *  \param[in] data expected data
 *  \param[in] len length of data
 *  \param[in] print boolean indicating whether we should print anything to stdout
 *  \returns boolean indicating whether msgb content is equal to a given data
 *
 * This function is not intended to be called directly but rather used through corresponding macro wrappers.
 */
bool _msgb_eq(const char *file, size_t line, const char *func, uint8_t level,
	      const struct msgb *msg, const uint8_t *data, size_t len, bool print)
{
	const char *m_dump;
	unsigned int m_len, i;
	uint8_t *m_data;

	if (!msg) {
		if (print)
			LOGPSRC(DLGLOBAL, LOGL_FATAL, file, line, "%s() NULL msg comparison\n", func);
		return false;
	}

	if (!data) {
		if (print)
			LOGPSRC(DLGLOBAL, LOGL_FATAL, file, line, "%s() NULL comparison data\n", func);
		return false;
	}

	switch (level) {
	case 0:
		m_len = msgb_length(msg);
		m_data = msgb_data(msg);
		m_dump = print ? msgb_hexdump(msg) : NULL;
		break;
	case 1:
		m_len = msgb_l1len(msg);
		m_data = msgb_l1(msg);
		m_dump = print ? msgb_hexdump_l1(msg) : NULL;
		break;
	case 2:
		m_len = msgb_l2len(msg);
		m_data = msgb_l2(msg);
		m_dump = print ? msgb_hexdump_l2(msg) : NULL;
		break;
	case 3:
		m_len = msgb_l3len(msg);
		m_data = msgb_l3(msg);
		m_dump = print ? msgb_hexdump_l3(msg) : NULL;
		break;
	case 4:
		m_len = msgb_l4len(msg);
		m_data = msgb_l4(msg);
		m_dump = print ? msgb_hexdump_l4(msg) : NULL;
		break;
	default:
		LOGPSRC(DLGLOBAL, LOGL_FATAL, file, line,
			"%s() FIXME: unexpected comparison level %u\n", func, level);
		return false;
	}

	if (m_len != len) {
		if (print)
			LOGPSRC(DLGLOBAL, LOGL_FATAL, file, line,
				"%s() Length mismatch: %d != %zu, %s\n", func, m_len, len, m_dump);
		return false;
	}

	if (memcmp(m_data, data, len) == 0)
		return true;

	if (!print)
		return false;

	LOGPSRC(DLGLOBAL, LOGL_FATAL, file, line,
		"%s() L%u data mismatch:\nexpected %s\n         ", func, level, osmo_hexdump(data, len));

	for(i = 0; i < len; i++)
		if (data[i] != m_data[i]) {
			LOGPC(DLGLOBAL, LOGL_FATAL, "!!\n");
			break;
		} else
			LOGPC(DLGLOBAL, LOGL_FATAL, ".. ");

	LOGPC(DLGLOBAL, LOGL_FATAL, "    msgb %s\n", osmo_hexdump(m_data, len));

	return false;
}

/*! get length of message buffer
 *  \param[in] msg message buffer
 *  \returns length of data section in message buffer
 */
uint16_t msgb_length(const struct msgb *msg)
{
	return msg->len;
}

/*! Set the talloc context for \ref msgb_alloc
 * Deprecated, use msgb_talloc_ctx_init() instead.
 *  \param[in] ctx talloc context to be used as root for msgb allocations
 */
void msgb_set_talloc_ctx(void *ctx)
{
	tall_msgb_ctx = ctx;
}

/*! Initialize a msgb talloc context for \ref msgb_alloc.
 * Create a talloc context called "msgb". If \a pool_size is 0, create a named
 * const as msgb talloc context. If \a pool_size is nonzero, create a talloc
 * pool, possibly for faster msgb allocations (see talloc_pool()).
 *  \param[in] root_ctx talloc context used as parent for the new "msgb" ctx.
 *  \param[in] pool_size if nonzero, create a talloc pool of this size.
 *  \returns the new msgb talloc context, e.g. for reporting
 */
void *msgb_talloc_ctx_init(void *root_ctx, unsigned int pool_size)
{
	if (!pool_size)
		tall_msgb_ctx = talloc_size(root_ctx, 0);
	else
		tall_msgb_ctx = talloc_pool(root_ctx, pool_size);
	talloc_set_name_const(tall_msgb_ctx, "msgb");
	return tall_msgb_ctx;
}

/*! Copy an msgb with memory reallocation.
 *
 *  This function allocates a new msgb with new_len size, copies the data buffer of msg,
 *  and adjusts the pointers (incl l1h-l4h) accordingly. The cb part is not copied.
 *  \param[in] ctx  talloc context on which allocation happens
 *  \param[in] msg  The old msgb object
 *  \param[in] new_len The length of new msgb object
 *  \param[in] name Human-readable name to be associated with new msgb
 */
struct msgb *msgb_copy_resize_c(const void *ctx, const struct msgb *msg, uint16_t new_len, const char *name)
{
	struct msgb *new_msg;

	if (new_len < msgb_length(msg)) {
		LOGP(DLGLOBAL, LOGL_ERROR,
			 "Data from old msgb (%u bytes) won't fit into new msgb (%u bytes) after reallocation\n",
			 msgb_length(msg), new_len);
		return NULL;
	}

	new_msg = msgb_alloc_c(ctx, new_len, name);
	if (!new_msg)
		return NULL;

	/* copy header */
	new_msg->len = msg->len;
	new_msg->data += msg->data - msg->_data;
	new_msg->head += msg->head - msg->_data;
	new_msg->tail += msg->tail - msg->_data;

	/* copy data */
	memcpy(new_msg->data, msg->data, msgb_length(msg));

	if (msg->l1h)
		new_msg->l1h = new_msg->_data + (msg->l1h - msg->_data);
	if (msg->l2h)
		new_msg->l2h = new_msg->_data + (msg->l2h - msg->_data);
	if (msg->l3h)
		new_msg->l3h = new_msg->_data + (msg->l3h - msg->_data);
	if (msg->l4h)
		new_msg->l4h = new_msg->_data + (msg->l4h - msg->_data);

	return new_msg;
}

/*! Copy an msgb with memory reallocation.
 *
 *  This function allocates a new msgb with new_len size, copies the data buffer of msg,
 *  and adjusts the pointers (incl l1h-l4h) accordingly. The cb part is not copied.
 *  \param[in] msg  The old msgb object
 *  \param[in] name Human-readable name to be associated with new msgb
 */
struct msgb *msgb_copy_resize(const struct msgb *msg, uint16_t new_len, const char *name)
{
	return msgb_copy_resize_c(tall_msgb_ctx, msg, new_len, name);
}

/*! Copy an msgb.
 *
 *  This function allocates a new msgb, copies the data buffer of msg,
 *  and adjusts the pointers (incl l1h-l4h) accordingly. The cb part
 *  is not copied.
 *  \param[in] ctx  talloc context on which allocation happens
 *  \param[in] msg  The old msgb object
 *  \param[in] name Human-readable name to be associated with msgb
 */
struct msgb *msgb_copy_c(const void *ctx, const struct msgb *msg, const char *name)
{
	return msgb_copy_resize_c(ctx, msg, msg->data_len, name);
}

/*! Copy an msgb.
 *
 *  This function allocates a new msgb, copies the data buffer of msg,
 *  and adjusts the pointers (incl l1h-l4h) accordingly. The cb part
 *  is not copied.
 *  \param[in] msg  The old msgb object
 *  \param[in] name Human-readable name to be associated with msgb
 */
struct msgb *msgb_copy(const struct msgb *msg, const char *name)
{
	return msgb_copy_c(tall_msgb_ctx, msg, name);
}

/*! Resize an area within an msgb
 *
 *  This resizes a sub area of the msgb data and adjusts the pointers (incl
 *  l1h-l4h) accordingly. The cb part is not updated. If the area is extended,
 *  the contents of the extension is undefined. The complete sub area must be a
 *  part of [data,tail].
 *
 *  \param[inout] msg       The msgb object
 *  \param[in]    area      A pointer to the sub-area
 *  \param[in]    old_size  The old size of the sub-area
 *  \param[in]    new_size  The new size of the sub-area
 *  \returns 0 on success, -1 if there is not enough space to extend the area
 */
int msgb_resize_area(struct msgb *msg, uint8_t *area,
			    int old_size, int new_size)
{
	int rc;
	uint8_t *post_start = area + old_size;
	int pre_len = area - msg->data;
	int post_len = msg->len - old_size - pre_len;
	int delta_size = new_size - old_size;

	if (old_size < 0 || new_size < 0)
		MSGB_ABORT(msg, "Negative sizes are not allowed\n");
	if (area < msg->data || post_start > msg->tail)
		MSGB_ABORT(msg, "Sub area is not fully contained in the msg data\n");

	if (delta_size == 0)
		return 0;

	if (delta_size > 0) {
		rc = msgb_trim(msg, msg->len + delta_size);
		if (rc < 0)
			return rc;
	}

	memmove(area + new_size, area + old_size, post_len);

	if (msg->l1h >= post_start)
		msg->l1h += delta_size;
	if (msg->l2h >= post_start)
		msg->l2h += delta_size;
	if (msg->l3h >= post_start)
		msg->l3h += delta_size;
	if (msg->l4h >= post_start)
		msg->l4h += delta_size;

	if (delta_size < 0)
		msgb_trim(msg, msg->len + delta_size);

	return 0;
}


/*! fill user-provided buffer with hexdump of the msg.
 * \param[out] buf caller-allocated buffer for output string
 * \param[in] buf_len length of buf
 * \param[in] msg message buffer to be dumped
 * \returns buf
 */
char *msgb_hexdump_buf(char *buf, size_t buf_len, const struct msgb *msg)
{
	unsigned int buf_offs = 0;
	int nchars;
	const unsigned char *start = msg->data;
	const unsigned char *lxhs[4];
	unsigned int i;

	lxhs[0] = msg->l1h;
	lxhs[1] = msg->l2h;
	lxhs[2] = msg->l3h;
	lxhs[3] = msg->l4h;

	for (i = 0; i < ARRAY_SIZE(lxhs); i++) {
		if (!lxhs[i])
			continue;

		if (lxhs[i] < msg->head)
			continue;
		if (lxhs[i] > msg->head + msg->data_len)
			continue;
		if (lxhs[i] > msg->tail)
			continue;
		if (lxhs[i] < msg->data || lxhs[i] > msg->tail) {
			nchars = snprintf(buf + buf_offs, buf_len - buf_offs,
					  "(L%d=data%+" PRIdPTR ") ",
					  i+1, lxhs[i] - msg->data);
			buf_offs += nchars;
			continue;
		}
		if (lxhs[i] < start) {
			nchars = snprintf(buf + buf_offs, buf_len - buf_offs,
					  "(L%d%+" PRIdPTR ") ", i+1,
					  start - lxhs[i]);
			buf_offs += nchars;
			continue;
		}
		nchars = snprintf(buf + buf_offs, buf_len - buf_offs,
				  "%s[L%d]> ",
				  osmo_hexdump(start, lxhs[i] - start),
				  i+1);
		if (nchars < 0 || nchars + buf_offs >= buf_len)
			return "ERROR";

		buf_offs += nchars;
		start = lxhs[i];
	}
	nchars = snprintf(buf + buf_offs, buf_len - buf_offs,
			  "%s", osmo_hexdump(start, msg->tail - start));
	if (nchars < 0 || nchars + buf_offs >= buf_len)
		return "ERROR";

	buf_offs += nchars;

	for (i = 0; i < ARRAY_SIZE(lxhs); i++) {
		if (!lxhs[i])
			continue;

		if (lxhs[i] < msg->head || lxhs[i] > msg->head + msg->data_len) {
			nchars = snprintf(buf + buf_offs, buf_len - buf_offs,
					  "(L%d out of range) ", i+1);
		} else if (lxhs[i] <= msg->data + msg->data_len &&
			   lxhs[i] > msg->tail) {
			nchars = snprintf(buf + buf_offs, buf_len - buf_offs,
					  "(L%d=tail%+" PRIdPTR ") ",
					  i+1, lxhs[i] - msg->tail);
		} else
			continue;

		if (nchars < 0 || nchars + buf_offs >= buf_len)
			return "ERROR";
		buf_offs += nchars;
	}

	return buf;
}

/*! Return a (static) buffer containing a hexdump of the msg.
 * \param[in] msg message buffer
 * \returns a pointer to a static char array
 */
const char *msgb_hexdump(const struct msgb *msg)
{
	static __thread char buf[4100];
	return msgb_hexdump_buf(buf, sizeof(buf), msg);
}

/*! Return a dynamically allocated buffer containing a hexdump of the msg
 * \param[in] ctx talloc context from where to allocate the output string
 * \param[in] msg message buffer
 * \returns a pointer to a static char array
 */
char *msgb_hexdump_c(const void *ctx, const struct msgb *msg)
{
	size_t buf_len = msgb_length(msg) * 3 + 100;
	char *buf = talloc_size(ctx, buf_len);
	if (!buf)
		return NULL;
	return msgb_hexdump_buf(buf, buf_len, msg);
}

/*! Print a string to the end of message buffer.
 * \param[in] msgb message buffer.
 * \param[in] format format string.
 * \returns 0 on success, -EINVAL on error.
 *
 * The resulting string is printed to the msgb without a trailing nul
 * character. A nul following the data tail may be written as an implementation
 * detail, but a trailing nul is never part of the msgb data in terms of
 * msgb_length().
 *
 * Note: the tailroom must always be one byte longer than the string to be
 * written. The msgb is filled only up to tailroom=1. This is an implementation
 * detail that allows leaving a nul character behind the valid data.
 *
 * In case of error, the msgb remains unchanged, though data may have been
 * written to the (unused) memory after the tail pointer.
 */
int msgb_printf(struct msgb *msgb, const char *format, ...)
{
	va_list args;
	int str_len;
	int rc = 0;

	OSMO_ASSERT(msgb);
	OSMO_ASSERT(format);

	/* Regardless of what we plan to add to the buffer, we must at least
	 * be able to store a string terminator (nullstring) */
	if (msgb_tailroom(msgb) < 1)
		return -EINVAL;

	va_start(args, format);

	str_len =
	    vsnprintf((char *)msgb->tail, msgb_tailroom(msgb), format, args);

	if (str_len >= msgb_tailroom(msgb) || str_len < 0) {
		rc = -EINVAL;
	} else
		msgb_put(msgb, str_len);

	va_end(args);
	return rc;
}

/*! @} */
