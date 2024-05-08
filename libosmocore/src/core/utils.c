/*
 * (C) 2011 by Harald Welte <laforge@gnumonks.org>
 * (C) 2011 by Sylvain Munaut <tnt@246tNt.com>
 * (C) 2014 by Nils O. Selåsdal <noselasd@fiane.dyndns.org>
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
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <inttypes.h>
#include <limits.h>

#include <osmocom/core/utils.h>
#include <osmocom/core/bit64gen.h>


/*! \addtogroup utils
 * @{
 * various utility routines
 *
 * \file utils.c */

static __thread char namebuf[255];
/* shared by osmo_str_tolower() and osmo_str_toupper() */
static __thread char capsbuf[128];

/*! get human-readable string for given value
 *  \param[in] vs Array of value_string tuples
 *  \param[in] val Value to be converted
 *  \returns pointer to human-readable string
 *
 * If val is found in vs, the array's string entry is returned. Otherwise, an
 * "unknown" string containing the actual value is composed in a static buffer
 * that is reused across invocations.
 */
const char *get_value_string(const struct value_string *vs, uint32_t val)
{
	const char *str = get_value_string_or_null(vs, val);
	if (str)
		return str;

	snprintf(namebuf, sizeof(namebuf), "unknown 0x%"PRIx32, val);
	namebuf[sizeof(namebuf) - 1] = '\0';
	return namebuf;
}

/*! get human-readable string or NULL for given value
 *  \param[in] vs Array of value_string tuples
 *  \param[in] val Value to be converted
 *  \returns pointer to human-readable string or NULL if val is not found
 */
const char *get_value_string_or_null(const struct value_string *vs,
				     uint32_t val)
{
	int i;

	if (!vs)
		return NULL;

	for (i = 0;; i++) {
		if (vs[i].value == 0 && vs[i].str == NULL)
			break;
		if (vs[i].value == val)
			return vs[i].str;
	}

	return NULL;
}

/*! get numeric value for given human-readable string
 *  \param[in] vs Array of value_string tuples
 *  \param[in] str human-readable string
 *  \returns numeric value (>0) or negative numer in case of error
 */
int get_string_value(const struct value_string *vs, const char *str)
{
	int i;

	for (i = 0;; i++) {
		if (vs[i].value == 0 && vs[i].str == NULL)
			break;
		if (!strcasecmp(vs[i].str, str))
			return vs[i].value;
	}
	return -EINVAL;
}

/*! Convert BCD-encoded digit into printable character
 *  \param[in] bcd A single BCD-encoded digit
 *  \returns single printable character
 */
char osmo_bcd2char(uint8_t bcd)
{
	if (bcd < 0xa)
		return '0' + bcd;
	else
		return 'A' + (bcd - 0xa);
}

/*! Convert number in ASCII to BCD value
 *  \param[in] c ASCII character
 *  \returns BCD encoded value of character
 */
uint8_t osmo_char2bcd(char c)
{
	if (c >= '0' && c <= '9')
		return c - 0x30;
	else if (c >= 'A' && c <= 'F')
		return 0xa + (c - 'A');
	else if (c >= 'a' && c <= 'f')
		return 0xa + (c - 'a');
	else
		return 0;
}

/*! Convert BCD to string.
 * The given nibble offsets are interpreted in BCD order, i.e. nibble 0 is bcd[0] & 0xf, nibble 1 is bcd[0] >> 4, nibble
 * 3 is bcd[1] & 0xf, etc..
 *  \param[out] dst  Output string buffer, is always nul terminated when dst_size > 0.
 *  \param[in] dst_size  sizeof() the output string buffer.
 *  \param[in] bcd  Binary coded data buffer.
 *  \param[in] start_nibble  Offset to start from, in nibbles, typically 1 to skip the first nibble.
 *  \param[in] end_nibble  Offset to stop before, in nibbles, e.g. sizeof(bcd)*2 - (bcd[0] & GSM_MI_ODD? 0:1).
 *  \param[in] allow_hex  If false, return error if there are digits other than 0-9. If true, return those as [A-F].
 *  \returns The strlen that would be written if the output buffer is large enough, excluding nul byte (like
 *           snprintf()), or -EINVAL if allow_hex is false and a digit > 9 is encountered. On -EINVAL, the conversion is
 *           still completed as if allow_hex were passed as true. Return -ENOMEM if dst is NULL or dst_size is zero.
 *           If end_nibble <= start_nibble, write an empty string to dst and return 0.
 */
int osmo_bcd2str(char *dst, size_t dst_size, const uint8_t *bcd, int start_nibble, int end_nibble, bool allow_hex)
{
	char *dst_end;
	int nibble_i;
	int rc = 0;

	if (!dst || dst_size < 1 || start_nibble < 0)
		return -ENOMEM;

	dst_end = dst + dst_size - 1;

	for (nibble_i = start_nibble; nibble_i < end_nibble && dst < dst_end; nibble_i++, dst++) {
		uint8_t nibble = bcd[nibble_i >> 1];
		if ((nibble_i & 1))
			nibble >>= 4;
		nibble &= 0xf;

		if (!allow_hex && nibble > 9)
			rc = -EINVAL;

		*dst = osmo_bcd2char(nibble);
	}
	*dst = '\0';

	if (rc < 0)
		return rc;
	return OSMO_MAX(0, end_nibble - start_nibble);
}

/*! Convert string to BCD.
 * The given nibble offsets are interpreted in BCD order, i.e. nibble 0 is bcd[0] & 0x0f, nibble 1 is bcd[0] & 0xf0, nibble
 * 3 is bcd[1] & 0x0f, etc..
 *  \param[out] dst  Output BCD buffer.
 *  \param[in] dst_size  sizeof() the output string buffer.
 *  \param[in] digits  String containing decimal or hexadecimal digits in upper or lower case.
 *  \param[in] start_nibble  Offset to start from, in nibbles, typically 1 to skip the first (MI type) nibble.
 *  \param[in] end_nibble  Negative to write all digits found in str, followed by 0xf nibbles to fill any started octet.
 *                         If >= 0, stop before this offset in nibbles, e.g. to get default behavior, pass
 *                         start_nibble + strlen(str) + ((start_nibble + strlen(str)) & 1? 1 : 0) + 1.
 *  \param[in] allow_hex  If false, return error if there are hexadecimal digits (A-F). If true, write those to
 *                        BCD.
 *  \returns The buffer size in octets that is used to place all bcd digits (including the skipped nibbles
 *           from 'start_nibble' and rounded up to full octets); -EINVAL on invalid digits;
 *           -ENOMEM if dst is NULL, if dst_size is too small to contain all nibbles, or if start_nibble is negative.
 */
int osmo_str2bcd(uint8_t *dst, size_t dst_size, const char *digits, int start_nibble, int end_nibble, bool allow_hex)
{
	const char *digit = digits;
	int nibble_i;

	if (!dst || !dst_size || start_nibble < 0)
		return -ENOMEM;

	if (end_nibble < 0) {
		end_nibble = start_nibble + strlen(digits);
		/* If the last octet is not complete, add another filler nibble */
		if (end_nibble & 1)
			end_nibble++;
	}
	if ((unsigned int) (end_nibble / 2) > dst_size)
		return -ENOMEM;

	for (nibble_i = start_nibble; nibble_i < end_nibble; nibble_i++) {
		uint8_t nibble = 0xf;
		int octet = nibble_i >> 1;
		if (*digit) {
			char c = *digit;
			digit++;
			if (c >= '0' && c <= '9')
				nibble = c - '0';
			else if (allow_hex && c >= 'A' && c <= 'F')
				nibble = 0xa + (c - 'A');
			else if (allow_hex && c >= 'a' && c <= 'f')
				nibble = 0xa + (c - 'a');
			else
				return -EINVAL;
		}
		nibble &= 0xf;
		if ((nibble_i & 1))
			dst[octet] = (nibble << 4) | (dst[octet] & 0x0f);
		else
			dst[octet] = (dst[octet] & 0xf0) | nibble;
	}

	/* floor(float(end_nibble) / 2) */
	return end_nibble / 2;
}

