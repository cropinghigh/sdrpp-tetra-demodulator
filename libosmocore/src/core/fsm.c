/*! \file fsm.c
 * Osmocom generic Finite State Machine implementation. */
/*
 * (C) 2016-2019 by Harald Welte <laforge@gnumonks.org>
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

#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include <osmocom/core/fsm.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/logging.h>
#include <osmocom/core/utils.h>

/*! \addtogroup fsm
 *  @{
 *  Finite State Machine abstraction
 *
 *  This is a generic C-language abstraction for implementing finite
 *  state machines within the Osmocom framework.  It is intended to
 *  replace existing hand-coded or even only implicitly existing FSMs
 *  all over the existing code base.
 *
 *  An libosmocore FSM is described by its \ref osmo_fsm description,
 *  which in turn refers to an array of \ref osmo_fsm_state descriptor,
 *  each describing a single state in the FSM.
 *
 *  The general idea is that all actions performed within one state are
 *  located at one position in the code (the state's action function),
 *  as opposed to the 'message-centric' view of e.g. the existing
 *  state machines of the LAPD(m) core, where there is one message for
 *  each possible event (primitive), and the function then needs to
 *  concern itself on how to handle that event over all possible states.
 *
 *  For each state, there is a bit-mask of permitted input events for
 *  this state, as well as a bit-mask of permitted new output states to
 *  which the state can change.  Furthermore, there is a function
 *  pointer implementing the actual handling of the input events
 *  occurring whilst in that state.
 *
 *  Furthermore, each state offers a function pointer that can be
 *  executed just before leaving a state, and another one just after
 *  entering a state.
 *
 *  When transitioning into a new state, an optional timer number and
 *  time-out can be passed along.  The timer is started just after
 *  entering the new state, and will call the \ref osmo_fsm timer_cb
 *  function once it expires.  This is intended to be used in telecom
 *  state machines where a given timer (identified by a certain number)
 *  is started to terminate the fsm or terminate the fsm once expected
 *  events are not happening before timeout expiration.
 *
 *  As there can often be many concurrent FSMs of one given class, we
 *  introduce the concept of \ref osmo_fsm_inst, i.e. an FSM instance.
 *  The instance keeps the actual state, while the \ref osmo_fsm
 *  descriptor contains the static/const descriptor of the FSM's states
 *  and possible transitions.
 *
 *  osmo_fsm are integrated with the libosmocore logging system.  The
 *  logging sub-system is determined by the FSM descriptor, as we assume
 *  one FSM (let's say one related to a location update procedure) is
 *  inevitably always tied to a sub-system.  The logging level however
 *  is configurable for each FSM instance, to ensure that e.g. DEBUG
 *  logging can be used for the LU procedure of one subscriber, while
 *  NOTICE level is used for all other subscribers.
 *
 *  In order to attach private state to the \ref osmo_fsm_inst, it
 *  offers an opaque private pointer.
 *
 * \file fsm.c */

LLIST_HEAD(osmo_g_fsms);
static bool fsm_log_addr = true;
static bool fsm_log_timeouts = false;
/*! See osmo_fsm_term_safely(). */
static bool fsm_term_safely_enabled = false;

/*! Internal state for FSM instance termination cascades. */
static __thread struct {
	/*! The first FSM instance that invoked osmo_fsm_inst_term() in the current cascade. */
	struct osmo_fsm_inst *root_fi;
	/*! 2 if a secondary FSM terminates, 3 if a secondary FSM causes a tertiary FSM to terminate, and so on. */
	unsigned int depth;
	/*! Talloc context to collect all deferred deallocations (FSM instances, and talloc objects if any). */
	void *collect_ctx;
	/*! See osmo_fsm_set_dealloc_ctx() */
	void *fsm_dealloc_ctx;
} fsm_term_safely;

/*! Internal call to free an FSM instance, which redirects to the context set by osmo_fsm_set_dealloc_ctx() if any.
 */
static void fsm_free_or_steal(void *talloc_object)
{
	if (fsm_term_safely.fsm_dealloc_ctx)
		talloc_steal(fsm_term_safely.fsm_dealloc_ctx, talloc_object);
	else
		talloc_free(talloc_object);
}

/*! specify if FSM instance addresses should be logged or not
 *
 *  By default, the FSM name includes the pointer address of the \ref
 *  osmo_fsm_inst.  This behavior can be disabled (and re-enabled)
 *  using this function.
 *
 *  \param[in] log_addr Indicate if FSM instance address shall be logged
 */
void osmo_fsm_log_addr(bool log_addr)
{
	fsm_log_addr = log_addr;
}

/*! Enable or disable logging of timeout values for FSM instance state changes.
 *
 * By default, state changes are logged by state name only, omitting the timeout. When passing true, each state change
 * will also log the T number (or Osmocom-specific X number) and the chosen timeout in seconds.
 * osmo_fsm_inst_state_chg_keep_timer() will log remaining timeout in millisecond precision.
 *
 * The default for this is false to reflect legacy behavior. Since various C tests that verify logging output already
 * existed prior to this option, keeping timeout logging off makes sure that they continue to pass. Particularly,
 * osmo_fsm_inst_state_chg_keep_timer() may cause non-deterministic logging of remaining timeout values.
 *
 * For any program that does not explicitly require deterministic logging output, i.e. anything besides regression tests
 * involving FSM instances, it is recommended to call osmo_fsm_log_timeouts(true).
 *
 * \param[in] log_timeouts  Pass true to log timeouts on state transitions, false to omit timeouts.
 */
