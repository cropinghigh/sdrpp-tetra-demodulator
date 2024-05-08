/*! \file use_count.h
 * Generic object usage counter API (get, put and deallocate on zero count).
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

#pragma once

#include <stdint.h>
#include <stdlib.h>

#include <osmocom/core/linuxlist.h>

/*! \defgroup use_count  Use Counter
 * @{
 * \file use_count.h
 */

struct osmo_use_count_entry;

/*! Invoked when a use count changes.
 *
 * The implementation is free to trigger actions on arbitrary use count changes, typically to free the
 * use_count->talloc_object when the total use count reaches zero.
 *
 * The implementation may modify use_count_entry->count, for example for handling of get()/put() bugs, to clamp specific use
 * tokens to specific counts, or to prevent the caller from put()ting into negative counts. When returning an error,
 * there is no implicit undo -- if errors need to be corrected, this function is responsible for that.
 *
 * Be aware: use token strings are not copied, and use count entries usually remain listed also when they reach a zero
 * count. This is trivially perfectly ok when using string literals as use tokens. It is also possible to use
 * dynamically allocated string tokens, but should a use token string become invalid memory when reaching zero count, it
 * is the responsibility of this function to set the use_count_entry->use = NULL; this is required to avoid subsequent
 * osmo_use_count_get_put() invocations from calling strcmp() on invalid memory. (Setting use = NULL cannot be done
 * implicitly after this callback invocation, because callback implementations are allowed to completely deallocate the
 * talloc_object and the use_count list entries, and setting use = NULL after that would be a use-after-free.)
 *
 * \param[in] use_count_entry  Use count entry that is being modified.
 * \param[in] old_use_count  Use count the item had before the change in use count.
 * \param[in] file  Source file string, passed in as __FILE__ from macro osmo_use_count_get_put().
 * \param[in] line  Source file line, passed in as __LINE__ from macro osmo_use_count_get_put().
 * \return 0 on success, negative if any undesired use count is reached; this rc will be returned by
 *         osmo_use_count_get_put().
 */
typedef int (* osmo_use_count_cb_t )(struct osmo_use_count_entry *use_count_entry, int32_t old_use_count,
				     const char *file, int line);

/*! Use counter state for one used object, managing N distinct named counters.
 *
 * Manage any number of uses of an object, with name tokens given to each use.
 *
 * A typical use tracking done by a single instance of this struct may look like:
 * "VLR subscr MSISDN-23 + SMS-receiver: now used by 6 (attached,2*SMS-receiver,SMS-pending,SMS,Paging)"
 * (This is a DREF log statement from an osmo-msc run delivering an SMS.)
 *
 * Use tokens are given as const char* strings. Typically string literals like "foo", __func__, or also NULL. Tokens may
 * be dynamically allocated or static char[] buffers as long as they are guaranteed to remain unchanged while referenced
 * by an osmo_use_count_entry. (Breakage occurs if one token magically changes to equal another listed token.)
 *
 * Instead of using string literals in the code directly, callers should use a #define, so that typos are caught at
 * compile time rather than introducing obscure failures that are hard to spot for humans -- don't use foo_get("bar")
 * and foo_put("bar"), but '#define FOO_USE_BAR "bar"' for foo_get(FOO_USE_BAR) and foo_put(FOO_USE_BAR).
 *
 * Counts are int32_t values, a separate count per use token string. Counts can be negative, though in the typical use
 * case are only positive or 0. Enforcing a range is entirely up to the osmo_use_count_cb_t() implementation.
 *
 * The talloc_object must be a pointer eligible to be a talloc context, i.e. either obtained from a function like
 * talloc_zero() or NULL. talloc_object is typically a pointer to the object that this struct is a member of.  Use count
 * entries may be allocated as talloc children of this (see also "Avoiding dynamic allocation" below).
 *
 * The use_cb() implementation allows to trigger actions when reaching specific use counts, e.g. deallocate when
 * reaching a total sum across all use tokens of zero.
 *
 * On initialization, this struct can be left fully zero initialized (the llist_head use_counts is implicitly
 * initialized upon the first osmo_use_count_get_put()). Usually, set only a talloc_object and a use_cb, though neither
 * is strictly required.
 *
 * Avoiding dynamic allocation: dynamic allocation can be avoided completely by providing sufficient static use count
 * entries with osmo_use_count_make_static_entries(). Otherwise, each new use token will dynamically allocate a new
 * osmo_use_count_entry; note that once allocated, these entries stay around even if they reached an entry count of
 * zero, and will be re-used for subsequent use count tokens. So even if not using osmo_use_count_make_static_entries(),
 * each osmo_use_count will keep dynamic allocations at a minimum. See also the documentation for osmo_use_count_cb_t.
 *
 * List traversal considerations: your typical use count list would max at about six entries in practice. Traversing six
 * llist->next pointers is less effort than doing a common strlen().
 *
 * Obtaining the total use count: osmo_use_count_total() traverses all use token entries and forms a sum. It is trivial
 * to keep a separate total count that completely avoids the need for calling this function, which is entirely up to the
 * individual osmo_use_count_cb_t() implementation. The optimization gained is usually not worth it, though.
 *
 * Use token comparison considerations: strcmp() to compare use tokens is a fairly good tradeoff:
 * - when the strings differ, strcmp() usually exits on the first or second character.
 * - when the strings are identical, they are usually the exact same char* address (from compile-time string constant),
 *   meaning that strcmp() is completely skipped.
 *   (quote: "if (e->use == use || (use && e->use && !strcmp(e->use, use)))")
 * - if we specified compile-time string constant use as requirement, we wouldn't need strcmp() at all, but this
 *   minuscule overhead has the benefit of complete correctness for any kinds of use token strings.
 *
 * Example:
 *
 *     struct foo {
 *             struct osmo_use_count use_count;
 *     };
 *
 *     // Convenience macros for struct foo instances. These are strict about use count errors.
 *     #define foo_get(FOO, USE) OSMO_ASSERT( osmo_use_count_get_put(&(FOO)->use_count, USE, 1) == 0 );
 *     #define foo_put(FOO, USE) OSMO_ASSERT( osmo_use_count_get_put(&(FOO)->use_count, USE, -1) == 0 );
 *
 *     int foo_use_cb(struct osmo_use_count_entry *use_count_entry, int32_t old_use_count, const char *file, int line)
 *     {
 *             struct foo *foo = use_count_entry->use_count->talloc_object;
 *             if (osmo_use_count_total(use_count_entry->use_count) == 0)
 *                     talloc_free(foo);
 *             return 0;
 *     }
 *
 *     // The function name is a convenient use token:
 *     void rx_stop_baz_request(struct foo *foo)
 *     {
 *             foo_get(foo, __func__);
 *
 *             foo_put(foo, "baz");
 *             printf("Stopped Bazing (%p)\n", foo);
 *
 *             foo_put(foo, __func__);
 *     }
 *
 *     void use_count_example()
 *     {
 *             struct foo *foo = talloc_zero(ctx, struct foo);
 *             *foo = (struct foo){
 *                     .use_count = {
 *                             .talloc_object = foo,
 *                             .use_cb = foo_use_cb,
 *                     },
 *             };
 *
 *             foo_get(foo, "bar");       // one osmo_use_count_entry was allocated
 *             foo_get(foo, "baz");       // a second osmo_use_count_entry was allocated
 *             foo_get(foo, "baz");       // still two entries
 *
 *             printf("use: %s\n", osmo_use_count_name_buf(namebuf, sizeof(namebuf), &foo->use_count));
 *             // "use: 3 (bar,2*baz)"
 *
 *             foo_put(foo, "bar");       // still two entries, one entry is idle ("bar"=0)
 *             foo_put(foo, "baz");
 *             rx_stop_baz_request(foo);
 *             // Final "baz" was put(), foo_use_cb() deallocated object foo, as well as all use count entries.
 *     };
 */