/*! Parse a string containing hexadecimal digits
 *  \param[in] str string containing ASCII encoded hexadecimal digits
 *  \param[out] b output buffer
 *  \param[in] max_len maximum space in output buffer
 *  \returns number of parsed octets, or -1 on error
 */
int osmo_hexparse(const char *str, uint8_t *b, unsigned int max_len)

{
	char c;
	uint8_t v;
	const char *strpos;
	unsigned int nibblepos = 0;

	memset(b, 0x00, max_len);

	for (strpos = str; (c = *strpos); strpos++) {
		/* skip whitespace */
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			continue;

		/* If the buffer is too small, error out */
		if (nibblepos >= (max_len << 1))
			return -1;

		if (c >= '0' && c <= '9')
			v = c - '0';
		else if (c >= 'a' && c <= 'f')
			v = 10 + (c - 'a');
		else if (c >= 'A' && c <= 'F')
			v = 10 + (c - 'A');
		else
			return -1;

		b[nibblepos >> 1] |= v << (nibblepos & 1 ? 0 : 4);
		nibblepos ++;
	}

	/* In case of uneven amount of digits, the last byte is not complete
	 * and that's an error. */
	if (nibblepos & 1)
		return -1;

	return nibblepos >> 1;
}

static __thread char hexd_buff[4096];
static const char hex_chars[] = "0123456789abcdef";

/*! Convert binary sequence to hexadecimal ASCII string.
 *  \param[out] out_buf  Output buffer to write the resulting string to.
 *  \param[in] out_buf_size  sizeof(out_buf).
 *  \param[in] buf  Input buffer, pointer to sequence of bytes.
 *  \param[in] len  Length of input buf in number of bytes.
 *  \param[in] delim  String to separate each byte; NULL or "" for no delim.
 *  \param[in] delim_after_last  If true, end the string in delim (true: "1a:ef:d9:", false: "1a:ef:d9");
 *                               if out_buf has insufficient space, the string will always end in a delim.
 *  \returns out_buf, containing a zero-terminated string, or "" (empty string) if out_buf == NULL or out_buf_size < 1.
 *
 * This function will print a sequence of bytes as hexadecimal numbers, adding one delim between each byte (e.g. for
 * delim passed as ":", return a string like "1a:ef:d9").
 *
 * The delim_after_last argument exists to be able to exactly show the original osmo_hexdump() behavior, which always
 * ends the string with a delimiter.
 */
const char *osmo_hexdump_buf(char *out_buf, size_t out_buf_size, const unsigned char *buf, int len, const char *delim,
			     bool delim_after_last)
{
	int i;
	char *cur = out_buf;
	size_t delim_len;

	if (!out_buf || !out_buf_size)
		return "";

	delim = delim ? : "";
	delim_len = strlen(delim);

	for (i = 0; i < len; i++) {
		const char *delimp = delim;
		int len_remain = out_buf_size - (cur - out_buf) - 1;
		if (len_remain < (int) (2 + delim_len)
		    && !(!delim_after_last && i == (len - 1) && len_remain >= 2))
			break;

		*cur++ = hex_chars[buf[i] >> 4];
		*cur++ = hex_chars[buf[i] & 0xf];

		if (i == (len - 1) && !delim_after_last)
			break;

		while (len_remain > 1 && *delimp) {
			*cur++ = *delimp++;
			len_remain--;
		}
	}
	*cur = '\0';
	return out_buf;
}

/*! Convert a sequence of unpacked bits to ASCII string, in user-supplied buffer.
 * \param[out] buf caller-provided output string buffer
 * \param[out] buf_len size of buf in bytes
 * \param[in] bits A sequence of unpacked bits
 * \param[in] len Length of bits
 * \return The output buffer (buf).
 */
char *osmo_ubit_dump_buf(char *buf, size_t buf_len, const uint8_t *bits, unsigned int len)
{
	unsigned int i;

	if (len > buf_len-1)
		len = buf_len-1;
	memset(buf, 0, buf_len);

	for (i = 0; i < len; i++) {
		char outch;
		switch (bits[i]) {
		case 0:
			outch = '0';
			break;
		case 0xff:
			outch = '?';
			break;
		case 1:
			outch = '1';
			break;
		default:
			outch = 'E';
			break;
		}
		buf[i] = outch;
	}
	buf[buf_len-1] = 0;
	return buf;
}

/*! Convert a sequence of unpacked bits to ASCII string, in static buffer.
 * \param[in] bits A sequence of unpacked bits
 * \param[in] len Length of bits
 * \returns string representation in static buffer.
 */
char *osmo_ubit_dump(const uint8_t *bits, unsigned int len)
{
	return osmo_ubit_dump_buf(hexd_buff, sizeof(hexd_buff), bits, len);
}

/*! Convert binary sequence to hexadecimal ASCII string
 *  \param[in] buf pointer to sequence of bytes
 *  \param[in] len length of buf in number of bytes
 *  \returns pointer to zero-terminated string
 *
 * This function will print a sequence of bytes as hexadecimal numbers,
 * adding one space character between each byte (e.g. "1a ef d9")
 *
 * The maximum size of the output buffer is 4096 bytes, i.e. the maximum
 * number of input bytes that can be printed in one call is 1365!
 */
char *osmo_hexdump(const unsigned char *buf, int len)
{
	osmo_hexdump_buf(hexd_buff, sizeof(hexd_buff), buf, len, " ", true);
	return hexd_buff;
}

/*! Convert binary sequence to hexadecimal ASCII string
 *  \param[in] ctx talloc context from where to allocate the output string
 *  \param[in] buf pointer to sequence of bytes
 *  \param[in] len length of buf in number of bytes
 *  \returns pointer to zero-terminated string
 *
 * This function will print a sequence of bytes as hexadecimal numbers,
 * adding one space character between each byte (e.g. "1a ef d9")
 */
char *osmo_hexdump_c(const void *ctx, const unsigned char *buf, int len)
{
	size_t hexd_buff_len = len * 3 + 1;
	char *hexd_buff = talloc_size(ctx, hexd_buff_len);
	if (!hexd_buff)
		return NULL;
	osmo_hexdump_buf(hexd_buff, hexd_buff_len, buf, len, " ", true);
	return hexd_buff;
}

/*! Convert binary sequence to hexadecimal ASCII string
 *  \param[in] buf pointer to sequence of bytes
 *  \param[in] len length of buf in number of bytes
 *  \returns pointer to zero-terminated string
 *
 * This function will print a sequence of bytes as hexadecimal numbers,
 * without any space character between each byte (e.g. "1aefd9")
 *
 * The maximum size of the output buffer is 4096 bytes, i.e. the maximum
 * number of input bytes that can be printed in one call is 2048!
 */
