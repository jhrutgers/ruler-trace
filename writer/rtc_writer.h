#ifndef __RTC_H
#define __RTC_H
/*
 * Ruler Trace Container
 * Copyright (C) 2020  Jochem Rutgers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stddef.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
/* C11 */
#  include <stdbool.h>
#  include <stdint.h>
#  ifdef RTC_64BIT_SIZE
typedef uint64_t rtc_offset;
#  else
typedef size_t rtc_offset;
#  endif
typedef uint32_t crc_t;
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
/* C99 */
#  include <stdbool.h>
#  include <stdint.h>
typedef size_t rtc_offset;
typedef uint32_t crc_t;
#else
/* Assume C89 */
#  ifndef bool
#    ifdef RTC_HAVE_STDBOOL_H
#      include <stdbool.h>
#    else
typedef enum { false, true } rtc_bool;
#      define bool rtc_bool
#    endif
#  endif
typedef size_t rtc_offset;
#  include <limits.h>
#  if UINT_MAX >= 0xffffffffUL
typedef unsigned int crc_t;
#  else
typedef unsigned long crc_t;
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
	RTC_FLAG_PLAIN = 0,
	RTC_FLAG_START = 1,
	RTC_FLAG_STOP = 2,
	RTC_FLAG_NEW_UNIT = 4,
	RTC_FLAG_FLUSH = 8
};

typedef unsigned char rtc_marker[4];
#define RTC_MARKER_1 { 0xB9, 0xB9, 0xB9, 0xB9 } /* superscript 1 in ISO-8859-1 */
#define RTC_MARKER RTC_MARKER_1

struct rtc_handle;

typedef int (rtc_write_callback)(struct rtc_handle* h, void const* buf, size_t len, int flags);

enum {
	RTC_MIN_UNIT_SIZE = 64
};

typedef struct rtc_param {
	size_t Unit; /* must be power of 2 */
	size_t unit;
	rtc_write_callback* write;
	void* arg;
} rtc_param;

#define RTC_STREAM_VARIABLE_LENGTH ((size_t)-1)

typedef struct rtc_stream_param {
	char const* name;
	size_t frame_length;
	char const* json; /* only fields (except id), including name and frame_length */
	bool hidden;
} rtc_stream_param;

typedef struct rtc_stream {
	rtc_stream_param const* param;
	struct rtc_handle* h;
	unsigned int id;
	char id_str[sizeof(unsigned int) <= 4 ? 11 : 21];
	size_t id_str_len;
	size_t param_json_len;
	bool used;
	rtc_offset index;
	struct rtc_stream* next;
	struct rtc_stream* prev;
} rtc_stream;

enum {
	RTC_STREAM_nop,
	RTC_STREAM_padding,
	RTC_STREAM_Marker,
	RTC_STREAM_Index,
	RTC_STREAM_index,
	RTC_STREAM_Meta,
	RTC_STREAM_meta,
	RTC_STREAM_Platform,
	RTC_STREAM_Crc,
	RTC_STREAM_DEFAULT_COUNT
};

typedef struct rtc_handle {
	rtc_param const* param;
	struct rtc_stream default_streams[RTC_STREAM_DEFAULT_COUNT];
	struct rtc_stream* first_stream;
	struct rtc_stream* last_stream;
	unsigned int free_id;
	rtc_offset cursor;
	bool meta_changed;
	rtc_offset Unit_end;
	rtc_offset unit_end;
#ifndef RTC_NO_CRC
	crc_t crc;
#endif
} rtc_handle;


void rtc_param_default(rtc_param* param);
int rtc_start(rtc_handle* h, rtc_param const* param);
int rtc_stop(rtc_handle* h);
int rtc_json(rtc_handle* h, rtc_write_callback* cb, bool defaults); /* show full json */

int rtc_open(rtc_handle* h, rtc_stream* s, rtc_stream_param const* param);
int rtc_close(rtc_stream* s); /* may return that it cannot delete it yet */
int rtc_write(rtc_stream* s, void const* buffer, size_t len, bool more);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* __RTC_H */