void osmo_fsm_log_timeouts(bool log_timeouts)
{
	fsm_log_timeouts = log_timeouts;
}

/*! Enable safer way to deallocate cascades of terminating FSM instances.
 *
 * Note, using osmo_fsm_set_dealloc_ctx() is a more general solution to this same problem.
 * Particularly, in a program using osmo_select_main_ctx(), the simplest solution to avoid most use-after-free problems
 * from FSM instance deallocation is using osmo_fsm_set_dealloc_ctx(OTC_SELECT).
 *
 * When enabled, an FSM instance termination detects whether another FSM instance is already terminating, and instead of
 * deallocating immediately, collects all terminating FSM instances in a talloc context, to be bulk deallocated once all
 * event handling and termination cascades are done.
 *
 * For example, if an FSM's cleanup() sends an event to some "other" FSM, which in turn causes the FSM's parent to
 * deallocate, then the parent would talloc_free() the child's memory, causing a use-after-free. There are infinite
 * constellations like this, which all are trivially solved with this feature enabled.
 *
 * For illustration, see fsm_dealloc_test.c.
 *
 * When enabled, this feature changes the order of logging, which may break legacy unit test expectations, and changes
 * the order of deallocation to after the parent term event is dispatched.
 *
 * \param[in] term_safely  Pass true to switch to safer FSM instance termination behavior.
 */
void osmo_fsm_term_safely(bool term_safely)
{
	fsm_term_safely_enabled = term_safely;
}

/*! Instead of deallocating FSM instances, move them to the given talloc context.
 *
 * It is the caller's responsibility to clear this context to actually free the memory of terminated FSM instances.
 * Make sure to not talloc_free(ctx) itself before setting a different osmo_fsm_set_dealloc_ctx(). To clear a ctx
 * without the need to call osmo_fsm_set_dealloc_ctx() again, rather use talloc_free_children(ctx).
 *
 * For example, to defer deallocation to the next osmo_select_main_ctx() iteration, set this to OTC_SELECT.
 *
 * Deferring deallocation is the simplest solution to avoid most use-after-free problems from FSM instance deallocation.
 * This is a simpler and more general solution than osmo_fsm_term_safely().
 *
 * To disable the feature again, pass NULL as ctx.
 *
 * Both osmo_fsm_term_safely() and osmo_fsm_set_dealloc_ctx() can be enabled at the same time, which will result in
 * first collecting deallocated FSM instances in fsm_term_safely.collect_ctx, and finally reparenting that to the ctx
 * passed here. However, in practice, it does not really make sense to enable both at the same time.
 *
 * \param ctx[in]  Instead of talloc_free()int, talloc_steal() all future deallocated osmo_fsm_inst instances to this
 *                 ctx. If NULL, go back to talloc_free() as usual.
 */
void osmo_fsm_set_dealloc_ctx(void *ctx)
{
	fsm_term_safely.fsm_dealloc_ctx = ctx;
}

/*! talloc_free() the given object immediately, or once ongoing FSM terminations are done.
 *
 * If an FSM deallocation cascade is ongoing, talloc_steal() the given talloc_object into the talloc context that is
 * freed once the cascade is done. If no FSM deallocation cascade is ongoing, or if osmo_fsm_term_safely() is disabled,
 * immediately talloc_free the object.
 *
 * This can be useful if some higher order talloc object, which is the talloc parent for FSM instances or their priv
 * objects, is not itself tied to an FSM instance. This function allows safely freeing it without affecting ongoing FSM
 * termination cascades.
 *
 * Once passed to this function, the talloc_object should be considered as already freed. Only FSM instance pre_term()
 * and cleanup() functions as well as event handling caused by these may safely assume that it is still valid memory.
 *
 * The talloc_object should not have multiple parents.
 *
 * (This function may some day move to public API, which might be redundant if we introduce a select-loop volatile
 * context mechanism to defer deallocation instead.)
 *
 * \param[in] talloc_object  Object pointer to free.
 */
static void osmo_fsm_defer_free(void *talloc_object)
{
	if (!fsm_term_safely.depth) {
		fsm_free_or_steal(talloc_object);
		return;
	}

	if (!fsm_term_safely.collect_ctx) {
		/* This is actually the first other object / FSM instance besides the root terminating inst. Create the
		 * ctx to collect this and possibly more objects to free. Avoid talloc parent loops: don't make this ctx
		 * the child of the root inst or anything like that. */
		fsm_term_safely.collect_ctx = talloc_named_const(NULL, 0, "fsm_term_safely.collect_ctx");
		OSMO_ASSERT(fsm_term_safely.collect_ctx);
	}
	talloc_steal(fsm_term_safely.collect_ctx, talloc_object);
}

struct osmo_fsm *osmo_fsm_find_by_name(const char *name)
{
	struct osmo_fsm *fsm;
	llist_for_each_entry(fsm, &osmo_g_fsms, list) {
		if (!strcmp(name, fsm->name))
			return fsm;
	}
	return NULL;
}