char *osmo_hexdump_nospc(const unsigned char *buf, int len)
{
	osmo_hexdump_buf(hexd_buff, sizeof(hexd_buff), buf, len, "", true);
	return hexd_buff;
}

/*! Convert binary sequence to hexadecimal ASCII string
 *  \param[in] ctx talloc context from where to allocate the output string
 *  \param[in] buf pointer to sequence of bytes
 *  \param[in] len length of buf in number of bytes
 *  \returns pointer to zero-terminated string
 *
 * This function will print a sequence of bytes as hexadecimal numbers,
 * without any space character between each byte (e.g. "1aefd9")
 */
char *osmo_hexdump_nospc_c(const void *ctx, const unsigned char *buf, int len)
{
	size_t hexd_buff_len = len * 2 + 1;
	char *hexd_buff = talloc_size(ctx, hexd_buff_len);
	if (!hexd_buff)
		return NULL;
	osmo_hexdump_buf(hexd_buff, hexd_buff_len, buf, len, "", true);
	return hexd_buff;
}


/* Compat with previous typo to preserve abi */
char *osmo_osmo_hexdump_nospc(const unsigned char *buf, int len)
#if defined(__MACH__) && defined(__APPLE__)
	;
#else
	__attribute__((weak, alias("osmo_hexdump_nospc")));
#endif

#include "osmo_config.h"
#ifdef HAVE_CTYPE_H
#include <ctype.h>
/*! Convert an entire string to lower case
 *  \param[out] out output string, caller-allocated
 *  \param[in] in input string
 */
void osmo_str2lower(char *out, const char *in)
{
	unsigned int i;

	for (i = 0; i < strlen(in); i++)
		out[i] = tolower((const unsigned char)in[i]);
	out[strlen(in)] = '\0';
}

/*! Convert an entire string to upper case
 *  \param[out] out output string, caller-allocated
 *  \param[in] in input string
 */
void osmo_str2upper(char *out, const char *in)
{
	unsigned int i;

	for (i = 0; i < strlen(in); i++)
		out[i] = toupper((const unsigned char)in[i]);
	out[strlen(in)] = '\0';
}
#endif /* HAVE_CTYPE_H */

/*! Wishful thinking to generate a constant time compare
 *  \param[in] exp Expected data
 *  \param[in] rel Comparison value
 *  \param[in] count Number of bytes to compare
 *  \returns 1 in case \a exp equals \a rel; zero otherwise
 *
 * Compare count bytes of exp to rel. Return 0 if they are identical, 1
 * otherwise. Do not return a mismatch on the first mismatching byte,
 * but always compare all bytes, regardless. The idea is that the amount of
 * matching bytes cannot be inferred from the time the comparison took. */
int osmo_constant_time_cmp(const uint8_t *exp, const uint8_t *rel, const int count)
{
	int x = 0, i;

	for (i = 0; i < count; ++i)
		x |= exp[i] ^ rel[i];

	/* if x is zero, all data was identical */
	return x? 1 : 0;
}

/*! Generic retrieval of 1..8 bytes as big-endian uint64_t
 *  \param[in] data Input data as byte-array
 *  \param[in] data_len Length of \a data in octets
 *  \returns uint64_t of \a data interpreted as big-endian
 *
 * This is like osmo_load64be_ext, except that if data_len is less than
 * sizeof(uint64_t), the data is interpreted as the least significant bytes
 * (osmo_load64be_ext loads them as the most significant bytes into the
 * returned uint64_t). In this way, any integer size up to 64 bits can be
 * decoded conveniently by using sizeof(), without the need to call specific
 * numbered functions (osmo_load16, 32, ...). */
uint64_t osmo_decode_big_endian(const uint8_t *data, size_t data_len)
{
	uint64_t value = 0;

	while (data_len > 0) {
		value = (value << 8) + *data;
		data += 1;
		data_len -= 1;
	}

	return value;
}

/*! Generic big-endian encoding of big endian number up to 64bit
 *  \param[in] value unsigned integer value to be stored
 *  \param[in] data_len number of octets
 *  \returns static buffer containing big-endian stored value
 *
 * This is like osmo_store64be_ext, except that this returns a static buffer of
 * the result (for convenience, but not threadsafe). If data_len is less than
 * sizeof(uint64_t), only the least significant bytes of value are encoded. */
uint8_t *osmo_encode_big_endian(uint64_t value, size_t data_len)
{
	static __thread uint8_t buf[sizeof(uint64_t)];
	OSMO_ASSERT(data_len <= ARRAY_SIZE(buf));
	osmo_store64be_ext(value, buf, data_len);
	return buf;
}

/*! Copy a C-string into a sized buffer
 *  \param[in] src source string
 *  \param[out] dst destination string
 *  \param[in] siz size of the \a dst buffer
 *  \returns length of \a src
 *
 * Copy at most \a siz bytes from \a src to \a dst, ensuring that the result is
 * NUL terminated. The NUL character is included in \a siz, i.e. passing the
 * actual sizeof(*dst) is correct.
 *
 * Note, a similar function that also limits the input buffer size is osmo_print_n().
 */
size_t osmo_strlcpy(char *dst, const char *src, size_t siz)
{
	size_t ret = src ? strlen(src) : 0;

	if (siz) {
		size_t len = OSMO_MIN(siz - 1, ret);
		if (len)
			memcpy(dst, src, len);
		dst[len] = '\0';
	}
	return ret;
}

/*! Find first occurence of a char in a size limited string.
 * Like strchr() but with a buffer size limit.
 * \param[in] str  String buffer to examine.
 * \param[in] str_size  sizeof(str).
 * \param[in] c  Character to look for.
 * \return Pointer to the matched char, or NULL if not found.
 */
const char *osmo_strnchr(const char *str, size_t str_size, char c)
{
	const char *end = str + str_size;
	const char *pos;
	if (!str)
		return NULL;
	for (pos = str; pos < end; pos++) {
		if (c == *pos)
			return pos;
		if (!*pos)
			return NULL;
	}
	return NULL;
}

/*! Validate that a given string is a hex string within given size limits.
 * Note that each hex digit amounts to a nibble, so if checking for a hex
 * string to result in N bytes, pass amount of digits as 2*N.
 * \param str  A nul-terminated string to validate, or NULL.
 * \param min_digits  least permitted amount of digits.
 * \param max_digits  most permitted amount of digits.
 * \param require_even  if true, require an even amount of digits.
 * \returns true when the hex_str contains only hexadecimal digits (no
 *          whitespace) and matches the requested length; also true
 *          when min_digits <= 0 and str is NULL.
 */
bool osmo_is_hexstr(const char *str, int min_digits, int max_digits,
		    bool require_even)
{
	int len;
	/* Use unsigned char * to avoid a compiler warning of
	 * "error: array subscript has type 'char' [-Werror=char-subscripts]" */
	const unsigned char *pos = (const unsigned char*)str;
	if (!pos)
		return min_digits < 1;
	for (len = 0; *pos && len < max_digits; len++, pos++)
		if (!isxdigit(*pos))
			return false;
	if (len < min_digits)
		return false;
	/* With not too many digits, we should have reached *str == nul */
	if (*pos)
		return false;
	if (require_even && (len & 1))
		return false;

	return true;
}

static const char osmo_identifier_illegal_chars[] = "., {}[]()<>|~\\^`'\"?=;/+*&%$#!";

