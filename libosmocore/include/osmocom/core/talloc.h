/*! \file talloc.h */
#pragma once
#include <talloc.h>

/*! per-thread talloc contexts.  This works around the problem that talloc is not
 * thread-safe. However, one can simply have a different set of talloc contexts for each
 * thread, and ensure that allocations made on one thread are always only free'd on that
 * very same thread.
 * WARNING: Users must make sure they free() on the same thread as they allocate!! */
struct osmo_talloc_contexts {
	/*! global per-thread talloc context. */
	void *global;
	/*! volatile select-dispatch context.  This context is completely free'd and
	 * re-created every time the main select loop in osmo_select_main() returns from
	 * select(2) and calls per-fd callback functions.  This allows users of this
	 * facility to allocate temporary objects like string buffers, message buffers
	 * and the like which are automatically free'd when going into the next select()
	 * system call */
	void *select;
};

extern __thread struct osmo_talloc_contexts *osmo_ctx;

/* short-hand #defines for the osmo talloc contexts (OTC) that can be used to pass
 * to the various _c functions like msgb_alloc_c() */
#define OTC_GLOBAL (osmo_ctx->global)
#define OTC_SELECT (osmo_ctx->select)

int osmo_ctx_init(const char *id);
