/*
 * isdnhdlc.h  --  General purpose ISDN HDLC decoder.
 *
 * Implementation of a HDLC decoder/encoder in software.
 * Necessary because some ISDN devices don't have HDLC
 * controllers.
 *
 * Copyright (C)
 *	2009	Karsten Keil		<keil@b1-systems.de>
 *	2002	Wolfgang Mües		<wolfgang@iksw-muees.de>
 *	2001	Frode Isaksen		<fisaksen@bewan.com>
 *	2001	Kai Germaschewski	<kai.germaschewski@gmx.de>
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

struct osmo_isdnhdlc_vars {
	int bit_shift;
	int hdlc_bits1;
	int data_bits;
	int ffbit_shift;	/* encoding only */
	int state;
	int dstpos;

	uint16_t crc;

	uint8_t cbin;
	uint8_t shift_reg;
	uint8_t ffvalue;

	/* set if transferring data */
	uint32_t data_received:1;
	/* set if D channel (send idle instead of flags) */
	uint32_t dchannel:1;
	/* set if 56K adaptation */
	uint32_t do_adapt56:1;
	/* set if in closing phase (need to send CRC + flag) */
	uint32_t do_closing:1;
	/* set if data is bitreverse */
	uint32_t do_bitreverse:1;
};

/* Feature Flags */
#define OSMO_HDLC_F_56KBIT	0x01
#define OSMO_HDLC_F_DCHANNEL	0x02
#define OSMO_HDLC_F_BITREVERSE	0x04

/*
  The return value from isdnhdlc_decode is
  the frame length, 0 if no complete frame was decoded,
  or a negative error number
*/
#define OSMO_HDLC_FRAMING_ERROR     1
#define OSMO_HDLC_CRC_ERROR         2
#define OSMO_HDLC_LENGTH_ERROR      3

extern void	osmo_isdnhdlc_rcv_init(struct osmo_isdnhdlc_vars *hdlc, uint32_t features);

extern int	osmo_isdnhdlc_decode(struct osmo_isdnhdlc_vars *hdlc, const uint8_t *src,
				     int slen, int *count, uint8_t *dst, int dsize);

extern void	osmo_isdnhdlc_out_init(struct osmo_isdnhdlc_vars *hdlc, uint32_t features);

extern int	osmo_isdnhdlc_encode(struct osmo_isdnhdlc_vars *hdlc, const uint8_t *src,
				     uint16_t slen, int *count, uint8_t *dst, int dsize);