/*! Determine if a given identifier is valid, i.e. doesn't contain illegal chars
 *  \param[in] str String to validate
 *  \param[in] sep_chars Permitted separation characters between identifiers.
 *  \returns true in case \a str contains only valid identifiers and sep_chars, false otherwise
 */
bool osmo_separated_identifiers_valid(const char *str, const char *sep_chars)
{
	/* characters that are illegal in names */
	unsigned int i;
	size_t len;

	/* an empty string is not a valid identifier */
	if (!str || (len = strlen(str)) == 0)
		return false;

	for (i = 0; i < len; i++) {
		if (sep_chars && strchr(sep_chars, str[i]))
			continue;
		/* check for 7-bit ASCII */
		if (str[i] & 0x80)
			return false;
		if (!isprint((int)str[i]))
			return false;
		/* check for some explicit reserved control characters */
		if (strchr(osmo_identifier_illegal_chars, str[i]))
			return false;
	}

	return true;
}

/*! Determine if a given identifier is valid, i.e. doesn't contain illegal chars
 *  \param[in] str String to validate
 *  \returns true in case \a str contains valid identifier, false otherwise
 */
bool osmo_identifier_valid(const char *str)
{
	return osmo_separated_identifiers_valid(str, NULL);
}

/*! Replace characters in the given string buffer so that it is guaranteed to pass osmo_separated_identifiers_valid().
 * To guarantee passing osmo_separated_identifiers_valid(), replace_with must not itself be an illegal character. If in
 * doubt, use '-'.
 * \param[inout] str  Identifier to sanitize, must be nul terminated and in a writable buffer.
 * \param[in] sep_chars  Additional characters that are to be replaced besides osmo_identifier_illegal_chars.
 * \param[in] replace_with  Replace any illegal characters with this character.
 */
void osmo_identifier_sanitize_buf(char *str, const char *sep_chars, char replace_with)
{
	char *pos;
	if (!str)
		return;
	for (pos = str; *pos; pos++) {
		if (strchr(osmo_identifier_illegal_chars, *pos)
		    || (sep_chars && strchr(sep_chars, *pos)))
			*pos = replace_with;
	}
}

/*! Like osmo_escape_str_buf2, but with unusual ordering of arguments, and may sometimes return string constants instead
 * of writing to buf for error cases or empty input.
 * Most *_buf() functions have the buffer and size as first arguments, here the arguments are last.
 * In particular, this function signature doesn't work with OSMO_STRBUF_APPEND_NOLEN().
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \param[inout] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  size of \a buf.
 * \returns buf containing an escaped representation, possibly truncated,
 *          or "(null)" if str == NULL, or "(error)" in case of errors.
 */
const char *osmo_escape_str_buf(const char *str, int in_len, char *buf, size_t bufsize)
{
	if (!str)
		return "(null)";
	if (!buf || !bufsize)
		return "(error)";
	return osmo_escape_str_buf2(buf, bufsize, str, in_len);
}

/*! Copy N characters to a buffer with a function signature useful for OSMO_STRBUF_APPEND().
 * Similarly to snprintf(), the result is always nul terminated (except if buf is NULL or bufsize is 0).
 * \param[out] buf  Target buffer.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  String to copy.
 * \param[in] n  Maximum number of non-nul characters to copy.
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
int osmo_print_n(char *buf, size_t bufsize, const char *str, size_t n)
{
	size_t write_n;

	if (!str)
		str = "";

	n = strnlen(str, n);

	if (!buf || !bufsize)
		return n;
	write_n = n;
	if (write_n >= bufsize)
		write_n = bufsize - 1;
	if (write_n)
		strncpy(buf, str, write_n);
	buf[write_n] = '\0';

	return n;
}

/*! Return the string with all non-printable characters escaped.
 * This internal function is the implementation for all osmo_escape_str* and osmo_quote_str* API versions.
 * It provides both the legacy (non C compatible) escaping, as well as C compatible string constant syntax,
 * and it provides a return value of characters-needed, to allow producing un-truncated strings in all cases.
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length (also past nul chars).
 * \param[in] legacy_format  If false, return C compatible string constants ("\x0f"), if true the legacy
 *                           escaping format ("\15"). The legacy format also escapes as "\a\b\f\v", while
 *                           the non-legacy format also escapes those as "\xNN" sequences.
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
static int _osmo_escape_str_buf(char *buf, size_t bufsize, const char *str, int in_len, bool legacy_format)
{
	struct osmo_strbuf sb = { .buf = buf, .len = bufsize };
	int in_pos = 0;
	int next_unprintable = 0;

	if (!str)
		in_len = 0;

	if (in_len < 0)
		in_len = strlen(str);

	/* Make sure of '\0' termination */
	if (!in_len)
		OSMO_STRBUF_PRINTF(sb, "%s", "");

	while (in_pos < in_len) {
		for (next_unprintable = in_pos;
		     next_unprintable < in_len && isprint((int)str[next_unprintable])
		     && str[next_unprintable] != '"'
		     && str[next_unprintable] != '\\';
		     next_unprintable++);

		OSMO_STRBUF_APPEND(sb, osmo_print_n, &str[in_pos], next_unprintable - in_pos);
		in_pos = next_unprintable;

		if (in_pos == in_len)
			goto done;

		switch (str[next_unprintable]) {
#define BACKSLASH_CASE(c, repr) \
		case c: \
			OSMO_STRBUF_PRINTF(sb, "\\%c", repr); \
			break

		BACKSLASH_CASE('\n', 'n');
		BACKSLASH_CASE('\r', 'r');
		BACKSLASH_CASE('\t', 't');
		BACKSLASH_CASE('\0', '0');
		BACKSLASH_CASE('\\', '\\');
		BACKSLASH_CASE('"', '"');

		default:
			if (legacy_format) {
				switch (str[next_unprintable]) {
				BACKSLASH_CASE('\a', 'a');
				BACKSLASH_CASE('\b', 'b');
				BACKSLASH_CASE('\v', 'v');
				BACKSLASH_CASE('\f', 'f');
				default:
					OSMO_STRBUF_PRINTF(sb, "\\%u", (unsigned char)str[in_pos]);
					break;
				}
				break;
			}

			OSMO_STRBUF_PRINTF(sb, "\\x%02x", (unsigned char)str[in_pos]);
			break;
		}
		in_pos ++;
#undef BACKSLASH_CASE
	}

done:
	return sb.chars_needed;
}

/*! Return the string with all non-printable characters escaped.
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length (also past nul chars).
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
int osmo_escape_str_buf3(char *buf, size_t bufsize, const char *str, int in_len)
{
	return _osmo_escape_str_buf(buf, bufsize, str, in_len, false);
}

/*! Return the string with all non-printable characters escaped.
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length (also past nul chars).
 * \return The output buffer (buf).
 */
char *osmo_escape_str_buf2(char *buf, size_t bufsize, const char *str, int in_len)
{
	_osmo_escape_str_buf(buf, bufsize, str, in_len, true);
	return buf;
}

/*! Return the string with all non-printable characters escaped.
 * Call osmo_escape_str_buf() with a static buffer.
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns buf containing an escaped representation, possibly truncated, or str itself.
 */
const char *osmo_escape_str(const char *str, int in_len)
{
	return osmo_escape_str_buf(str, in_len, namebuf, sizeof(namebuf));
}

