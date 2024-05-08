/*! \file soft_uart.c
 *  Software UART implementation. */
/*
 * (C) 2022 by Harald Welte <laforge@gnumonks.org>
 * (C) 2023 by sysmocom - s.f.m.c. GmbH <info@sysmocom.de>
 *
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

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/timer.h>
#include <osmocom/core/soft_uart.h>

/*! Rx/Tx flow state of a soft-UART */
enum suart_flow_state {
	SUART_FLOW_ST_IDLE,	/*!< waiting for a start bit or Tx data */
	SUART_FLOW_ST_DATA,	/*!< receiving/transmitting data bits */
	SUART_FLOW_ST_PARITY,	/*!< receiving/transmitting parity bits */
	SUART_FLOW_ST_STOP,	/*!< receiving/transmitting stop bits */
};

/*! Internal state of a soft-UART */
struct osmo_soft_uart {
	struct osmo_soft_uart_cfg cfg;
	const char *name;
	/* modem status (bitmask of OSMO_SUART_STATUS_F_*) */
	unsigned int status;
	struct {
		bool running;
		uint8_t bit_count;
		uint8_t shift_reg;
		struct msgb *msg;
		ubit_t parity_bit; /* 0 (even) / 1 (odd) */
		unsigned int flags;
		struct osmo_timer_list timer;
		enum suart_flow_state flow_state;
	} rx;
	struct {
		bool running;
		uint8_t bit_count;
		uint8_t shift_reg;
		ubit_t parity_bit; /* 0 (even) / 1 (odd) */
		enum suart_flow_state flow_state;
	} tx;
};

/*! Default soft-UART configuration (8-N-1) */
const struct osmo_soft_uart_cfg osmo_soft_uart_default_cfg = {
	.num_data_bits = 8,
	.num_stop_bits = 1,
	.parity_mode = OSMO_SUART_PARITY_NONE,
	.rx_buf_size = 1024,
	.rx_timeout_ms = 100,
	.flow_ctrl_mode = OSMO_SUART_FLOW_CTRL_NONE,
};

/*************************************************************************
 * Receiver
 *************************************************************************/

/*! Flush the receive buffer, passing ownership of the msgb to the .rx_cb().
 * \param[in] suart soft-UART instance holding the receive buffer. */
void osmo_soft_uart_flush_rx(struct osmo_soft_uart *suart)
{
	if (suart->rx.msg && msgb_length(suart->rx.msg)) {
		osmo_timer_del(&suart->rx.timer);
		if (suart->cfg.rx_cb) {
			suart->cfg.rx_cb(suart->cfg.priv, suart->rx.msg, suart->rx.flags);
			/* call-back has taken ownership of msgb, no need to free() here */
			suart->rx.msg = msgb_alloc_c(suart, suart->cfg.rx_buf_size, "soft_uart_rx");
		} else {
			msgb_reset(suart->rx.msg);
		}
	}
}

/* one character was received; add to receive buffer and notify user, if needed */
static void suart_rx_ch(struct osmo_soft_uart *suart, uint8_t ch)
{
	unsigned int msg_len;

	OSMO_ASSERT(suart->rx.msg);
	msgb_put_u8(suart->rx.msg, ch);
	msg_len = msgb_length(suart->rx.msg);

	if (msg_len >= suart->cfg.rx_buf_size || suart->rx.flags) {
		/* either the buffer is full, or we hit a parity and/or a framing error */
		osmo_soft_uart_flush_rx(suart);
	} else if (msg_len == 1) {
		/* first character in new message: start timer */
		osmo_timer_schedule(&suart->rx.timer, suart->cfg.rx_timeout_ms / 1000,
				    (suart->cfg.rx_timeout_ms % 1000) * 1000);
	}
}

