/* Osmocom implementation of pseudo-random bit sequence generation */
/* (C) 2017 by Harald Welte <laforge@gnumonks.org> 
 * All Rights Reserved
 *
 * SPDX-License-Identifier: GPL-2.0+
 * */

#include <stdint.h>
#include <string.h>
#include <osmocom/core/bits.h>
#include <osmocom/core/prbs.h>

/*! \brief PRBS-7 according ITU-T O.150 */
const struct osmo_prbs osmo_prbs7 = {
	/* x^7 + x^6 + 1 */
	.name = "PRBS-7",
	.len = 7,
	.coeff = (1<<6) | (1<<5),
};

/*! \brief PRBS-9 according ITU-T O.150 */
const struct osmo_prbs osmo_prbs9 = {
	/* x^9 + x^5 + 1 */
	.name = "PRBS-9",
	.len = 9,
	.coeff = (1<<8) | (1<<4),
};

/*! \brief PRBS-11 according ITU-T O.150 */
const struct osmo_prbs osmo_prbs11 = {
	/* x^11 + x^9 + 1 */
	.name = "PRBS-11",
	.len = 11,
	.coeff = (1<<10) | (1<<8),
};

/*! \brief PRBS-15 according ITU-T O.150 */
const struct osmo_prbs osmo_prbs15 = {
	/* x^15 + x^14+ 1 */
	.name = "PRBS-15",
	.len = 15,
	.coeff = (1<<14) | (1<<13),
};

/*! \brief Initialize the given caller-allocated PRBS state */
void osmo_prbs_state_init(struct osmo_prbs_state *st, const struct osmo_prbs *prbs)
{
	memset(st, 0, sizeof(*st));
	st->prbs = prbs;
	st->state = 1;
}

static void osmo_prbs_process_bit(struct osmo_prbs_state *state, ubit_t bit)
{
	state->state >>= 1;
	if (bit)
		state->state ^= state->prbs->coeff;
}

/*! \brief Get the next bit out of given PRBS instance */
ubit_t osmo_prbs_get_ubit(struct osmo_prbs_state *state)
{
	ubit_t result = state->state & 0x1;
	osmo_prbs_process_bit(state, result);

	return result;
}

/*! \brief Fill buffer of unpacked bits with next bits out of given PRBS instance */
int osmo_prbs_get_ubits(ubit_t *out, unsigned int out_len, struct osmo_prbs_state *state)
{
	unsigned int i;

	for (i = 0; i < out_len; i++)
		out[i] = osmo_prbs_get_ubit(state);

	return i;
}