/*! Return the string with all non-printable characters escaped, in dynamically-allocated buffer.
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns dynamically-allocated output buffer, containing an escaped representation
 */
char *osmo_escape_str_c(const void *ctx, const char *str, int in_len)
{
	/* The string will be at least as long as in_len, but some characters might need escaping.
	 * These extra bytes should catch most usual escaping situations, avoiding a second run in OSMO_NAME_C_IMPL. */
	OSMO_NAME_C_IMPL(ctx, in_len + 16, "ERROR", _osmo_escape_str_buf, str, in_len, true);
}

/*! Return a quoted and escaped representation of the string.
 * This internal function is the implementation for all osmo_quote_str* API versions.
 * It provides both the legacy (non C compatible) escaping, as well as C compatible string constant syntax,
 * and it provides a return value of characters-needed, to allow producing un-truncated strings in all cases.
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length (also past nul chars).
 * \param[in] legacy_format  If false, return C compatible string constants ("\x0f"), if true the legacy
 *                           escaping format ("\15"). The legacy format also escapes as "\a\b\f\v", while
 *                           the non-legacy format also escapes those as "\xNN" sequences.
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
static size_t _osmo_quote_str_buf(char *buf, size_t bufsize, const char *str, int in_len, bool legacy_format)
{
	struct osmo_strbuf sb = { .buf = buf, .len = bufsize };
	if (!str)
		OSMO_STRBUF_PRINTF(sb, "NULL");
	else {
		OSMO_STRBUF_PRINTF(sb, "\"");
		OSMO_STRBUF_APPEND(sb, _osmo_escape_str_buf, str, in_len, legacy_format);
		OSMO_STRBUF_PRINTF(sb, "\"");
	}
	return sb.chars_needed;
}

/*! Like osmo_escape_str_buf3(), but returns double-quotes around a string, or "NULL" for a NULL string.
 * This allows passing any char* value and get its C representation as string.
 * The function signature is suitable for OSMO_STRBUF_APPEND_NOLEN().
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
int osmo_quote_str_buf3(char *buf, size_t bufsize, const char *str, int in_len)
{
	return _osmo_quote_str_buf(buf, bufsize, str, in_len, false);
}

/*! Like osmo_escape_str_buf2(), but returns double-quotes around a string, or "NULL" for a NULL string.
 * This allows passing any char* value and get its C representation as string.
 * The function signature is suitable for OSMO_STRBUF_APPEND_NOLEN().
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \return The output buffer (buf).
 */
char *osmo_quote_str_buf2(char *buf, size_t bufsize, const char *str, int in_len)
{
	_osmo_quote_str_buf(buf, bufsize, str, in_len, true);
	return buf;
}

/*! Like osmo_quote_str_buf2, but with unusual ordering of arguments, and may sometimes return string constants instead
 * of writing to buf for error cases or empty input.
 * Most *_buf() functions have the buffer and size as first arguments, here the arguments are last.
 * In particular, this function signature doesn't work with OSMO_STRBUF_APPEND_NOLEN().
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns buf containing a quoted and escaped representation, possibly truncated.
 */
const char *osmo_quote_str_buf(const char *str, int in_len, char *buf, size_t bufsize)
{
	if (!str)
		return "NULL";
	if (!buf || !bufsize)
		return "(error)";
	_osmo_quote_str_buf(buf, bufsize, str, in_len, true);
	return buf;
}

/*! Like osmo_quote_str_buf() but returns the result in a static buffer.
 * The static buffer is shared with get_value_string() and osmo_escape_str().
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns static buffer containing a quoted and escaped representation, possibly truncated.
 */
const char *osmo_quote_str(const char *str, int in_len)
{
	_osmo_quote_str_buf(namebuf, sizeof(namebuf), str, in_len, true);
	return namebuf;
}

/*! Like osmo_quote_str_buf() but returns the result in a dynamically-allocated buffer.
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns dynamically-allocated buffer containing a quoted and escaped representation.
 */
char *osmo_quote_str_c(const void *ctx, const char *str, int in_len)
{
	/* The string will be at least as long as in_len, but some characters might need escaping.
	 * These extra bytes should catch most usual escaping situations, avoiding a second run in OSMO_NAME_C_IMPL. */
	OSMO_NAME_C_IMPL(ctx, in_len + 16, "ERROR", _osmo_quote_str_buf, str, in_len, true);
}

/*! Return the string with all non-printable characters escaped.
 * In contrast to osmo_escape_str_buf2(), this returns the needed buffer size suitable for OSMO_STRBUF_APPEND(), and
 * this escapes characters in a way compatible with C string constant syntax.
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length (also past nul chars).
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
size_t osmo_escape_cstr_buf(char *buf, size_t bufsize, const char *str, int in_len)
{
	return _osmo_escape_str_buf(buf, bufsize, str, in_len, false);
}

/*! Return the string with all non-printable characters escaped, in dynamically-allocated buffer.
 * In contrast to osmo_escape_str_c(), this escapes characters in a way compatible with C string constant syntax, and
 * allocates sufficient memory in all cases.
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns dynamically-allocated buffer, containing an escaped representation.
 */
char *osmo_escape_cstr_c(void *ctx, const char *str, int in_len)
{
	/* The string will be at least as long as in_len, but some characters might need escaping.
	 * These extra bytes should catch most usual escaping situations, avoiding a second run in OSMO_NAME_C_IMPL. */
	OSMO_NAME_C_IMPL(ctx, in_len + 16, "ERROR", _osmo_escape_str_buf, str, in_len, false);
}

/*! Like osmo_escape_str_buf2(), but returns double-quotes around a string, or "NULL" for a NULL string.
 * This allows passing any char* value and get its C representation as string.
 * The function signature is suitable for OSMO_STRBUF_APPEND_NOLEN().
 * In contrast to osmo_escape_str_buf2(), this returns the needed buffer size suitable for OSMO_STRBUF_APPEND(), and
 * this escapes characters in a way compatible with C string constant syntax.
 * \param[out] buf  string buffer to write escaped characters to.
 * \param[in] bufsize  sizeof(buf).
 * \param[in] str  A string that may contain any characters.
 * \param[in] in_len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \return Number of characters that would be written if bufsize were large enough excluding '\0' (like snprintf()).
 */
size_t osmo_quote_cstr_buf(char *buf, size_t bufsize, const char *str, int in_len)
{
	return _osmo_quote_str_buf(buf, bufsize, str, in_len, false);
}

/*! Return the string quoted and with all non-printable characters escaped, in dynamically-allocated buffer.
 * In contrast to osmo_quote_str_c(), this escapes characters in a way compatible with C string constant syntax, and
 * allocates sufficient memory in all cases.
 * \param[in] str  A string that may contain any characters.
 * \param[in] len  Pass -1 to print until nul char, or >= 0 to force a length.
 * \returns dynamically-allocated buffer, containing a quoted and escaped representation.
 */
char *osmo_quote_cstr_c(void *ctx, const char *str, int in_len)
{
	/* The string will be at least as long as in_len plus two quotes, but some characters might need escaping.
	 * These extra bytes should catch most usual escaping situations, avoiding a second run in OSMO_NAME_C_IMPL. */
	OSMO_NAME_C_IMPL(ctx, in_len + 16, "ERROR", _osmo_quote_str_buf, str, in_len, false);
}

/*! perform an integer square root operation on unsigned 32bit integer.
 *  This implementation is taken from "Hacker's Delight" Figure 11-1 "Integer square root, Newton's
 *  method", which can also be found at http://www.hackersdelight.org/hdcodetxt/isqrt.c.txt */