struct osmo_fsm_inst *osmo_fsm_inst_find_by_name(const struct osmo_fsm *fsm,
						 const char *name)
{
	struct osmo_fsm_inst *fi;

	if (!name)
		return NULL;

	llist_for_each_entry(fi, &fsm->instances, list) {
		if (!fi->name)
			continue;
		if (!strcmp(name, fi->name))
			return fi;
	}
	return NULL;
}

struct osmo_fsm_inst *osmo_fsm_inst_find_by_id(const struct osmo_fsm *fsm,
						const char *id)
{
	struct osmo_fsm_inst *fi;

	llist_for_each_entry(fi, &fsm->instances, list) {
		if (!strcmp(id, fi->id))
			return fi;
	}
	return NULL;
}

/*! register a FSM with the core
 *
 *  A FSM descriptor needs to be registered with the core before any
 *  instances can be created for it.
 *
 *  \param[in] fsm Descriptor of Finite State Machine to be registered
 *  \returns 0 on success; negative on error
 */
int osmo_fsm_register(struct osmo_fsm *fsm)
{
	if (!osmo_identifier_valid(fsm->name)) {
		LOGP(DLGLOBAL, LOGL_ERROR, "Attempting to register FSM with illegal identifier '%s'\n", fsm->name);
		return -EINVAL;
	}
	if (osmo_fsm_find_by_name(fsm->name))
		return -EEXIST;
	if (fsm->event_names == NULL)
		LOGP(DLGLOBAL, LOGL_ERROR, "FSM '%s' has no event names! Please fix!\n", fsm->name);
	llist_add_tail(&fsm->list, &osmo_g_fsms);
	INIT_LLIST_HEAD(&fsm->instances);

	return 0;
}

/*! unregister a FSM from the core
 *
 *  Once the FSM descriptor is unregistered, active instances can still
 *  use it, but no new instances may be created for it.
 *
 *  \param[in] fsm Descriptor of Finite State Machine to be removed
 */
void osmo_fsm_unregister(struct osmo_fsm *fsm)
{
	llist_del(&fsm->list);
}

/* small wrapper function around timer expiration (for logging) */
static void fsm_tmr_cb(void *data)
{
	struct osmo_fsm_inst *fi = data;
	struct osmo_fsm *fsm = fi->fsm;
	int32_t T = fi->T;

	LOGPFSM(fi, "Timeout of " OSMO_T_FMT "\n", OSMO_T_FMT_ARGS(fi->T));

	if (fsm->timer_cb) {
		int rc = fsm->timer_cb(fi);
		if (rc != 1)
			/* We don't actually know whether fi exists anymore.
			 * Make sure to not access it and return right away. */
			return;
		/* The timer_cb told us to terminate, so we can safely assume
		 * that fi still exists. */
		LOGPFSM(fi, "timer_cb requested termination\n");
	} else
		LOGPFSM(fi, "No timer_cb, automatic termination\n");

	/* if timer_cb returns 1 or there is no timer_cb */
	osmo_fsm_inst_term(fi, OSMO_FSM_TERM_TIMEOUT, &T);
}

/*! Change id of the FSM instance
 * \param[in] fi FSM instance
 * \param[in] id new ID
 * \returns 0 if the ID was updated, otherwise -EINVAL
 */
int osmo_fsm_inst_update_id(struct osmo_fsm_inst *fi, const char *id)
{
	if (!id)
		return osmo_fsm_inst_update_id_f(fi, NULL);
	else
		return osmo_fsm_inst_update_id_f(fi, "%s", id);
}

static void update_name(struct osmo_fsm_inst *fi)
{
	if (fi->name)
		talloc_free((char*)fi->name);

	if (!fsm_log_addr) {
		if (fi->id)
			fi->name = talloc_asprintf(fi, "%s(%s)", fi->fsm->name, fi->id);
		else
			fi->name = talloc_asprintf(fi, "%s", fi->fsm->name);
	} else {
		if (fi->id)
			fi->name = talloc_asprintf(fi, "%s(%s)[%p]", fi->fsm->name, fi->id, fi);
		else
			fi->name = talloc_asprintf(fi, "%s[%p]", fi->fsm->name, fi);
	}
}

/*! Change id of the FSM instance using a string format.
 * \param[in] fi FSM instance.
 * \param[in] fmt format string to compose new ID.
 * \param[in] ... variable argument list for format string.
 * \returns 0 if the ID was updated, otherwise -EINVAL.
 */
int osmo_fsm_inst_update_id_f(struct osmo_fsm_inst *fi, const char *fmt, ...)
{
	char *id = NULL;

	if (fmt) {
		va_list ap;

		va_start(ap, fmt);
		id = talloc_vasprintf(fi, fmt, ap);
		va_end(ap);

		if (!osmo_identifier_valid(id)) {
			LOGP(DLGLOBAL, LOGL_ERROR,
			     "Attempting to set illegal id for FSM instance of type '%s': %s\n",
			     fi->fsm->name, osmo_quote_str(id, -1));
			talloc_free(id);
			return -EINVAL;
		}
	}

	if (fi->id)
		talloc_free((char*)fi->id);
	fi->id = id;

	update_name(fi);
	return 0;
}

