#pragma once

/*! \defgroup rate_ctr Rate counters
 *  @{
 * \file rate_ctr.h */

#include <stdint.h>

#include <osmocom/core/linuxlist.h>

/*! Number of rate counter intervals */
#define RATE_CTR_INTV_NUM	4

/*! Rate counter interval */
enum rate_ctr_intv {
	RATE_CTR_INTV_SEC,	/*!< last second */
	RATE_CTR_INTV_MIN,	/*!< last minute */
	RATE_CTR_INTV_HOUR,	/*!< last hour */
	RATE_CTR_INTV_DAY,	/*!< last day */
};

/*! data we keep for each of the intervals */
struct rate_ctr_per_intv {
	uint64_t last;		/*!< counter value in last interval */
	uint64_t rate;		/*!< counter rate */
};

/*! data we keep for each actual value */
struct rate_ctr {
	uint64_t current;	/*!< current value */
	uint64_t previous;	/*!< previous value, used for delta */
	/*! per-interval data */
	struct rate_ctr_per_intv intv[RATE_CTR_INTV_NUM];
};

/*! rate counter description */
struct rate_ctr_desc {
	const char *name;	/*!< name of the counter */
	const char *description;/*!< description of the counter */
};

/*! description of a rate counter group */
struct rate_ctr_group_desc {
	/*! The prefix to the name of all counters in this group */
	const char *group_name_prefix;
	/*! The human-readable description of the group */
	const char *group_description;
	/*! The class to which this group belongs */
	int class_id;
	/*! The number of counters in this group */
	unsigned int num_ctr;
	/*! Pointer to array of counter names */
	const struct rate_ctr_desc *ctr_desc;
};

/*! One instance of a counter group class */
struct rate_ctr_group {
	/*! Linked list of all counter groups in the system */
	struct llist_head list;
	/*! Pointer to the counter group class */
	const struct rate_ctr_group_desc *desc;
	/*! The index of this ctr_group within its class */
	unsigned int idx;
	/*! Optional string-based identifier to be used instead of index at report time */
	char *name;
	/*! Actual counter structures below. Don't access it directly, use APIs below! */
	struct rate_ctr ctr[0];
};

struct rate_ctr_group *rate_ctr_group_alloc(void *ctx,
					    const struct rate_ctr_group_desc *desc,
					    unsigned int idx);

static inline void rate_ctr_group_upd_idx(struct rate_ctr_group *grp, unsigned int idx)
{
	grp->idx = idx;
}
void rate_ctr_group_set_name(struct rate_ctr_group *grp, const char *name);

struct rate_ctr *rate_ctr_group_get_ctr(struct rate_ctr_group *grp, unsigned int idx);

void rate_ctr_group_free(struct rate_ctr_group *grp);

/*! Increment the counter by \a inc
 *  \param ctr \ref rate_ctr to increment
 *  \param inc quantity to increment \a ctr by */
void rate_ctr_add(struct rate_ctr *ctr, int inc);

/*! Increment the counter by \a inc
 *  \param ctrg \ref rate_ctr_group of counter
 *  \param idx index into \a ctrg counter group
 *  \param inc quantity to increment \a ctr by */
static inline void rate_ctr_add2(struct rate_ctr_group *ctrg, unsigned int idx, int inc)
{
	rate_ctr_add(rate_ctr_group_get_ctr(ctrg, idx), inc);
}

/*! Increment the counter by 1
 *  \param ctr \ref rate_ctr to increment */
static inline void rate_ctr_inc(struct rate_ctr *ctr)
{
	rate_ctr_add(ctr, 1);
}

/*! Increment the counter by 1
 *  \param ctrg \ref rate_ctr_group of counter
 *  \param idx index into \a ctrg counter group */
static inline void rate_ctr_inc2(struct rate_ctr_group *ctrg, unsigned int idx)
{
	rate_ctr_inc(rate_ctr_group_get_ctr(ctrg, idx));
}


/*! Return the counter difference since the last call to this function */
int64_t rate_ctr_difference(struct rate_ctr *ctr);

int rate_ctr_init(void *tall_ctx);

struct rate_ctr_group *rate_ctr_get_group_by_name_idx(const char *name, const unsigned int idx);
const struct rate_ctr *rate_ctr_get_by_name(const struct rate_ctr_group *ctrg, const char *name);

typedef int (*rate_ctr_handler_t)(
	struct rate_ctr_group *, struct rate_ctr *,
	const struct rate_ctr_desc *, void *);
typedef int (*rate_ctr_group_handler_t)(struct rate_ctr_group *, void *);


int rate_ctr_for_each_counter(struct rate_ctr_group *ctrg,
	rate_ctr_handler_t handle_counter, void *data);

int rate_ctr_for_each_group(rate_ctr_group_handler_t handle_group, void *data);

void rate_ctr_reset(struct rate_ctr *ctr);
void rate_ctr_group_reset(struct rate_ctr_group *ctrg);

/*! @} */