uint32_t osmo_isqrt32(uint32_t x)
{
	uint32_t x1;
	int s, g0, g1;

	if (x <= 1)
		return x;

	s = 1;
	x1 = x - 1;
	if (x1 > 0xffff) {
		s = s + 8;
		x1 = x1 >> 16;
	}
	if (x1 > 0xff) {
		s = s + 4;
		x1 = x1 >> 8;
	}
	if (x1 > 0xf) {
		s = s + 2;
		x1 = x1 >> 4;
	}
	if (x1 > 0x3) {
		s = s + 1;
	}

	g0 = 1 << s;			/* g0 = 2**s */
	g1 = (g0 + (x >> s)) >> 1;	/* g1 = (g0 + x/g0)/2 */

	/* converges after four to five divisions for arguments up to 16,785,407 */
	while (g1 < g0) {
		g0 = g1;
		g1 = (g0 + (x/g0)) >> 1;
	}
	return g0;
}

/*! Convert a string to lowercase, while checking buffer size boundaries.
 * The result written to \a dest is guaranteed to be nul terminated if \a dest_len > 0.
 * If dest == src, the string is converted in-place, if necessary truncated at dest_len - 1 characters
 * length as well as nul terminated.
 * Note: similar osmo_str2lower(), but safe to use for src strings of arbitrary length.
 *  \param[out] dest  Target buffer to write lowercase string.
 *  \param[in] dest_len  Maximum buffer size of dest (e.g. sizeof(dest)).
 *  \param[in] src  String to convert to lowercase.
 *  \returns Length of \a src, like osmo_strlcpy(), but if \a dest == \a src at most \a dest_len - 1.
 */
size_t osmo_str_tolower_buf(char *dest, size_t dest_len, const char *src)
{
	size_t rc;
	if (dest == src) {
		if (dest_len < 1)
			return 0;
		dest[dest_len - 1] = '\0';
		rc = strlen(dest);
	} else {
		if (dest_len < 1)
			return strlen(src);
		rc = osmo_strlcpy(dest, src, dest_len);
	}
	for (; *dest; dest++)
		*dest = tolower(*dest);
	return rc;
}

/*! Convert a string to lowercase, using a static buffer.
 * The resulting string may be truncated if the internally used static buffer is shorter than src.
 * The internal buffer is at least 128 bytes long, i.e. guaranteed to hold at least 127 characters and a
 * terminating nul. The static buffer returned is shared with osmo_str_toupper().
 * See also osmo_str_tolower_buf().
 * \param[in] src  String to convert to lowercase.
 * \returns Resulting lowercase string in a static buffer, always nul terminated.
 */
const char *osmo_str_tolower(const char *src)
{
	osmo_str_tolower_buf(capsbuf, sizeof(capsbuf), src);
	return capsbuf;
}

/*! Convert a string to lowercase, dynamically allocating the output from given talloc context
 * See also osmo_str_tolower_buf().
 * \param[in] ctx  talloc context from where to allocate the output string
 * \param[in] src  String to convert to lowercase.
 * \returns Resulting lowercase string in a dynamically allocated buffer, always nul terminated.
 */
char *osmo_str_tolower_c(const void *ctx, const char *src)
{
	size_t buf_len = strlen(src) + 1;
	char *buf = talloc_size(ctx, buf_len);
	if (!buf)
		return NULL;
	osmo_str_tolower_buf(buf, buf_len, src);
	return buf;
}

/*! Convert a string to uppercase, while checking buffer size boundaries.
 * The result written to \a dest is guaranteed to be nul terminated if \a dest_len > 0.
 * If dest == src, the string is converted in-place, if necessary truncated at dest_len - 1 characters
 * length as well as nul terminated.
 * Note: similar osmo_str2upper(), but safe to use for src strings of arbitrary length.
 *  \param[out] dest  Target buffer to write uppercase string.
 *  \param[in] dest_len  Maximum buffer size of dest (e.g. sizeof(dest)).
 *  \param[in] src  String to convert to uppercase.
 *  \returns Length of \a src, like osmo_strlcpy(), but if \a dest == \a src at most \a dest_len - 1.
 */
size_t osmo_str_toupper_buf(char *dest, size_t dest_len, const char *src)
{
	size_t rc;
	if (dest == src) {
		if (dest_len < 1)
			return 0;
		dest[dest_len - 1] = '\0';
		rc = strlen(dest);
	} else {
		if (dest_len < 1)
			return strlen(src);
		rc = osmo_strlcpy(dest, src, dest_len);
	}
	for (; *dest; dest++)
		*dest = toupper(*dest);
	return rc;
}

/*! Convert a string to uppercase, using a static buffer.
 * The resulting string may be truncated if the internally used static buffer is shorter than src.
 * The internal buffer is at least 128 bytes long, i.e. guaranteed to hold at least 127 characters and a
 * terminating nul. The static buffer returned is shared with osmo_str_tolower().
 * See also osmo_str_toupper_buf().
 * \param[in] src  String to convert to uppercase.
 * \returns Resulting uppercase string in a static buffer, always nul terminated.
 */
const char *osmo_str_toupper(const char *src)
{
	osmo_str_toupper_buf(capsbuf, sizeof(capsbuf), src);
	return capsbuf;
}

/*! Convert a string to uppercase, dynamically allocating the output from given talloc context
 * See also osmo_str_tolower_buf().
 * \param[in] ctx  talloc context from where to allocate the output string
 * \param[in] src  String to convert to uppercase.
 * \returns Resulting uppercase string in a dynamically allocated buffer, always nul terminated.
 */
char *osmo_str_toupper_c(const void *ctx, const char *src)
{
	size_t buf_len = strlen(src) + 1;
	char *buf = talloc_size(ctx, buf_len);
	if (!buf)
		return NULL;
	osmo_str_toupper_buf(buf, buf_len, src);
	return buf;
}

/*! Calculate the Luhn checksum (as used for IMEIs).
 * \param[in] in  Input digits in ASCII string representation.
 * \param[in] in_len  Count of digits to use for the input (14 for IMEI).
 * \returns checksum char (e.g. '3'); negative on error
 */
char osmo_luhn(const char* in, int in_len)
{
	int i, sum = 0;

	/* All input must be numbers */
	for (i = 0; i < in_len; i++) {
		if (!isdigit((unsigned char)in[i]))
			return -EINVAL;
	}

	/* Double every second digit and add it to sum */
	for (i = in_len - 1; i >= 0; i -= 2) {
		int dbl = (in[i] - '0') * 2;
		if (dbl > 9)
			dbl -= 9;
		sum += dbl;
	}

	/* Add other digits to sum */
	for (i = in_len - 2; i >= 0; i -= 2)
		sum += in[i] - '0';

	/* Final checksum */
	return (sum * 9) % 10 + '0';
}

/*! Remove up to N chars from the end of an osmo_strbuf.
 * |--char-count---| - - chars_needed - - |
 *               |<---------drop----------|
 */