/*! Change id of the FSM instance using a string format, and ensuring a valid id.
 * Replace any characters that are not permitted as FSM identifier with replace_with.
 * \param[in] fi FSM instance.
 * \param[in] replace_with Character to use instead of non-permitted FSM id characters.
 *                         Make sure to choose a legal character, e.g. '-'.
 * \param[in] fmt format string to compose new ID.
 * \param[in] ... variable argument list for format string.
 * \returns 0 if the ID was updated, otherwise -EINVAL.
 */
int osmo_fsm_inst_update_id_f_sanitize(struct osmo_fsm_inst *fi, char replace_with, const char *fmt, ...)
{
	char *id = NULL;
	va_list ap;
	int rc;

	if (!fmt)
		return osmo_fsm_inst_update_id(fi, NULL);

	va_start(ap, fmt);
	id = talloc_vasprintf(fi, fmt, ap);
	va_end(ap);

	osmo_identifier_sanitize_buf(id, NULL, replace_with);

	rc = osmo_fsm_inst_update_id(fi, id);
	talloc_free(id);
	return rc;
}

/*! allocate a new instance of a specified FSM
 *  \param[in] fsm Descriptor of the FSM
 *  \param[in] ctx talloc context from which to allocate memory
 *  \param[in] priv private data reference store in fsm instance
 *  \param[in] log_level The log level for events of this FSM
 *  \param[in] id The name/ID of the FSM instance
 *  \returns newly-allocated, initialized and registered FSM instance
 */
struct osmo_fsm_inst *osmo_fsm_inst_alloc(struct osmo_fsm *fsm, void *ctx, void *priv,
					  int log_level, const char *id)
{
	struct osmo_fsm_inst *fi = talloc_zero(ctx, struct osmo_fsm_inst);

	fi->fsm = fsm;
	fi->priv = priv;
	fi->log_level = log_level;
	osmo_timer_setup(&fi->timer, fsm_tmr_cb, fi);

	if (osmo_fsm_inst_update_id(fi, id) < 0) {
		fsm_free_or_steal(fi);
		return NULL;
	}

	INIT_LLIST_HEAD(&fi->proc.children);
	INIT_LLIST_HEAD(&fi->proc.child);
	llist_add(&fi->list, &fsm->instances);

	LOGPFSM(fi, "Allocated\n");

	return fi;
}

/*! allocate a new instance of a specified FSM as child of
 *  other FSM instance
 *
 *  This is like \ref osmo_fsm_inst_alloc but using the parent FSM as
 *  talloc context, and inheriting the log level of the parent.
 *
 *  \param[in] fsm Descriptor of the to-be-allocated FSM
 *  \param[in] parent Parent FSM instance
 *  \param[in] parent_term_event Event to be sent to parent when terminating
 *  \returns newly-allocated, initialized and registered FSM instance
 */
struct osmo_fsm_inst *osmo_fsm_inst_alloc_child(struct osmo_fsm *fsm,
						struct osmo_fsm_inst *parent,
						uint32_t parent_term_event)
{
	struct osmo_fsm_inst *fi;

	fi = osmo_fsm_inst_alloc(fsm, parent, NULL, parent->log_level,
				 parent->id);
	if (!fi) {
		/* indicate immediate termination to caller */
		osmo_fsm_inst_dispatch(parent, parent_term_event, NULL);
		return NULL;
	}

	LOGPFSM(fi, "is child of %s\n", osmo_fsm_inst_name(parent));

	osmo_fsm_inst_change_parent(fi, parent, parent_term_event);

	return fi;
}

/*! unlink child FSM from its parent FSM.
 *  \param[in] fi Descriptor of the child FSM to unlink.
 *  \param[in] ctx New talloc context
 *
 * Never call this function from the cleanup callback, because at that time
 * the child FSMs will already be terminated. If unlinking should be performed
 * on FSM termination, use the grace callback instead. */
void osmo_fsm_inst_unlink_parent(struct osmo_fsm_inst *fi, void *ctx)
{
	if (fi->proc.parent) {
		talloc_steal(ctx, fi);
		fi->proc.parent = NULL;
		fi->proc.parent_term_event = 0;
		llist_del(&fi->proc.child);
	}
}

/*! change parent instance of an FSM.
 *  \param[in] fi Descriptor of the to-be-allocated FSM.
 *  \param[in] new_parent New parent FSM instance.
 *  \param[in] new_parent_term_event Event to be sent to parent when terminating.
 *
 * Never call this function from the cleanup callback!
 * (see also osmo_fsm_inst_unlink_parent()).*/
void osmo_fsm_inst_change_parent(struct osmo_fsm_inst *fi,
				 struct osmo_fsm_inst *new_parent,
				 uint32_t new_parent_term_event)
{
	/* Make sure a possibly existing old parent is unlinked first
	 * (new_parent can be NULL) */
	osmo_fsm_inst_unlink_parent(fi, new_parent);

	/* Add new parent */
	if (new_parent) {
		fi->proc.parent = new_parent;
		fi->proc.parent_term_event = new_parent_term_event;
		llist_add(&fi->proc.child, &new_parent->proc.children);
	}
}

/*! delete a given instance of a FSM
 *  \param[in] fi FSM instance to be un-registered and deleted
 */