/* receive a single bit */
static inline void suart_rx_bit(struct osmo_soft_uart *suart, const ubit_t bit)
{
	switch (suart->rx.flow_state) {
	case SUART_FLOW_ST_IDLE:
		if (bit == 0) { /* start bit condition */
			suart->rx.flow_state = SUART_FLOW_ST_DATA;
			suart->rx.flags = 0x00;
			suart->rx.shift_reg = 0;
			suart->rx.bit_count = 0;
			suart->rx.parity_bit = 0;
		}
		break;
	case SUART_FLOW_ST_DATA:
		suart->rx.bit_count++;
		suart->rx.shift_reg >>= 1;
		if (bit != 0) {
			suart->rx.parity_bit = !suart->rx.parity_bit; /* flip */
			suart->rx.shift_reg |= 0x80;
		}
		if (suart->rx.bit_count >= suart->cfg.num_data_bits) {
			/* we have accumulated enough data bits */
			if (suart->cfg.parity_mode != OSMO_SUART_PARITY_NONE)
				suart->rx.flow_state = SUART_FLOW_ST_PARITY;
			else
				suart->rx.flow_state = SUART_FLOW_ST_STOP;
			/* align the register if needed */
			if (suart->cfg.num_data_bits < 8)
				suart->rx.shift_reg >>= (8 - suart->cfg.num_data_bits);
		}
		break;
	case SUART_FLOW_ST_PARITY:
		switch (suart->cfg.parity_mode) {
		case OSMO_SUART_PARITY_EVEN:
			/* number of 1-bits (in both data and parity) shall be even */
			if (suart->rx.parity_bit != bit)
				suart->rx.flags |= OSMO_SUART_F_PARITY_ERROR;
			break;
		case OSMO_SUART_PARITY_ODD:
			/* number of 1-bits (in both data and parity) shall be odd */
			if (suart->rx.parity_bit == bit)
				suart->rx.flags |= OSMO_SUART_F_PARITY_ERROR;
			break;
		case OSMO_SUART_PARITY_MARK:
			/* parity bit must always be 1 */
			if (bit != 1)
				suart->rx.flags |= OSMO_SUART_F_PARITY_ERROR;
			break;
		case OSMO_SUART_PARITY_SPACE:
			/* parity bit must always be 0 */
			if (bit != 0)
				suart->rx.flags |= OSMO_SUART_F_PARITY_ERROR;
			break;
		case OSMO_SUART_PARITY_NONE: /* shall not happen */
		default:
			OSMO_ASSERT(0);
		}

		suart->rx.flow_state = SUART_FLOW_ST_STOP;
		break;
	case SUART_FLOW_ST_STOP:
		suart->rx.bit_count++;
		if (bit != 1)
			suart->rx.flags |= OSMO_SUART_F_FRAMING_ERROR;

		if (suart->rx.bit_count >= (suart->cfg.num_data_bits + suart->cfg.num_stop_bits)) {
			/* we have accumulated enough stop bits */
			suart_rx_ch(suart, suart->rx.shift_reg);
			suart->rx.flow_state = SUART_FLOW_ST_IDLE;
		}
		break;
	}
}

/* receive timer expiration: flush rx-buffer to user call-back */
static void suart_rx_timer_cb(void *data)
{
	struct osmo_soft_uart *suart = data;
	osmo_soft_uart_flush_rx(suart);
}

/*! Feed a number of unpacked bits into the soft-UART receiver.
 * \param[in] suart soft-UART instance to feed bits into.
 * \param[in] ubits pointer to the unpacked bits.
 * \param[in] n_ubits number of unpacked bits to be fed.
 * \returns 0 on success; negative on error.
 *          -EAGAIN indicates that the receiver is disabled. */
int osmo_soft_uart_rx_ubits(struct osmo_soft_uart *suart, const ubit_t *ubits, size_t n_ubits)
{
	if (!suart->rx.running)
		return -EAGAIN;
	for (size_t i = 0; i < n_ubits; i++)
		suart_rx_bit(suart, ubits[i]);
	return 0;
}

/*************************************************************************
 * Transmitter
 *************************************************************************/

