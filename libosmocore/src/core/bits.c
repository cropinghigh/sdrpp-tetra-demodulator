/*
 * (C) 2011 by Harald Welte <laforge@gnumonks.org>
 * (C) 2011 by Sylvain Munaut <tnt@246tNt.com>
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

#include <stdint.h>

#include <osmocom/core/bits.h>

/*! \addtogroup bits
 *  @{
 *  Osmocom bit level support code.
 *
 *  This module implements the notion of different bit-fields, such as
 *  - unpacked bits (\ref ubit_t), i.e. 1 bit per byte
 *  - packed bits (\ref pbit_t), i.e. 8 bits per byte
 *  - soft bits (\ref sbit_t), 1 bit per byte from -127 to 127
 *
 * \file bits.c */

/*! convert unpacked bits to packed bits, return length in bytes
 *  \param[out] out output buffer of packed bits
 *  \param[in] in input buffer of unpacked bits
 *  \param[in] num_bits number of bits
 */
int osmo_ubit2pbit(pbit_t *out, const ubit_t *in, unsigned int num_bits)
{
	unsigned int i;
	uint8_t curbyte = 0;
	pbit_t *outptr = out;

	for (i = 0; i < num_bits; i++) {
		uint8_t bitnum = 7 - (i % 8);

		curbyte |= (in[i] << bitnum);

		if(i % 8 == 7){
			*outptr++ = curbyte;
			curbyte = 0;
		}
	}
	/* we have a non-modulo-8 bitcount */
	if (i % 8)
		*outptr++ = curbyte;

	return outptr - out;
}

/*! Shift unaligned input to octet-aligned output
 *  \param[out] out output buffer, unaligned
 *  \param[in] in input buffer, octet-aligned
 *  \param[in] num_nibbles number of nibbles
 */
void osmo_nibble_shift_right(uint8_t *out, const uint8_t *in,
			     unsigned int num_nibbles)
{
	unsigned int i, num_whole_bytes = num_nibbles / 2;
	if (!num_whole_bytes)
		return;

	/* first byte: upper nibble empty, lower nibble from src */
	out[0] = (in[0] >> 4);

	/* bytes 1.. */
	for (i = 1; i < num_whole_bytes; i++)
		out[i] = ((in[i - 1] & 0xF) << 4) | (in[i] >> 4);

	/* shift the last nibble, in case there's an odd count */
	i = num_whole_bytes;
	if (num_nibbles & 1)
		out[i] = ((in[i - 1] & 0xF) << 4) | (in[i] >> 4);
	else
		out[i] = (in[i - 1] & 0xF) << 4;
}

/*! Shift unaligned input to octet-aligned output
 *  \param[out] out output buffer, octet-aligned
 *  \param[in] in input buffer, unaligned
 *  \param[in] num_nibbles number of nibbles
 */
void osmo_nibble_shift_left_unal(uint8_t *out, const uint8_t *in,
				unsigned int num_nibbles)
{
	unsigned int i, num_whole_bytes = num_nibbles / 2;
	if (!num_whole_bytes)
		return;

	for (i = 0; i < num_whole_bytes; i++)
		out[i] = ((in[i] & 0xF) << 4) | (in[i + 1] >> 4);

	/* shift the last nibble, in case there's an odd count */
	i = num_whole_bytes;
	if (num_nibbles & 1)
		out[i] = (in[i] & 0xF) << 4;
}

/*! convert unpacked bits to soft bits
 *  \param[out] out output buffer of soft bits
 *  \param[in] in input buffer of unpacked bits
 *  \param[in] num_bits number of bits
 */
void osmo_ubit2sbit(sbit_t *out, const ubit_t *in, unsigned int num_bits)
{
	unsigned int i;
	for (i = 0; i < num_bits; i++)
		out[i] = in[i] ? -127 : 127;
}

/*! convert soft bits to unpacked bits
 *  \param[out] out output buffer of unpacked bits
 *  \param[in] in input buffer of soft bits
 *  \param[in] num_bits number of bits
 */
void osmo_sbit2ubit(ubit_t *out, const sbit_t *in, unsigned int num_bits)
{
	unsigned int i;
	for (i = 0; i < num_bits; i++)
		out[i] = in[i] < 0;
}

/*! convert packed bits to unpacked bits, return length in bytes
 *  \param[out] out output buffer of unpacked bits
 *  \param[in] in input buffer of packed bits
 *  \param[in] num_bits number of bits
 *  \return number of bytes used in \ref out
 */
int osmo_pbit2ubit(ubit_t *out, const pbit_t *in, unsigned int num_bits)
{
	unsigned int i;
	ubit_t *cur = out;
	ubit_t *limit = out + num_bits;

	for (i = 0; i < (num_bits/8)+1; i++) {
		pbit_t byte = in[i];
		*cur++ = (byte >> 7) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 6) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 5) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 4) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 3) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 2) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 1) & 1;
		if (cur >= limit)
			break;
		*cur++ = (byte >> 0) & 1;
		if (cur >= limit)
			break;
	}
	return cur - out;
}