void osmo_fsm_inst_free(struct osmo_fsm_inst *fi)
{
	osmo_timer_del(&fi->timer);
	llist_del(&fi->list);

	if (fsm_term_safely.depth) {
		/* Another FSM instance has caused this one to free and is still busy with its termination. Don't free
		 * yet, until the other FSM instance is done. */
		osmo_fsm_defer_free(fi);
		/* The root_fi can't go missing really, but to be safe... */
		if (fsm_term_safely.root_fi)
			LOGPFSM(fi, "Deferring: will deallocate with %s\n", fsm_term_safely.root_fi->name);
		else
			LOGPFSM(fi, "Deferring deallocation\n");

		/* Don't free anything yet. Exit. */
		return;
	}

	/* fsm_term_safely.depth == 0.
	 * - If fsm_term_safely is enabled, this is the original FSM instance that started terminating first. Free this
	 *   and along with it all other collected terminated FSM instances.
	 * - If fsm_term_safely is disabled, this is just any FSM instance deallocating. */

	if (fsm_term_safely.collect_ctx) {
		/* The fi may be a child of any other FSM instances or objects collected in the collect_ctx. Don't
		 * deallocate separately to avoid use-after-free errors, put it in there and deallocate all at once. */
		LOGPFSM(fi, "Deallocated, including all deferred deallocations\n");
		osmo_fsm_defer_free(fi);
		fsm_free_or_steal(fsm_term_safely.collect_ctx);
		fsm_term_safely.collect_ctx = NULL;
	} else {
		LOGPFSM(fi, "Deallocated\n");
		fsm_free_or_steal(fi);
	}
	fsm_term_safely.root_fi = NULL;
}

/*! get human-readable name of FSM event
 *  \param[in] fsm FSM descriptor of event
 *  \param[in] event Event integer value
 *  \returns string rendering of the event
 */
const char *osmo_fsm_event_name(const struct osmo_fsm *fsm, uint32_t event)
{
	static __thread char buf[32];
	if (!fsm->event_names) {
		snprintf(buf, sizeof(buf), "%"PRIu32, event);
		return buf;
	} else
		return get_value_string(fsm->event_names, event);
}

/*! get human-readable name of FSM instance
 *  \param[in] fi FSM instance
 *  \returns string rendering of the FSM identity
 */
const char *osmo_fsm_inst_name(const struct osmo_fsm_inst *fi)
{
	if (!fi)
		return "NULL";

	if (fi->name)
		return fi->name;
	else
		return fi->fsm->name;
}

/*! get human-readable name of FSM state
 *  \param[in] fsm FSM descriptor
 *  \param[in] state FSM state number
 *  \returns string rendering of the FSM state
 */
const char *osmo_fsm_state_name(const struct osmo_fsm *fsm, uint32_t state)
{
	static __thread char buf[32];
	if (state >= fsm->num_states) {
		snprintf(buf, sizeof(buf), "unknown %"PRIu32, state);
		return buf;
	} else
		return fsm->states[state].name;
}

static int state_chg(struct osmo_fsm_inst *fi, uint32_t new_state,
		     bool keep_timer, unsigned long timeout_ms, int T,
		     const char *file, int line)
{
	struct osmo_fsm *fsm = fi->fsm;
	uint32_t old_state = fi->state;
	const struct osmo_fsm_state *st = &fsm->states[fi->state];
	struct timeval remaining;

	if (fi->proc.terminating) {
		LOGPFSMSRC(fi, file, line,
			   "FSM instance already terminating, not changing state to %s\n",
			   osmo_fsm_state_name(fsm, new_state));
		return -EINVAL;
	}

	/* validate if new_state is a valid state */
	if (!(st->out_state_mask & (1 << new_state))) {
		LOGPFSMLSRC(fi, LOGL_ERROR, file, line,
			    "transition to state %s not permitted!\n",
			    osmo_fsm_state_name(fsm, new_state));
		return -EPERM;
	}

	if (!keep_timer) {
		/* delete the old timer */
		osmo_timer_del(&fi->timer);
	}

	if (st->onleave)
		st->onleave(fi, new_state);

	if (fsm_log_timeouts) {
		char trailer[64];
		trailer[0] = '\0';
		if (keep_timer && fi->timer.active) {
			/* This should always give us a timeout, but just in case the return value indicates error, omit
			 * logging the remaining time. */
			if (osmo_timer_remaining(&fi->timer, NULL, &remaining))
				snprintf(trailer, sizeof(trailer), "(keeping " OSMO_T_FMT ")",
					 OSMO_T_FMT_ARGS(fi->T));
			else
				snprintf(trailer, sizeof(trailer), "(keeping " OSMO_T_FMT
					  ", %ld.%03lds remaining)", OSMO_T_FMT_ARGS(fi->T),
					  (long) remaining.tv_sec, remaining.tv_usec / 1000);
		} else if (timeout_ms) {
			if (timeout_ms % 1000 == 0)
				/* keep log output legacy compatible to avoid autotest failures */
				snprintf(trailer, sizeof(trailer), "(" OSMO_T_FMT ", %lus)",
					   OSMO_T_FMT_ARGS(T), timeout_ms/1000);
			else
				snprintf(trailer, sizeof(trailer), "(" OSMO_T_FMT ", %lums)",
					   OSMO_T_FMT_ARGS(T), timeout_ms);
		} else
			snprintf(trailer, sizeof(trailer), "(no timeout)");

		LOGPFSMSRC(fi, file, line, "State change to %s %s\n",
			   osmo_fsm_state_name(fsm, new_state), trailer);
	} else {
		LOGPFSMSRC(fi, file, line, "state_chg to %s\n",
			   osmo_fsm_state_name(fsm, new_state));
	}

	fi->state = new_state;
	st = &fsm->states[new_state];

	if (!keep_timer
	    || (keep_timer && !osmo_timer_pending(&fi->timer))) {
		fi->T = T;
		if (timeout_ms) {
			osmo_timer_schedule(&fi->timer,
			      /* seconds */ (timeout_ms / 1000),
			 /* microseconds */ (timeout_ms % 1000) * 1000);
		}
	}

	/* Call 'onenter' last, user might terminate FSM from there */
	if (st->onenter)
		st->onenter(fi, old_state);

	return 0;
}