void osmo_strbuf_drop_tail(struct osmo_strbuf *sb, size_t n_chars)
{
	size_t drop_n;
	if (sb->pos <= sb->buf)
		return;
	drop_n = OSMO_MIN(sb->chars_needed, n_chars);
	sb->chars_needed -= drop_n;
	/* chars_needed was reduced by n_chars, which may have been entirely behind the end of a full buffer, within the
	 * hypothetical chars_needed. Modify the buffer tail pos only if the buffer is not or longer full now. */
	if (sb->chars_needed >= OSMO_STRBUF_CHAR_COUNT(*sb))
		return;
	sb->pos = sb->buf + sb->chars_needed;
	*sb->pos = '\0';
}

/*! Let osmo_strbuf know that n_chars characters (excluding nul) were written to the end of the buffer.
 * If sb is nonempty, the n_chars are assumed to have been written to sb->pos. If sb is still empty and pos == NULL, the
 * n_chars are assumed to have been written to the start of the buffer.
 * Advance sb->pos and sb->chars_needed by at most n_chars, or up to sb->len - 1.
 * Ensure nul termination. */
void osmo_strbuf_added_tail(struct osmo_strbuf *sb, size_t n_chars)
{
	/* On init of an osmo_strbuf, sb->pos == NULL, which is defined as semantically identical to pointing at the
	 * start of the buffer. A caller may just write to the buffer and call osmo_strbuf_added_tail(), in which case
	 * still pos == NULL. pos != NULL happens as soon as the first OSMO_STRBUF_*() API has acted on the strbuf. */
	if (!sb->pos)
		sb->pos = sb->buf;
	sb->chars_needed += n_chars;
	/* first get remaining space, not counting trailing nul; but safeguard against empty buffer */
	size_t n_added = OSMO_STRBUF_REMAIN(*sb);
	if (n_added)
		n_added--;
	/* do not add more than fit in sb->len, still ensuring nul termination */
	n_added = OSMO_MIN(n_added, n_chars);
	if (n_added)
		sb->pos += n_added;
	/* when a strbuf is full, sb->pos may point after the final nul, so nul terminate only when pos is valid. */
	if (sb->pos < sb->buf + sb->len)
		*sb->pos = '\0';
}

/*! Compare start of a string.
 * This is an optimisation of 'strstr(str, startswith_str) == str' because it doesn't search through the entire string.
 * \param str  (Longer) string to compare.
 * \param startswith_str  (Shorter) string to compare with the start of str.
 * \return true iff the first characters of str fully match startswith_str or startswith_str is empty. */
bool osmo_str_startswith(const char *str, const char *startswith_str)
{
	if (!startswith_str || !*startswith_str)
		return true;
	if (!str)
		return false;
	return strncmp(str, startswith_str, strlen(startswith_str)) == 0;
}

/*! Convert a string of a floating point number to a signed int, with a decimal factor (fixed-point precision).
 * For example, with precision=3, convert "-1.23" to -1230. In other words, the float value is multiplied by
 * 10 to-the-power-of precision to obtain the returned integer.
 * The usable range of digits is -INT64_MAX .. INT64_MAX -- note, not INT64_MIN! The value of INT64_MIN is excluded to
 * reduce implementation complexity. See also utils_test.c.
 * The advantage over using sscanf("%f") is guaranteed precision: float or double types may apply rounding in the
 * conversion result. osmo_float_str_to_int() and osmo_int_to_float_str_buf() guarantee true results when converting
 * back and forth between string and int.
 * \param[out] val  Returned integer value.
 * \param[in] str  String of a float, like '-12.345'.
 * \param[in] precision  Fixed-point precision, or  * \returns 0 on success, negative on error.
 */
int osmo_float_str_to_int(int64_t *val, const char *str, unsigned int precision)
{
	const char *point;
	char *endptr;
	const char *p;
	int64_t sign = 1;
	int64_t integer = 0;
	int64_t decimal = 0;
	int64_t precision_factor;
	int64_t integer_max;
	int64_t decimal_max;
	unsigned int i;

	OSMO_ASSERT(val);
	*val = 0;

	if (!str)
		return -EINVAL;
	if (str[0] == '-') {
		str = str + 1;
		sign = -1;
	} else if (str[0] == '+') {
		str = str + 1;
	}
	if (!str[0])
		return -EINVAL;

	/* Validate entire string as purely digits and at most one decimal dot. If not doing this here in advance,
	 * parsing digits might stop early because of precision cut-off and miss validation of input data. */
	point = NULL;
	for (p = str; *p; p++) {
		if (*p == '.') {
			if (point)
				return -EINVAL;
			point = p;
		} else if (!isdigit((unsigned char)*p))
			return -EINVAL;
	}

	/* Parse integer part if there is one. If the string starts with a point, there's nothing to parse for the
	 * integer part. */
	if (!point || point > str) {
		errno = 0;
		integer = strtoll(str, &endptr, 10);
		if ((errno == ERANGE && (integer == LLONG_MAX || integer == LLONG_MIN))
		    || (errno != 0 && integer == 0))
			return -ERANGE;

		if ((point && endptr != point)
		    || (!point && *endptr))
			return -EINVAL;
	}

	/* Parse the fractional part if there is any, and if the precision is nonzero (if we even care about fractional
	 * digits) */
	if (precision && point && point[1] != '\0') {
		/* limit the number of digits parsed to 'precision'.
		 * If 'precision' is larger than the 19 digits representable in int64_t, skip some, to pick up lower
		 * magnitude digits. */
		unsigned int skip_digits = (precision < 20) ? 0 : precision - 20;
		char decimal_str[precision + 1];
		osmo_strlcpy(decimal_str, point+1, precision+1);

		/* fill with zeros to make exactly 'precision' digits */
		for (i = strlen(decimal_str); i < precision; i++)
			decimal_str[i] = '0';
		decimal_str[precision] = '\0';

		for (i = 0; i < skip_digits; i++) {
			/* When skipping digits because precision > nr-of-digits-in-int64_t, they must be zero;
			 * if there is a nonzero digit above the precision, it's -ERANGE. */
			if (decimal_str[i] != '0')
				return -ERANGE;
		}
		errno = 0;
		decimal = strtoll(decimal_str + skip_digits, &endptr, 10);
		if ((errno == ERANGE && (decimal == LLONG_MAX || decimal == LLONG_MIN))
		    || (errno != 0 && decimal == 0))
			return -ERANGE;

		if (*endptr)
			return -EINVAL;
	}

	if (precision > 18) {
		/* Special case of returning more digits than fit in int64_t range, e.g.
		 * osmo_float_str_to_int("0.0000000012345678901234567", precision=25) -> 12345678901234567. */
		precision_factor = 0;
		integer_max = 0;
		decimal_max = INT64_MAX;
	} else {
		/* Do not surpass the resulting int64_t range. Depending on the amount of precision, the integer part
		 * and decimal part have specific ranges they must comply to. */
		precision_factor = 1;
		for (i = 0; i < precision; i++)
		     precision_factor *= 10;
		integer_max = INT64_MAX / precision_factor;
		if (integer == integer_max)
			decimal_max = INT64_MAX % precision_factor;
		else
			decimal_max = INT64_MAX;
	}

	if (integer > integer_max)
		return -ERANGE;
	if (decimal > decimal_max)
		return -ERANGE;

	*val = sign * (integer * precision_factor + decimal);
	return 0;
}