/* pull a single bit out of the UART transmitter */
static inline ubit_t suart_tx_bit(struct osmo_soft_uart *suart, struct msgb *msg)
{
	ubit_t tx_bit = 1;

	switch (suart->tx.flow_state) {
	case SUART_FLOW_ST_IDLE:
		if (msg && msgb_length(msg) > 0) { /* if we have pending data */
			suart->tx.shift_reg = msgb_pull_u8(msg);
			suart->tx.flow_state = SUART_FLOW_ST_DATA;
			suart->tx.bit_count = 0;
			suart->tx.parity_bit = 0;
			tx_bit = 0;
		}
		break;
	case SUART_FLOW_ST_DATA:
		tx_bit = suart->tx.shift_reg & 1;
		suart->tx.parity_bit ^= tx_bit;
		suart->tx.shift_reg >>= 1;
		suart->tx.bit_count++;
		if (suart->tx.bit_count >= suart->cfg.num_data_bits) {
			/* we have transmitted all data bits */
			if (suart->cfg.parity_mode != OSMO_SUART_PARITY_NONE)
				suart->tx.flow_state = SUART_FLOW_ST_PARITY;
			else
				suart->tx.flow_state = SUART_FLOW_ST_STOP;
		}
		break;
	case SUART_FLOW_ST_PARITY:
		switch (suart->cfg.parity_mode) {
		case OSMO_SUART_PARITY_EVEN:
			/* number of 1-bits (in both data and parity) shall be even */
			tx_bit = suart->tx.parity_bit;
			break;
		case OSMO_SUART_PARITY_ODD:
			/* number of 1-bits (in both data and parity) shall be odd */
			tx_bit = !suart->tx.parity_bit;
			break;
		case OSMO_SUART_PARITY_MARK:
			/* parity bit must always be 1 */
			tx_bit = 1;
			break;
		case OSMO_SUART_PARITY_SPACE:
			/* parity bit must always be 0 */
			tx_bit = 0;
			break;
		case OSMO_SUART_PARITY_NONE:
		default: /* shall not happen */
			OSMO_ASSERT(0);
		}

		suart->tx.flow_state = SUART_FLOW_ST_STOP;
		break;
	case SUART_FLOW_ST_STOP:
		suart->tx.bit_count++;
		if (suart->tx.bit_count >= (suart->cfg.num_data_bits + suart->cfg.num_stop_bits)) {
			/* we have transmitted all stop bits, we're done */
			suart->tx.flow_state = SUART_FLOW_ST_IDLE;
		}
		break;
	}

	return tx_bit;
}

/* pull pending bits out of the UART */
static size_t suart_tx_pending(struct osmo_soft_uart *suart, ubit_t *ubits, size_t n_ubits)
{
	size_t i;

	for (i = 0; i < n_ubits; i++) {
		if (suart->tx.flow_state == SUART_FLOW_ST_IDLE)
			break;
		ubits[i] = suart_tx_bit(suart, NULL);
	}

	return i;
}

/*! Pull a number of unpacked bits out of the soft-UART transmitter.
 * \param[in] suart soft-UART instance to pull the bits from.
 * \param[out] ubits pointer to a buffer where to store pulled bits.
 * \param[in] n_ubits number of unpacked bits to be pulled.
 * \returns number of bits pulled (may be less than n_ubits); negative on error.
 *          -EAGAIN indicates that the transmitter is disabled. */