/*! perform a state change of the given FSM instance
 *
 *  Best invoke via the osmo_fsm_inst_state_chg() macro which logs the source
 *  file where the state change was effected. Alternatively, you may pass \a
 *  file as NULL to use the normal file/line indication instead.
 *
 *  All changes to the FSM instance state must be made via an osmo_fsm_inst_state_chg_*
 *  function.  It verifies that the existing state actually permits a
 *  transition to new_state.
 *
 *  If timeout_secs is 0, stay in the new state indefinitely, without a timeout
 *  (stop the FSM instance's timer if it was runnning).
 *
 *  If timeout_secs > 0, start or reset the FSM instance's timer with this
 *  timeout. On expiry, invoke the FSM instance's timer_cb -- if no timer_cb is
 *  set, an expired timer immediately terminates the FSM instance with
 *  OSMO_FSM_TERM_TIMEOUT.
 *
 *  The value of T is stored in fi->T and is then available for query in
 *  timer_cb. If passing timeout_secs == 0, it is recommended to also pass T ==
 *  0, so that fi->T is reset to 0 when no timeout is invoked.
 *
 *  Positive values for T are considered to be 3GPP spec compliant and appear in
 *  logging and VTY as "T1234", while negative values are considered to be
 *  Osmocom specific timers, represented in logging and VTY as "X1234".
 *
 *  See also osmo_tdef_fsm_inst_state_chg() from the osmo_tdef API, which
 *  provides a unified way to configure and apply GSM style Tnnnn timers to FSM
 *  state transitions.
 *
 *  \param[in] fi FSM instance whose state is to change
 *  \param[in] new_state The new state into which we should change
 *  \param[in] timeout_secs Timeout in seconds (if !=0), maximum-clamped to 2147483647 seconds.
 *  \param[in] T Timer number, where positive numbers are considered to be 3GPP spec compliant timer numbers and are
 *               logged as "T1234", while negative numbers are considered Osmocom specific timer numbers logged as
 *               "X1234".
 *  \param[in] file Calling source file (from osmo_fsm_inst_state_chg macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_state_chg macro)
 *  \returns 0 on success; negative on error
 */
int _osmo_fsm_inst_state_chg(struct osmo_fsm_inst *fi, uint32_t new_state,
			     unsigned long timeout_secs, int T,
			     const char *file, int line)
{
	return state_chg(fi, new_state, false, timeout_secs*1000, T, file, line);
}
int _osmo_fsm_inst_state_chg_ms(struct osmo_fsm_inst *fi, uint32_t new_state,
				unsigned long timeout_ms, int T,
				const char *file, int line)
{
	return state_chg(fi, new_state, false, timeout_ms, T, file, line);
}

/*! perform a state change while keeping the current timer running.
 *
 *  This is useful to keep a timeout across several states (without having to round the
 *  remaining time to seconds).
 *
 *  Best invoke via the osmo_fsm_inst_state_chg_keep_timer() macro which logs the source
 *  file where the state change was effected. Alternatively, you may pass \a
 *  file as NULL to use the normal file/line indication instead.
 *
 *  All changes to the FSM instance state must be made via an osmo_fsm_inst_state_chg_*
 *  function.  It verifies that the existing state actually permits a
 *  transition to new_state.
 *
 *  \param[in] fi FSM instance whose state is to change
 *  \param[in] new_state The new state into which we should change
 *  \param[in] file Calling source file (from osmo_fsm_inst_state_chg macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_state_chg macro)
 *  \returns 0 on success; negative on error
 */
int _osmo_fsm_inst_state_chg_keep_timer(struct osmo_fsm_inst *fi, uint32_t new_state,
					const char *file, int line)
{
	return state_chg(fi, new_state, true, 0, 0, file, line);
}

/*! perform a state change while keeping the current timer if running, or starting a timer otherwise.
 *
 *  This is useful to keep a timeout across several states, but to make sure that some timeout is actually running.
 *
 *  Best invoke via the osmo_fsm_inst_state_chg_keep_or_start_timer() macro which logs the source file where the state
 *  change was effected. Alternatively, you may pass file as NULL to use the normal file/line indication instead.
 *
 *  All changes to the FSM instance state must be made via an osmo_fsm_inst_state_chg_*
 *  function.  It verifies that the existing state actually permits a
 *  transition to new_state.
 *
 *  \param[in] fi FSM instance whose state is to change
 *  \param[in] new_state The new state into which we should change
 *  \param[in] timeout_secs If no timer is running yet, set this timeout in seconds (if !=0), maximum-clamped to
 *                          2147483647 seconds.
 *  \param[in] T Timer number, where positive numbers are considered to be 3GPP spec compliant timer numbers and are
 *               logged as "T1234", while negative numbers are considered Osmocom specific timer numbers logged as
 *               "X1234".
 *  \param[in] file Calling source file (from osmo_fsm_inst_state_chg macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_state_chg macro)
 *  \returns 0 on success; negative on error
 */
