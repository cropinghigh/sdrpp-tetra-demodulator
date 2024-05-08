/*! \file use_count.c
 * Generic object usage counter Implementation (get, put and deallocate on zero count).
 */
/*
 * (C) 2019 by sysmocom s.f.m.c. GmbH <info@sysmocom.de>
 *
 * All Rights Reserved
 *
 * Author: Neels Hofmeyr <neels@hofmeyr.de>
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
 */

#include <errno.h>
#include <inttypes.h>
#include <string.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/use_count.h>

/*! \addtogroup use_count
 *
 * Generic object usage counter (get, put and deallocate on zero count).
 *
 * For an example and a detailed description, see struct osmo_use_count.
 *
 * @{
 * \file use_count.c
 */

/*! Add two int32_t but make sure to min- and max-clamp at INT32_MIN and INT32_MAX, respectively. */
static inline bool count_safe(int32_t *val_p, int32_t add)
{
	int32_t val = *val_p;

	/* A simpler implementation would just let the integer overflow and compare with previous value afterwards, but
	 * that causes runtime errors in the address sanitizer. So let's just do this without tricks. */
	if (add < 0 && val < 0 && val - INT32_MIN < -add) {
		*val_p = INT32_MIN;
		return false;
	}

	if (add > 0 && val > 0 && INT32_MAX - val < add) {
		*val_p = INT32_MAX;
		return false;
	}

	*val_p = val + add;
	return true;
}

/*! Return the sum of all use counts, min- and max-clamped at INT32_MIN and INT32_MAX.
 * \param[in] uc  Use counts to sum up.
 * \return Accumulated counts, or 0 if uc is NULL.
 */
int32_t osmo_use_count_total(const struct osmo_use_count *uc)
{
	struct osmo_use_count_entry *e;
	int32_t total = 0;

	if (!uc || !uc->use_counts.next)
		return 0;

	llist_for_each_entry(e, &uc->use_counts, entry) {
		count_safe(&total, e->count);
	}
	return total;
}

/*! Return use count by a single use token.
 * \param[in] uc  Use counts to look up in.
 * \param[in] use  Use token.
 * \return Use count, or 0 if uc is NULL or use token is not present.
 */
int32_t osmo_use_count_by(const struct osmo_use_count *uc, const char *use)
{
	const struct osmo_use_count_entry *e;
	if (!uc)
		return 0;
	e = osmo_use_count_find(uc, use);
	if (!e)
		return 0;
	return e->count;
}

/*! Write a comprehensive listing of use counts to a string buffer.
 * Reads like "12 (3*barring,fighting,8*kungfoo)".
 * \param[inout] buf  Destination buffer.
 * \param[in] buf_len  sizeof(buf).
 * \param[in] uc  Use counts to print.
 * \return buf, always nul-terminated (except when buf_len < 1).
 */
const char *osmo_use_count_name_buf(char *buf, size_t buf_len, const struct osmo_use_count *uc)
{
	osmo_use_count_to_str_buf(buf, buf_len, uc);
	return buf;
}

/*! Write a comprehensive listing of use counts to a string buffer.
 * Reads like "12 (3*barring,fighting,8*kungfoo)".
 * \param[inout] buf  Destination buffer.
 * \param[in] buf_len  sizeof(buf).
 * \param[in] uc  Use counts to print.
 * \return number of bytes that would be written, like snprintf().
 */
int osmo_use_count_to_str_buf(char *buf, size_t buf_len, const struct osmo_use_count *uc)
{
	int32_t count = osmo_use_count_total(uc);
	struct osmo_strbuf sb = { .buf = buf, .len = buf_len };
	struct osmo_use_count_entry *e;
	bool first;

	OSMO_STRBUF_PRINTF(sb, "%" PRId32 " (", count);

	if (!uc->use_counts.next)
		goto uninitialized;

	first = true;
	llist_for_each_entry(e, &uc->use_counts, entry) {
		if (!e->count)
			continue;
		if (!first)
			OSMO_STRBUF_PRINTF(sb, ",");
		first = false;
		if (e->count != 1)
			OSMO_STRBUF_PRINTF(sb, "%" PRId32 "*", e->count);
		OSMO_STRBUF_PRINTF(sb, "%s", e->use ? : "NULL");
	}
	if (first)
		OSMO_STRBUF_PRINTF(sb, "-");

uninitialized:
	OSMO_STRBUF_PRINTF(sb, ")");
	return sb.chars_needed;
}

/*! Write a comprehensive listing of use counts to a talloc allocated string buffer.
 * Reads like "12 (3*barring,fighting,8*kungfoo)".
 * \param[in] ctx  talloc pool to allocate from.
 * \param[in] uc  Use counts to print.
 * \return buf, always nul-terminated.
 */
char *osmo_use_count_to_str_c(void *ctx, const struct osmo_use_count *uc)
{
	OSMO_NAME_C_IMPL(ctx, 32, "ERROR", osmo_use_count_to_str_buf, uc)
}

/* Return a use token's use count entry -- probably you want osmo_use_count_by() instead.
 * \param[in] uc  Use counts to look up in.
 * \param[in] use  Use token.
 * \return matching entry, or NULL if not present.
 */
struct osmo_use_count_entry *osmo_use_count_find(const struct osmo_use_count *uc, const char *use)
{
	struct osmo_use_count_entry *e;
	if (!uc->use_counts.next)
		return NULL;
	llist_for_each_entry(e, &uc->use_counts, entry) {
		if (e->use == use || (use && e->use && !strcmp(e->use, use)))
			return e;
	}
	return NULL;
}