/*! Convert an integer to a floating point string using a decimal quotient (fixed-point precision).
 * For example, with precision = 3, convert -1230 to "-1.23".
 * The usable range of digits is -INT64_MAX .. INT64_MAX -- note, not INT64_MIN! The value of INT64_MIN is excluded to
 * reduce implementation complexity. See also utils_test.c.
 * The advantage over using printf("%.6g") is guaranteed precision: float or double types may apply rounding in the
 * conversion result. osmo_float_str_to_int() and osmo_int_to_float_str_buf() guarantee true results when converting
 * back and forth between string and int.
 * The resulting string omits trailing zeros in the fractional part (like "%g" would) but never applies rounding.
 * \param[out] buf  Buffer to write string to.
 * \param[in] buflen  sizeof(buf).
 * \param[in] val  Value to convert to float.
 * \returns number of chars that would be written, like snprintf().
 */
int osmo_int_to_float_str_buf(char *buf, size_t buflen, int64_t val, unsigned int precision)
{
	struct osmo_strbuf sb = { .buf = buf, .len = buflen };
	unsigned int i;
	unsigned int w;
	int64_t precision_factor;
	if (val < 0) {
		OSMO_STRBUF_PRINTF(sb, "-");
		if (val == INT64_MIN) {
			OSMO_STRBUF_PRINTF(sb, "ERR");
			return sb.chars_needed;
		}
		val = -val;
	}

	if (precision > 18) {
		/* Special case of returning more digits than fit in int64_t range, e.g.
		 * osmo_int_to_float_str(12345678901234567, precision=25) -> "0.0000000012345678901234567". */
		if (!val) {
			OSMO_STRBUF_PRINTF(sb, "0");
			return sb.chars_needed;
		}
		OSMO_STRBUF_PRINTF(sb, "0.");
		for (i = 19; i < precision; i++)
			OSMO_STRBUF_PRINTF(sb, "0");
		precision = 19;
	} else {
		precision_factor = 1;
		for (i = 0; i < precision; i++)
		     precision_factor *= 10;

		OSMO_STRBUF_PRINTF(sb, "%" PRId64, val / precision_factor);
		val %= precision_factor;
		if (!val)
			return sb.chars_needed;
		OSMO_STRBUF_PRINTF(sb, ".");
	}

	/* print fractional part, skip trailing zeros */
	w = precision;
	while (!(val % 10)) {
		val /= 10;
		w--;
	}
	OSMO_STRBUF_PRINTF(sb, "%0*" PRId64, w, val);
	return sb.chars_needed;
}

/*! Convert an integer with a factor of a million to a floating point string.
 * For example, convert -1230000 to "-1.23".
 * \param[in] ctx  Talloc ctx to allocate string buffer from.
 * \param[in] val  Value to convert to float.
 * \returns resulting string, dynamically allocated.
 */
char *osmo_int_to_float_str_c(void *ctx, int64_t val, unsigned int precision)
{
	OSMO_NAME_C_IMPL(ctx, 16, "ERROR", osmo_int_to_float_str_buf, val, precision)
}

/*! Convert a string of a number to int64_t, including all common strtoll() validity checks.
 * It's not so trivial to call strtoll() and properly verify that the input string was indeed a valid number string.
 * \param[out] result  Buffer for the resulting integer number, or NULL if the caller is only interested in the
 *                     validation result (returned rc).
 * \param[in] str  The string to convert.
 * \param[in] base  The integer base, i.e. 10 for decimal numbers or 16 for hexadecimal, as in strtoll().
 * \param[in] min_val  The smallest valid number expected in the string.
 * \param[in] max_val  The largest valid number expected in the string.
 * \return 0 on success, -EOVERFLOW if the number in the string exceeds int64_t, -ENOTSUPP if the base is not supported,
 * -ERANGE if the converted number exceeds the range [min_val..max_val] but is still within int64_t range, -E2BIG if
 * surplus characters follow after the number, -EINVAL if the string does not contain a number. In case of -ERANGE and
 * -E2BIG, the converted number is still accurately returned in result. In case of -EOVERFLOW, the returned value is
 * clamped to INT64_MIN..INT64_MAX.
 */
int osmo_str_to_int64(int64_t *result, const char *str, int base, int64_t min_val, int64_t max_val)
{
	long long int val;
	char *endptr;
	if (result)
		*result = 0;
	if (!str || !*str)
		return -EINVAL;
	errno = 0;
	val = strtoll(str, &endptr, base);
	/* In case the number string exceeds long long int range, strtoll() clamps the returned value to LLONG_MIN or
	 * LLONG_MAX. Make sure of the same here with respect to int64_t. */
	if (val < INT64_MIN) {
		if (result)
			*result = INT64_MIN;
		return -ERANGE;
	}
	if (val > INT64_MAX) {
		if (result)
			*result = INT64_MAX;
		return -ERANGE;
	}
	if (result)
		*result = (int64_t)val;
	switch (errno) {
	case 0:
		break;
	case ERANGE:
		return -EOVERFLOW;
	default:
	case EINVAL:
		return -ENOTSUP;
	}
	if (!endptr || *endptr) {
		/* No chars were converted */
		if (endptr == str)
			return -EINVAL;
		/* Or there are surplus chars after the converted number */
		return -E2BIG;
	}
	if (val < min_val || val > max_val)
		return -ERANGE;
	return 0;
}

/*! Convert a string of a number to int, including all common strtoll() validity checks.
 * Same as osmo_str_to_int64() but using the plain int data type.
 * \param[out] result  Buffer for the resulting integer number, or NULL if the caller is only interested in the
 *                     validation result (returned rc).
 * \param[in] str  The string to convert.
 * \param[in] base  The integer base, i.e. 10 for decimal numbers or 16 for hexadecimal, as in strtoll().
 * \param[in] min_val  The smallest valid number expected in the string.
 * \param[in] max_val  The largest valid number expected in the string.
 * \return 0 on success, -EOVERFLOW if the number in the string exceeds int range, -ENOTSUPP if the base is not supported,
 * -ERANGE if the converted number exceeds the range [min_val..max_val] but is still within int range, -E2BIG if
 * surplus characters follow after the number, -EINVAL if the string does not contain a number. In case of -ERANGE and
 * -E2BIG, the converted number is still accurately returned in result. In case of -EOVERFLOW, the returned value is
 * clamped to INT_MIN..INT_MAX.
 */
int osmo_str_to_int(int *result, const char *str, int base, int min_val, int max_val)
{
	int64_t val;
	int rc = osmo_str_to_int64(&val, str, base, min_val, max_val);
	/* In case the number string exceeds long long int range, strtoll() clamps the returned value to LLONG_MIN or
	 * LLONG_MAX. Make sure of the same here with respect to int. */
	if (val < INT_MIN) {
		if (result)
			*result = INT_MIN;
		return -EOVERFLOW;
	}
	if (val > INT_MAX) {
		if (result)
			*result = INT_MAX;
		return -EOVERFLOW;
	}
	if (result)
		*result = (int)val;
	return rc;
}

/*! Replace a string using talloc and release its prior content (if any).
 *  This is a format string capable equivalent of osmo_talloc_replace_string().
 * \param[in] ctx Talloc context to use for allocation.
 * \param[out] dst Pointer to string, will be updated with ptr to new string.
 * \param[in] fmt Format string that will be copied to newly allocated string. */
void osmo_talloc_replace_string_fmt(void *ctx, char **dst, const char *fmt, ...)
{
	char *name = NULL;

	if (fmt != NULL) {
		va_list ap;

		va_start(ap, fmt);
		name = talloc_vasprintf(ctx, fmt, ap);
		va_end(ap);
	}

	talloc_free(*dst);
	*dst = name;
}

/*! @} */