int _osmo_fsm_inst_state_chg_keep_or_start_timer(struct osmo_fsm_inst *fi, uint32_t new_state,
						 unsigned long timeout_secs, int T,
						 const char *file, int line)
{
	return state_chg(fi, new_state, true, timeout_secs*1000, T, file, line);
}
int _osmo_fsm_inst_state_chg_keep_or_start_timer_ms(struct osmo_fsm_inst *fi, uint32_t new_state,
						    unsigned long timeout_ms, int T,
						    const char *file, int line)
{
	return state_chg(fi, new_state, true, timeout_ms, T, file, line);
}


/*! dispatch an event to an osmocom finite state machine instance
 *
 *  Best invoke via the osmo_fsm_inst_dispatch() macro which logs the source
 *  file where the event was effected. Alternatively, you may pass \a file as
 *  NULL to use the normal file/line indication instead.
 *
 *  Any incoming events to \ref osmo_fsm instances must be dispatched to
 *  them via this function.  It verifies, whether the event is permitted
 *  based on the current state of the FSM.  If not, -1 is returned.
 *
 *  \param[in] fi FSM instance
 *  \param[in] event Event to send to FSM instance
 *  \param[in] data Data to pass along with the event
 *  \param[in] file Calling source file (from osmo_fsm_inst_dispatch macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_dispatch macro)
 *  \returns 0 in case of success; negative on error
 */
int _osmo_fsm_inst_dispatch(struct osmo_fsm_inst *fi, uint32_t event, void *data,
			    const char *file, int line)
{
	struct osmo_fsm *fsm;
	const struct osmo_fsm_state *fs;

	if (!fi) {
		LOGPSRC(DLGLOBAL, LOGL_ERROR, file, line,
			"Trying to dispatch event %"PRIu32" to non-existent"
			" FSM instance!\n", event);
		osmo_log_backtrace(DLGLOBAL, LOGL_ERROR);
		return -ENODEV;
	}

	fsm = fi->fsm;

	if (fi->proc.terminating) {
		LOGPFSMSRC(fi, file, line,
			   "FSM instance already terminating, not dispatching event %s\n",
			   osmo_fsm_event_name(fsm, event));
		return -EINVAL;
	}

	OSMO_ASSERT(fi->state < fsm->num_states);
	fs = &fi->fsm->states[fi->state];

	LOGPFSMSRC(fi, file, line,
		   "Received Event %s\n", osmo_fsm_event_name(fsm, event));

	if (((1 << event) & fsm->allstate_event_mask) && fsm->allstate_action) {
		fsm->allstate_action(fi, event, data);
		return 0;
	}

	if (!((1 << event) & fs->in_event_mask)) {
		LOGPFSMLSRC(fi, LOGL_ERROR, file, line,
			    "Event %s not permitted\n",
			    osmo_fsm_event_name(fsm, event));
		return -1;
	}

	if (fs->action)
		fs->action(fi, event, data);

	return 0;
}

/*! Terminate FSM instance with given cause
 *
 *  This safely terminates the given FSM instance by first iterating
 *  over all children and sending them a termination event.  Next, it
 *  calls the FSM descriptors cleanup function (if any), followed by
 *  releasing any memory associated with the FSM instance.
 *
 *  Finally, the parent FSM instance (if any) is notified using the
 *  parent termination event configured at time of FSM instance start.
 *
 *  \param[in] fi FSM instance to be terminated
 *  \param[in] cause Cause / reason for termination
 *  \param[in] data Opaque event data to be passed with the parent term event
 *  \param[in] file Calling source file (from osmo_fsm_inst_term macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_term macro)
 */