int osmo_soft_uart_tx_ubits(struct osmo_soft_uart *suart, ubit_t *ubits, size_t n_ubits)
{
	const struct osmo_soft_uart_cfg *cfg = &suart->cfg;
	size_t n_frame_bits, n_chars;
	struct msgb *msg = NULL;

	if (OSMO_UNLIKELY(n_ubits == 0))
		return -EINVAL;

	if (!suart->tx.running)
		return -EAGAIN;

	switch (suart->cfg.flow_ctrl_mode) {
	case OSMO_SUART_FLOW_CTRL_DTR_DSR:
		/* if DSR is de-asserted, Tx pending bits and suspend */
		if (~suart->status & OSMO_SUART_STATUS_F_DSR)
			return suart_tx_pending(suart, ubits, n_ubits);
		/* else: keep transmitting as usual */
		break;
	case OSMO_SUART_FLOW_CTRL_RTS_CTS:
		/* if CTS is de-asserted, Tx pending bits and suspend */
		if (~suart->status & OSMO_SUART_STATUS_F_CTS)
			return suart_tx_pending(suart, ubits, n_ubits);
		/* else: keep transmitting as usual */
		break;
	case OSMO_SUART_FLOW_CTRL_NONE:
	default:
		break;
	}

	/* calculate UART frame size for the effective config */
	n_frame_bits = 1 + cfg->num_data_bits + cfg->num_stop_bits;
	if (cfg->parity_mode != OSMO_SUART_PARITY_NONE)
		n_frame_bits += 1;

	/* calculate the number of characters we can fit into n_ubits */
	n_chars = n_ubits / n_frame_bits;
	if (n_chars == 0) {
		/* we can transmit at least one character */
		if (suart->tx.flow_state == SUART_FLOW_ST_IDLE)
			n_chars = 1;
	}

	if (n_chars > 0) {
		/* allocate a Tx buffer msgb */
		msg = msgb_alloc_c(suart, n_chars, "soft_uart_tx");
		OSMO_ASSERT(msg != NULL);

		/* call the .tx_cb() to populate the Tx buffer */
		OSMO_ASSERT(cfg->tx_cb != NULL);
		suart->cfg.tx_cb(cfg->priv, msg);
	}

	for (size_t i = 0; i < n_ubits; i++)
		ubits[i] = suart_tx_bit(suart, msg);
	msgb_free(msg);

	return n_ubits;
}

/*! Get the modem status bitmask of the given soft-UART.
 * \param[in] suart soft-UART instance to get the modem status.
 * \returns bitmask of OSMO_SUART_STATUS_F_*. */
unsigned int osmo_soft_uart_get_status(const struct osmo_soft_uart *suart)
{
	return suart->status;
}

/*! Set the modem status bitmask of the given soft-UART.
 * \param[in] suart soft-UART instance to set the modem status.
 * \param[in] status bitmask of OSMO_SUART_STATUS_F_*.
 * \returns 0 on success; negative on error. */
int osmo_soft_uart_set_status(struct osmo_soft_uart *suart, unsigned int status)
{
	const struct osmo_soft_uart_cfg *cfg = &suart->cfg;

	if (cfg->status_change_cb != NULL) {
		if (suart->status != status)
			cfg->status_change_cb(cfg->priv, status);
	}

	suart->status = status;
	return 0;
}

/*! Activate/deactivate a modem status line of the given soft-UART.
 * \param[in] suart soft-UART instance to update the modem status.
 * \param[in] line a modem status line, one of OSMO_SUART_STATUS_F_*.
 * \param[in] active activate (true) or deactivate (false) the line. */
void osmo_soft_uart_set_status_line(struct osmo_soft_uart *suart,
				    enum osmo_soft_uart_status line,
				    bool active)
{
	unsigned int status = suart->status;

	if (active) /* assert the given line */
		status |= line;
	else /* de-assert the given line */
		status &= ~line;

	osmo_soft_uart_set_status(suart, status);
}


/*************************************************************************
 * Management / Initialization
 *************************************************************************/

/*! Allocate a soft-UART instance.
 * \param[in] ctx parent talloc context.
 * \param[in] name name of the soft-UART instance.
 * \param[in] cfg initial configuration of the soft-UART instance.
 * \returns pointer to allocated soft-UART instance; NULL on error. */
