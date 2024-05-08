/*! \file conv_acc_sse_avx.c
 * Accelerated Viterbi decoder implementation
 * for architectures with both SSSE3 and AVX2 support. */
/*
 * Copyright (C) 2013, 2014 Thomas Tsou <tom@tsou.cc>
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
 */

#include <stdint.h>
#include "osmo_config.h"

#include <emmintrin.h>
#include <tmmintrin.h>
#include <xmmintrin.h>
#include <immintrin.h>

#if defined(HAVE_SSE4_1)
#include <smmintrin.h>
#endif

#define SSE_ALIGN 16


/* Broadcast 16-bit integer
 * Repeat the low 16-bit integer to all elements of the 128-bit SSE
 * register. Only AVX2 has a dedicated broadcast instruction; use repeat
 * unpacks for SSE only architectures. This is a destructive operation and
 * the source register is overwritten.
 *
 * Input:
 * M0 - Low 16-bit element is read
 *
 * Output:
 * M0 - Contains broadcasted values
 */
#define SSE_BROADCAST(M0) \
{ \
	M0 = _mm_broadcastw_epi16(M0); \
}

/**
 * Include common SSE implementation
 */
#include <conv_acc_sse_impl.h>

/* Aligned Memory Allocator
 * SSE requires 16-byte memory alignment. We store relevant trellis values
 * (accumulated sums, outputs, and path decisions) as 16 bit signed integers
 * so the allocated memory is casted as such.
 */
__attribute__ ((visibility("hidden")))
int16_t *osmo_conv_sse_avx_vdec_malloc(size_t n)
{
	return (int16_t *) _mm_malloc(sizeof(int16_t) * n, SSE_ALIGN);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_vdec_free(int16_t *ptr)
{
	_mm_free(ptr);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_metrics_k5_n2(const int8_t *val,
	const int16_t *out, int16_t *sums, int16_t *paths, int norm)
{
	const int16_t _val[4] = { val[0], val[1], val[0], val[1] };

	_sse_metrics_k5_n2(_val, out, sums, paths, norm);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_metrics_k5_n3(const int8_t *val,
	const int16_t *out, int16_t *sums, int16_t *paths, int norm)
{
	const int16_t _val[4] = { val[0], val[1], val[2], 0 };

	_sse_metrics_k5_n4(_val, out, sums, paths, norm);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_metrics_k5_n4(const int8_t *val,
	const int16_t *out, int16_t *sums, int16_t *paths, int norm)
{
	const int16_t _val[4] = { val[0], val[1], val[2], val[3] };

	_sse_metrics_k5_n4(_val, out, sums, paths, norm);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_metrics_k7_n2(const int8_t *val,
	const int16_t *out, int16_t *sums, int16_t *paths, int norm)
{
	const int16_t _val[4] = { val[0], val[1], val[0], val[1] };

	_sse_metrics_k7_n2(_val, out, sums, paths, norm);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_metrics_k7_n3(const int8_t *val,
	const int16_t *out, int16_t *sums, int16_t *paths, int norm)
{
	const int16_t _val[4] = { val[0], val[1], val[2], 0 };

	_sse_metrics_k7_n4(_val, out, sums, paths, norm);
}

__attribute__ ((visibility("hidden")))
void osmo_conv_sse_avx_metrics_k7_n4(const int8_t *val,
	const int16_t *out, int16_t *sums, int16_t *paths, int norm)
{
	const int16_t _val[4] = { val[0], val[1], val[2], val[3] };

	_sse_metrics_k7_n4(_val, out, sums, paths, norm);
}