void _osmo_fsm_inst_term(struct osmo_fsm_inst *fi,
			 enum osmo_fsm_term_cause cause, void *data,
			 const char *file, int line)
{
	struct osmo_fsm_inst *parent;
	uint32_t parent_term_event = fi->proc.parent_term_event;

	if (fi->proc.terminating) {
		LOGPFSMSRC(fi, file, line, "Ignoring trigger to terminate: already terminating\n");
		return;
	}
	fi->proc.terminating = true;

	/* Start termination cascade handling only if the feature is enabled. Also check the current depth: though
	 * unlikely, theoretically the fsm_term_safely_enabled flag could be toggled in the middle of a cascaded
	 * termination, so make sure to continue if it already started. */
	if (fsm_term_safely_enabled || fsm_term_safely.depth) {
		fsm_term_safely.depth++;
		/* root_fi is just for logging, so no need to be extra careful about it. */
		if (!fsm_term_safely.root_fi)
			fsm_term_safely.root_fi = fi;
	}

	if (fsm_term_safely.depth > 1) {
		/* fsm_term_safely is enabled and this is a secondary FSM instance terminated, caused by the root_fi. */
		LOGPFSMSRC(fi, file, line, "Terminating in cascade, depth %d (cause = %s, caused by: %s)\n",
			   fsm_term_safely.depth, osmo_fsm_term_cause_name(cause),
			   fsm_term_safely.root_fi ? fsm_term_safely.root_fi->name : "unknown");
		/* The root_fi can't go missing really, but to be safe, log "unknown" in that case. */
	} else {
		/* fsm_term_safely is disabled, or this is the root_fi. */
		LOGPFSMSRC(fi, file, line, "Terminating (cause = %s)\n", osmo_fsm_term_cause_name(cause));
	}

	/* graceful exit (optional) */
	if (fi->fsm->pre_term)
		fi->fsm->pre_term(fi, cause);

	_osmo_fsm_inst_term_children(fi, OSMO_FSM_TERM_PARENT, NULL,
				     file, line);

	/* delete ourselves from the parent */
	parent = fi->proc.parent;
	if (parent) {
		LOGPFSMSRC(fi, file, line, "Removing from parent %s\n",
			   osmo_fsm_inst_name(parent));
		llist_del(&fi->proc.child);
	}

	/* call destructor / clean-up function */
	if (fi->fsm->cleanup)
		fi->fsm->cleanup(fi, cause);

	/* Fetch parent again in case it has changed. */
	parent = fi->proc.parent;

	/* Legacy behavior if fsm_term_safely is disabled: free before dispatching parent event. (If fsm_term_safely is
	 * enabled, depth will *always* be > 0 here.) Pivot on depth instead of the enabled flag in case the enabled
	 * flag is toggled in the middle of an FSM term. */
	if (!fsm_term_safely.depth) {
		LOGPFSMSRC(fi, file, line, "Freeing instance\n");
		osmo_fsm_inst_free(fi);
	}

	/* indicate our termination to the parent */
	if (parent && cause != OSMO_FSM_TERM_PARENT)
		_osmo_fsm_inst_dispatch(parent, parent_term_event, data,
					file, line);

	/* Newer, safe deallocation: free only after the parent_term_event was dispatched, to catch all termination
	 * cascades, and free all FSM instances at once. (If fsm_term_safely is enabled, depth will *always* be > 0
	 * here.) osmo_fsm_inst_free() will do the defer magic depending on the fsm_term_safely.depth. */
	if (fsm_term_safely.depth) {
		fsm_term_safely.depth--;
		osmo_fsm_inst_free(fi);
	}
}

/*! Terminate all child FSM instances of an FSM instance.
 *
 *  Iterate over all children and send them a termination event, with the given
 *  cause. Pass OSMO_FSM_TERM_PARENT to avoid dispatching events from the
 *  terminated child FSMs.
 *
 *  \param[in] fi FSM instance that should be cleared of child FSMs
 *  \param[in] cause Cause / reason for termination (OSMO_FSM_TERM_PARENT)
 *  \param[in] data Opaque event data to be passed with the parent term events
 *  \param[in] file Calling source file (from osmo_fsm_inst_term_children macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_term_children macro)
 */
void _osmo_fsm_inst_term_children(struct osmo_fsm_inst *fi,
				  enum osmo_fsm_term_cause cause,
				  void *data,
				  const char *file, int line)
{
	struct osmo_fsm_inst *first_child, *last_seen_first_child;

	/* iterate over all children, starting from the beginning every time:
	 * terminating an FSM may emit events that cause other FSMs to also
	 * terminate and remove themselves from this list. */
	last_seen_first_child = NULL;
	while (!llist_empty(&fi->proc.children)) {
		first_child = llist_entry(fi->proc.children.next,
					  typeof(*first_child),
					  proc.child);

		/* paranoia: do not loop forever */
		if (first_child == last_seen_first_child) {
			LOGPFSMLSRC(fi, LOGL_ERROR, file, line,
				    "Internal error while terminating child"
				    " FSMs: a child FSM is stuck\n");
			break;
		}
		last_seen_first_child = first_child;

		/* terminate child */
		_osmo_fsm_inst_term(first_child, cause, data,
				    file, line);
	}
}

/*! Broadcast an event to all the FSMs children.
 *
 *  Iterate over all children and send them the specified event.
 *
 *  \param[in] fi FSM instance of the parent
 *  \param[in] event Event to send to children of FSM instance
 *  \param[in] data Data to pass along with the event
 *  \param[in] file Calling source file (from osmo_fsm_inst_dispatch macro)
 *  \param[in] line Calling source line (from osmo_fsm_inst_dispatch macro)
 */
void _osmo_fsm_inst_broadcast_children(struct osmo_fsm_inst *fi,
					uint32_t event, void *data,
					const char *file, int line)
{
	struct osmo_fsm_inst *child, *tmp;
	llist_for_each_entry_safe(child, tmp, &fi->proc.children, proc.child) {
		_osmo_fsm_inst_dispatch(child, event, data, file, line);
	}
}

const struct value_string osmo_fsm_term_cause_names[] = {
	OSMO_VALUE_STRING(OSMO_FSM_TERM_PARENT),
	OSMO_VALUE_STRING(OSMO_FSM_TERM_REQUEST),
	OSMO_VALUE_STRING(OSMO_FSM_TERM_REGULAR),
	OSMO_VALUE_STRING(OSMO_FSM_TERM_ERROR),
	OSMO_VALUE_STRING(OSMO_FSM_TERM_TIMEOUT),
	{ 0, NULL }
};

/*! @} */