struct osmo_soft_uart *osmo_soft_uart_alloc(void *ctx, const char *name,
					    const struct osmo_soft_uart_cfg *cfg)
{
	struct osmo_soft_uart *suart = talloc_zero(ctx, struct osmo_soft_uart);
	if (!suart)
		return NULL;
	suart->name = talloc_strdup(suart, name);

	OSMO_ASSERT(cfg != NULL);
	suart->cfg = *cfg;

	return suart;
}

/*! Release memory taken by the given soft-UART.
 * \param[in] suart soft-UART instance to be free()d. */
void osmo_soft_uart_free(struct osmo_soft_uart *suart)
{
	if (suart == NULL)
		return;

	osmo_timer_del(&suart->rx.timer);
	msgb_free(suart->rx.msg);

	talloc_free((void *)suart->name);
	talloc_free(suart);
}

/*! Change soft-UART configuration to the user-provided config.
 * \param[in] suart soft-UART instance to be re-configured.
 * \param[in] cfg the user-provided config to be applied.
 * \returns 0 on success; negative on error. */
int osmo_soft_uart_configure(struct osmo_soft_uart *suart, const struct osmo_soft_uart_cfg *cfg)
{
	/* consistency checks on the configuration */
	if (cfg->num_data_bits > 8 || cfg->num_data_bits == 0)
		return -EINVAL;
	if (cfg->num_stop_bits == 0)
		return -EINVAL;
	if (cfg->parity_mode < 0 || cfg->parity_mode >= _OSMO_SUART_PARITY_NUM)
		return -EINVAL;
	if (cfg->rx_buf_size == 0)
		return -EINVAL;

	if (suart->cfg.rx_buf_size > cfg->rx_buf_size ||
	    suart->cfg.rx_timeout_ms > cfg->rx_timeout_ms) {
		osmo_soft_uart_flush_rx(suart);
	}

	suart->cfg = *cfg;

	osmo_timer_setup(&suart->rx.timer, suart_rx_timer_cb, suart);

	return 0;
}

/*! Get a name for the given soft-UART instance.
 * \param[in] suart soft-UART instance to get the name from.
 * \returns name of the given soft-UART instance. */
const char *osmo_soft_uart_get_name(const struct osmo_soft_uart *suart)
{
	return suart->name;
}

/*! Set a new name for the given soft-UART instance.
 * \param[in] suart soft-UART instance to set the name for.
 * \param[in] name the new name. */
void osmo_soft_uart_set_name(struct osmo_soft_uart *suart, const char *name)
{
	osmo_talloc_replace_string(suart, (char **)&suart->name, name);
}

/*! Enable/disable receiver of the given soft-UART.
 * \param[in] suart soft-UART instance to be re-configured.
 * \param[in] enable enable/disable state of the receiver.
 * \returns 0 on success; negative on error. */
int osmo_soft_uart_set_rx(struct osmo_soft_uart *suart, bool enable)
{
	if (!enable && suart->rx.running) {
		osmo_soft_uart_flush_rx(suart);
		suart->rx.running = false;
		suart->rx.flow_state = SUART_FLOW_ST_IDLE;
	} else if (enable && !suart->rx.running) {
		if (!suart->rx.msg)
			suart->rx.msg = msgb_alloc_c(suart, suart->cfg.rx_buf_size, "soft_uart_rx");
		suart->rx.running = true;
		suart->rx.flow_state = SUART_FLOW_ST_IDLE;
	}

	return 0;
}

/*! Enable/disable transmitter of the given soft-UART.
 * \param[in] suart soft-UART instance to be re-configured.
 * \param[in] enable enable/disable state of the transmitter.
 * \returns 0 on success; negative on error. */
int osmo_soft_uart_set_tx(struct osmo_soft_uart *suart, bool enable)
{
	if (!enable && suart->tx.running) {
		suart->tx.running = false;
		suart->tx.flow_state = SUART_FLOW_ST_IDLE;
	} else if (enable && !suart->tx.running) {
		suart->tx.running = true;
		suart->tx.flow_state = SUART_FLOW_ST_IDLE;
	}

	return 0;
}