/*! convert unpacked bits to packed bits (extended options)
 *  \param[out] out output buffer of packed bits
 *  \param[in] out_ofs offset into output buffer
 *  \param[in] in input buffer of unpacked bits
 *  \param[in] in_ofs offset into input buffer
 *  \param[in] num_bits number of bits
 *  \param[in] lsb_mode Encode bits in LSB order instead of MSB
 *  \returns length in bytes (max written offset of output buffer + 1)
 */
int osmo_ubit2pbit_ext(pbit_t *out, unsigned int out_ofs,
                       const ubit_t *in, unsigned int in_ofs,
                       unsigned int num_bits, int lsb_mode)
{
	unsigned int i, op, bn;
	for (i=0; i<num_bits; i++) {
		op = out_ofs + i;
		bn = lsb_mode ? (op&7) : (7-(op&7));
		if (in[in_ofs+i])
			out[op>>3] |= 1 << bn;
		else
			out[op>>3] &= ~(1 << bn);
	}
	return ((out_ofs + num_bits - 1) >> 3) + 1;
}

/*! convert packed bits to unpacked bits (extended options)
 *  \param[out] out output buffer of unpacked bits
 *  \param[in] out_ofs offset into output buffer
 *  \param[in] in input buffer of packed bits
 *  \param[in] in_ofs offset into input buffer
 *  \param[in] num_bits number of bits
 *  \param[in] lsb_mode Encode bits in LSB order instead of MSB
 *  \returns length in bytes (max written offset of output buffer + 1)
 */
int osmo_pbit2ubit_ext(ubit_t *out, unsigned int out_ofs,
                       const pbit_t *in, unsigned int in_ofs,
                       unsigned int num_bits, int lsb_mode)
{
	unsigned int i, ip, bn;
	for (i=0; i<num_bits; i++) {
		ip = in_ofs + i;
		bn = lsb_mode ? (ip&7) : (7-(ip&7));
		out[out_ofs+i] = !!(in[ip>>3] & (1<<bn));
	}
	return out_ofs + num_bits;
}

/* look-up table for bit-reversal within a byte. Generated using:
	int i,k;
        for (i = 0 ; i < 256 ; i++) {
                uint8_t sample = 0 ;
                for (k = 0; k<8; k++) {
                        if ( i & 1 << k ) sample |= 0x80 >>  k;
                }
                flip_table[i] = sample;
        }
 */
static const uint8_t flip_table[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff,
};

/*! generalized bit reversal function
 *  \param[in] x the 32bit value to be reversed
 *  \param[in] k the type of reversal requested
 *  \returns the reversed 32bit dword
 *
 * This function reverses the bit order within a 32bit word. Depending
 * on "k", it either reverses all bits in a 32bit dword, or the bytes in
 * the dword, or the bits in each byte of a dword, or simply swaps the
 * two 16bit words in a dword.  See Chapter 7 "Hackers Delight"
 */
uint32_t osmo_bit_reversal(uint32_t x, enum osmo_br_mode k)
{
	if (k &  1) x = (x & 0x55555555) <<  1 | (x & 0xAAAAAAAA) >>  1;
	if (k &  2) x = (x & 0x33333333) <<  2 | (x & 0xCCCCCCCC) >>  2;
	if (k &  4) x = (x & 0x0F0F0F0F) <<  4 | (x & 0xF0F0F0F0) >>  4;
	if (k &  8) x = (x & 0x00FF00FF) <<  8 | (x & 0xFF00FF00) >>  8;
	if (k & 16) x = (x & 0x0000FFFF) << 16 | (x & 0xFFFF0000) >> 16;

	return x;
}

/*! reverse the bit-order in each byte of a dword
 *  \param[in] x 32bit input value
 *  \returns 32bit value where bits of each byte have been reversed
 *
 * See Chapter 7 "Hackers Delight"
 */
uint32_t osmo_revbytebits_32(uint32_t x)
{
	x = (x & 0x55555555) <<  1 | (x & 0xAAAAAAAA) >>  1;
	x = (x & 0x33333333) <<  2 | (x & 0xCCCCCCCC) >>  2;
	x = (x & 0x0F0F0F0F) <<  4 | (x & 0xF0F0F0F0) >>  4;

	return x;
}

/*! reverse the bit order in a byte
 *  \param[in] x 8bit input value
 *  \returns 8bit value where bits order has been reversed
 */
uint32_t osmo_revbytebits_8(uint8_t x)
{
	return flip_table[x];
}

/*! reverse bit-order of each byte in a buffer
 *  \param[in] buf buffer containing bytes to be bit-reversed
 *  \param[in] len length of buffer in bytes
 *
 *  This function reverses the bits in each byte of the buffer
 */
void osmo_revbytebits_buf(uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < len; i++)
		buf[i] = flip_table[buf[i]];
}

/*! @} */