struct osmo_use_count {
	/*! Context to talloc-allocate use count entries from (if at all necessary); back-pointer to the owning object
	 * for osmo_use_count_cb_t implementations. */
	void *talloc_object;
	/*! If not NULL, this is invoked for each use count change. */
	osmo_use_count_cb_t use_cb;
	/*! List of use tokens. No need to touch this, the llist is initialized implicitly. */
	struct llist_head use_counts;
};

/*! One named counter in the list managed by osmo_use_count.
 * Gets created as necessary by osmo_use_count_get_put(). The total current use count of an object is the sum of all
 * individual osmo_use_count_entry->count.
 *
 *      object            <--backpointer-+
 *     t|  .osmo_use_count               |
 *     a|     .talloc_object ------------+
 *     l|     .use_counts llist:          use   count
 *     l|->      - osmo_use_count_entry: "foo"  1
 *     o|->      - osmo_use_count_entry: "bar"  3
 *     c|->      - osmo_use_count_entry: "baz"  0  (currently unused entry)
 */
struct osmo_use_count_entry {
	/*! Entry in osmo_use_count->use_counts. */
	struct llist_head entry;
	/*! Parent use count and backpointer to the talloc_object. */
	struct osmo_use_count *use_count;
	/*! Use token string that was passed to osmo_use_count_get_put(). */
	const char *use;
	/*! Current use count amount for only this use token string.
	 * If zero, this entry is currently unused and kept around to avoid frequent de-/allocation. */
	int32_t count;
};

/*! Change the use count for a given use token.
 * \param USE_LIST  A struct osmo_use_count*, e.g. &my_obj->use_count.
 * \param USE  A use token: arbitrary string (const char*). This must remain valid memory, e.g. string constants.
 * \param CHANGE  Signed integer value to add to the use count: positive means get(), negative means put().
 * \return Negative on range violations or USE_LIST == NULL, the use_cb()'s return value, or 0 on success.
 */
#define osmo_use_count_get_put(USE_LIST, USE, CHANGE) \
	_osmo_use_count_get_put(USE_LIST, USE, CHANGE, __FILE__, __LINE__)

int _osmo_use_count_get_put(struct osmo_use_count *uc, const char *use, int32_t change,
			    const char *file, int line);

const char *osmo_use_count_name_buf(char *buf, size_t buf_len, const struct osmo_use_count *uc);
int osmo_use_count_to_str_buf(char *buf, size_t buf_len, const struct osmo_use_count *uc);
char *osmo_use_count_to_str_c(void *ctx, const struct osmo_use_count *uc);

int32_t osmo_use_count_total(const struct osmo_use_count *uc);
int32_t osmo_use_count_by(const struct osmo_use_count *uc, const char *use);

struct osmo_use_count_entry *osmo_use_count_find(const struct osmo_use_count *uc, const char *use);
void osmo_use_count_free(struct osmo_use_count_entry *use_count_entry);

void osmo_use_count_make_static_entries(struct osmo_use_count *uc, struct osmo_use_count_entry *buf,
					size_t buf_n_entries);

/*! @} */
