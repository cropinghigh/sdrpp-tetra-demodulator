#pragma once
#include <stdint.h>
#include <osmocom/core/bits.h>

/*! \brief definition of a PRBS sequence */
struct osmo_prbs {
	const char *name;	/*!< human-readable name */
	unsigned int len;	/*!< length in bits */
	uint64_t coeff;		/*!< coefficients */
};

/*! \brief state of a given PRBS sequence generator */
struct osmo_prbs_state {
	const struct osmo_prbs *prbs;
	uint64_t state;
};

extern const struct osmo_prbs osmo_prbs7;
extern const struct osmo_prbs osmo_prbs9;
extern const struct osmo_prbs osmo_prbs11;
extern const struct osmo_prbs osmo_prbs15;

void osmo_prbs_state_init(struct osmo_prbs_state *st, const struct osmo_prbs *prbs);
ubit_t osmo_prbs_get_ubit(struct osmo_prbs_state *state);
int osmo_prbs_get_ubits(ubit_t *out, unsigned int out_len, struct osmo_prbs_state *state);