/*! Find a use count entry that currently has zero count, and re-use that for this new use token. */
static struct osmo_use_count_entry *osmo_use_count_repurpose_zero_entry(struct osmo_use_count *uc, const char *use)
{
	struct osmo_use_count_entry *e;
	if (!uc->use_counts.next)
		return NULL;
	llist_for_each_entry(e, &uc->use_counts, entry) {
		if (!e->count) {
			e->use = use;
			return e;
		}
	}
	return NULL;
}

/*! Allocate a new use count entry, happens implicitly in osmo_use_count_get_put(). */
static struct osmo_use_count_entry *osmo_use_count_create(struct osmo_use_count *uc, const char *use)
{
	struct osmo_use_count_entry *e = talloc_zero(uc->talloc_object, struct osmo_use_count_entry);
	if (!e)
		return NULL;
	*e = (struct osmo_use_count_entry){
		.use_count = uc,
		.use = use,
	};
	if (!uc->use_counts.next)
		INIT_LLIST_HEAD(&uc->use_counts);
	llist_add_tail(&e->entry, &uc->use_counts);
	return e;
}

/*! Deallocate a use count entry.
 * Normally, this is not necessary -- it is ok and even desirable to leave use count entries around even when they reach
 * a count of zero, until the use_count->talloc_object deallocates and removes all of them in one flush. This avoids
 * repeated allocation and deallocation for use tokens, because use count entries that have reached zero count are
 * repurposed for any other use tokens. A cleanup makes sense only if a very large number of differing use tokens surged
 * at the same time, and the owning object will not be deallocated soon; if so, this should be done by the
 * osmo_use_count_cb_t implementation.
 *
 * osmo_use_count_free() must *not* be called on use count entries that were added by
 * osmo_use_count_make_static_entries(). This is the responsibility of the osmo_use_count_cb_t() implementation.
 *
 * \param[in] use_count_entry  Use count entry to unlist and free.
 */
void osmo_use_count_free(struct osmo_use_count_entry *use_count_entry)
{
	if (!use_count_entry)
		return;
	llist_del(&use_count_entry->entry);
	talloc_free(use_count_entry);
}

/*! Implementation for osmo_use_count_get_put(), which can also be directly invoked to pass source file information. For
 * arguments besides file and line, see osmo_use_count_get_put().
 * \param[in] file  Source file path, as in __FILE__.
 * \param[in] line  Source file line, as in __LINE__.
 */
int _osmo_use_count_get_put(struct osmo_use_count *uc, const char *use, int32_t change,
			    const char *file, int line)
{
	struct osmo_use_count_entry *e;
	int32_t old_use_count;
	if (!uc)
		return -EINVAL;
	if (!change)
		return 0;

	e = osmo_use_count_find(uc, use);
	if (!e)
		e = osmo_use_count_repurpose_zero_entry(uc, use);
	if (!e)
		e = osmo_use_count_create(uc, use);
	if (!e)
		return -ENOMEM;

	if (!e->count) {
		/* move to end */
		llist_del(&e->entry);
		llist_add_tail(&e->entry, &uc->use_counts);
	}

	old_use_count = e->count;
	if (!count_safe(&e->count, change)) {
		e->count = old_use_count;
		return -ERANGE;
	}

	if (uc->use_cb)
		return uc->use_cb(e, old_use_count, file, line);
	return 0;
}

/*! Add N static use token entries to avoid dynamic allocation of use count tokens.
 * When not using this function, use count entries are talloc allocated from uc->talloc_object as talloc context. This
 * means that there are small dynamic allocations for each use count token. osmo_use_count_get_put() normally leaves
 * zero-count entries around and re-purposes them later, so the number of small allocations is at most the number of
 * concurrent differently-named uses of the same object. If that is not enough, this function allows completely avoiding
 * dynamic use count allocations, by adding N static entries with a zero count and a NULL use token.  They will be used
 * by osmo_use_count_get_put(), and, if the caller avoids using osmo_use_count_free(), the osmo_use_count implementation
 * never deallocates them. The idea is that the entries are members of the uc->talloc_object, or that they will by other
 * means be implicitly deallocated by the talloc_object. It is fine to call
 * osmo_use_count_make_static_entries(buf_n_entries=N) and later have more than N concurrent uses, i.e. it is no problem
 * to mix static and dynamic entries. To completely avoid dynamic use count entries, N has to >= the maximum number of
 * concurrent differently-named uses that will occur in the lifetime of the talloc_object.
 *
 *    struct my_object {
 *            struct osmo_use_count use_count;
 *            struct osmo_use_count_entry use_count_buf[3]; // planning for 3 concurrent users
 *    };
 *
 *    void example() {
 *            struct my_object *o = talloc_zero(ctx, struct my_object);
 *            osmo_use_count_make_static_entries(&o->use_count, o->use_count_buf, ARRAY_SIZE(o->use_count_buf));
 *    }
 */
void osmo_use_count_make_static_entries(struct osmo_use_count *uc, struct osmo_use_count_entry *buf,
					size_t buf_n_entries)
{
	size_t idx;
	if (!uc->use_counts.next)
		INIT_LLIST_HEAD(&uc->use_counts);
	for (idx = 0; idx < buf_n_entries; idx++) {
		struct osmo_use_count_entry *e = &buf[idx];
		*e = (struct osmo_use_count_entry){
			.use_count = uc,
		};
		llist_add_tail(&e->entry, &uc->use_counts);
	}
}

/*! @} */
