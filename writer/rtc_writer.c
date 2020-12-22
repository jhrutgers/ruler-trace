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

#include "rtc_writer.h"

#include <errno.h>
#include <string.h>
#include <assert.h>

/**************************************
 * Configuration
 */

#define RTC_FRAME_MAX_HEADER_SIZE 10
#define RTC_FRAME_MAX_PAYLOAD 1024


/**************************************
 * Utilities
 */

#ifndef MAX
#  define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#  define MIN(a,b)  ((a) < (b) ? (a) : (b))
#endif

#ifndef STRINGIFY
#  define STRINGIFY_(x) #x
#  define STRINGIFY(x) STRINGIFY_(x)
#endif

#ifndef likely
#  ifdef __GCC__
#    define likely(expr) __builtin_expect(!!(expr), 1)
#  else
#    define likely(expr) (expr)
#  endif
#endif

#ifndef unlikely
#  ifdef __GCC__
#    define unlikely(expr) __builtin_expect(!!(expr), 0)
#  else
#    define unlikely(expr) (expr)
#  endif
#endif

#define check_res(call)		{ int res_ = (call); if(unlikely(res_)) return res_; }

#ifdef __GCC__
#  define rtc_popcount(x)	__builtin_popcountl(x)
#else
static int rtc_popcount(size_t x) {
	int cnt = 0;

	for(; x; x >>= 1u)
		if(x & 1u)
			cnt++;

	return cnt;
}
#endif


static size_t rtc_itoa(unsigned int x, char* buf, size_t len) {
	char b[22]; /* 2^64 has 20 decimals, plus \0 and one to check for overflow. */
	size_t i = sizeof(b);

	assert(buf);

	b[--i] = '\0';
	do {
		b[--i] = (char)('0' + (x % 10u));
		x /= 10u;
	} while(x);

	assert(i > 0);
	assert(sizeof(b) - i <= len);
	len = sizeof(b) - i;
	memcpy(buf, &b[i], len);
	return len - 1; /* Without \0 */
}

static size_t rtc_encode_int(rtc_offset x, char* buf) {
	rtc_offset len = 0;

	do {
		*buf = x & 0x7fu;
		x >>= 7u;
		if(x)
			*buf |= (char)0x80u;
		buf++;
		len++;
	} while(x);

	return len;
}

#define RTC_ENCODE_INT_BUF(x)	((sizeof(x) * 8u + 6u) / 7u)



/**************************************
 * Trace
 */

void rtc_param_default(rtc_param* param) {
	if(!param)
		return;

	memset(param, 0, sizeof(*param));
	param->Unit = 1 << 20;
	param->unit = 1 << 17;
}

static rtc_stream_param const rtc_default_stream_param[RTC_STREAM_DEFAULT_COUNT] = {
	{
		/* .name = */ "nop",
		/* .frame_length = */ 0,
		/* .json = */ "name:\"nop\",length:0",
		/* .hidden = */ true
	},
	{
		/* .name = */ "padding",
		/* .frame_length = */ (size_t)-1,
		/* .json = */ "name:\"padding\"",
		/* .hidden = */ true
	},
	{
		/* .name = */ "Marker",
		/* .frame_length = */ RTC_FRAME_MAX_PAYLOAD,
		/* .json = */ "name:\"Marker\",length:" STRINGIFY(RTC_FRAME_MAX_PAYLOAD),
		/* .hidden = */ true
	},
	{
		/* .name = */ "Index",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"Index\",format:\"index\""
	},
	{
		/* .name = */ "index",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"index\",format:\"index\""
	},
	{
		/* .name = */ "Meta",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"Meta\",format:\"json\""
	},
	{
		/* .name = */ "meta",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"meta\",format:\"json\"",
		/* .hidden = */ true
	}
};

int rtc_start(rtc_handle* h, rtc_param const* param) {
	int i;

	if(!h)
		return EINVAL;
	if(!param)
		return EINVAL;
	if(!param->write)
		return EINVAL;
	if(param->Unit < RTC_MIN_UNIT_SIZE)
		return EINVAL;
	if(param->unit < RTC_MIN_UNIT_SIZE)
		return EINVAL;
	if(param->Unit < param->unit)
		return EINVAL;
	if(rtc_popcount(param->Unit) != 1)
		return EINVAL;
	if(rtc_popcount(param->unit) != 1)
		return EINVAL;

	memset(h, 0, sizeof(*h));
	h->param = param;

	for(i = 0; i < RTC_STREAM_DEFAULT_COUNT; i++)
		check_res(rtc_open(h, &h->default_streams[i], &rtc_default_stream_param[i]));

	return 0;
}

int rtc_stop(rtc_handle* h) {
	if(!h)
		return EINVAL;

	h->param->write(h, NULL, 0, RTC_FLAG_STOP | RTC_FLAG_FLUSH);
	return 0;
}


/**************************************
 * JSON
 */

static int rtc_json_stream(rtc_stream* s, rtc_write_callback* cb) {
	check_res(cb(s->h, "{id:", 4, 0));
	check_res(cb(s->h, s->id_str, s->id_str_len, 0));

	if(s->param_json_len) {
		check_res(cb(s->h, ",", 1, 0));
		check_res(cb(s->h, s->param->json, s->param_json_len, 0));
	}

	return cb(s->h, "},", 2, 0);
}

static int rtc_json_(rtc_stream* s, rtc_write_callback* cb, bool defaults) {
	size_t len;
	rtc_handle* h = s->h;
	char buf[sizeof(h->first_stream->id_str)];

	if(!cb)
		return 0;

	check_res(cb(h, "[", 1, 0));

	for(; s; s = s->next)
		if(defaults || s->id >= RTC_STREAM_DEFAULT_COUNT)
			check_res(rtc_json_stream(s, cb));

	len = rtc_itoa(h->free_id, buf, sizeof(buf));
	assert(len < sizeof(buf));
	check_res(cb(h, buf, len, 0));
	check_res(cb(h, "]", 1, RTC_FLAG_FLUSH));

	return 0;
}

int rtc_json(rtc_handle* h, rtc_write_callback* cb, bool defaults) {
	if(!h)
		return EINVAL;

	return rtc_json_(h->first_stream, cb, defaults);
}


/**************************************
 * Stream
 */

static int rtc_meta(rtc_stream* s);

int rtc_open(rtc_handle* h, rtc_stream* s, rtc_stream_param const* param) {
	if(!h)
		return EINVAL;
	if(!s)
		return EINVAL;
	if(!param)
		return EINVAL;
	if(h->free_id && (h->free_id << 1u) == 0)
		return ENOMEM;

	memset(s, 0, sizeof(*s));
	s->param = param;
	s->h = h;
	s->id = h->free_id++;
	s->id_str_len = rtc_itoa(s->id, s->id_str, sizeof(s->id_str));
	assert(s->id_str_len < sizeof(s->id_str));
	if(param->json)
		s->param_json_len = strlen(param->json);

	if(h->cursor != h->Unit_end)
		check_res(rtc_meta(s));

	if(!h->first_stream) {
		h->first_stream = h->last_stream = s;
	} else {
		s->prev = h->last_stream;
		h->last_stream = s->prev->next = s;
	}

	h->meta_changed = true;

	return 0;
}

int rtc_close(rtc_stream* s) {
	if(!s)
		return EINVAL;
	if(!s->h)
		return 0;
	if(s->used)
		return EAGAIN;

	if(s->h->first_stream == s) {
		if(s->h->last_stream == s) {
			s->h->first_stream = s->h->last_stream = NULL;
		} else {
			s->h->first_stream = s->next;
			s->next->prev = NULL;
		}
	} else if(s->h->last_stream == s) {
		s->h->last_stream = s->prev;
		s->prev->next = NULL;
	} else {
		s->prev->next = s->next;
		s->next->prev = s->prev;
	}

	s->h = NULL;
	return 0;
}


/**************************************
 * Common frame functions
 */

static int rtc_write_(rtc_stream* s, void const* buffer, size_t len, bool more, bool stayInUnit);

static int rtc_emit(rtc_handle* h, void const* buffer, size_t len, int flags) {
	rtc_offset new_cursor = h->cursor + len;

	if(!len && !flags)
		return 0;

	if(new_cursor < h->cursor) {
		/* Overflow. */
		return ENOSPC;
	}

	h->cursor = new_cursor;
	return h->param->write(h, buffer, len, flags);
}

static size_t rtc_header(rtc_stream* s, size_t payload, char* hdr, bool more) {
	size_t len = 0;

	len += rtc_encode_int(s->id << 1u, hdr);
	if(more)
		hdr[0] |= 1u;

	if(s->param->frame_length == RTC_STREAM_VARIABLE_LENGTH)
		len += rtc_encode_int(payload, &hdr[len]);

	return len;
}

typedef struct rtc_frame {
	char buffer[RTC_FRAME_MAX_PAYLOAD];
	size_t len;
} rtc_frame;

static rtc_frame frame;

static int rtc_frame_append(rtc_stream* s, rtc_frame* frame, void const* buf, size_t len, int flags) {
	char const* b = (char const*)buf;
	char const* frameBuffer_ = frame->buffer;

	while(len) {
		size_t chunk = MIN(len, sizeof(frame->buffer) - frame->len);
		bool more = !(flags & RTC_FLAG_FLUSH);

		if(unlikely(frame->len == 0 && chunk == len && !more))
			/* No copy needed. */
			frameBuffer_ = b;
		else
			memcpy(frame->buffer + frame->len, buf, chunk);

		frame->len += chunk;
		len -= chunk;
		b += chunk;

		if(frame->len == sizeof(frame->buffer) || !more) {
			check_res(rtc_write_(s, frameBuffer_, frame->len, more, true));
			frame->len = 0;
		}
	}

	return 0;
}


/**************************************
 * Special frames
 */

static char const rtc_padding_buffer[MIN(64, RTC_MIN_UNIT_SIZE)];

static int rtc_padding(rtc_handle* h, size_t len) {
	char hdr[RTC_FRAME_MAX_HEADER_SIZE];
	size_t hdrlen;
	size_t chunk;
	size_t payload;
	rtc_stream* s = &h->default_streams[RTC_STREAM_padding];

	while(len) {
		if(len == 1) {
			/* nul frame is a 0 byte */
			return rtc_emit(h, rtc_padding_buffer, 1, 0);
		}

		assert(s->id <= 0x3f);
		assert(s->param->frame_length == RTC_STREAM_VARIABLE_LENGTH);

		hdr[0] = (char)(s->id << 1u);
		hdrlen = 1u;

		/* Guess the length of rtc_encode_int() */
		chunk = len - 2u; /* id and at least 1-byte length */
		do {
			hdrlen++;
			chunk >>= 7u;
		} while(chunk);

		/* Prepare header. It might become a byte shorter, as len decreased. */
		payload = len - hdrlen;
		hdrlen = 1u + rtc_encode_int(payload, &hdr[1]);
		len -= hdrlen;
		len -= payload;

		check_res(rtc_emit(h, hdr, hdrlen, 0));

		while(payload) {
			chunk = MIN(sizeof(rtc_padding_buffer), payload);
			check_res(rtc_emit(h, rtc_padding_buffer, chunk, 0));
			payload -= chunk;
		}
	}

	return 0;
}

static rtc_frame indexFrame;

static int rtc_index_(rtc_stream* s, bool full) {
	rtc_stream* si;
	rtc_offset here = s->h->cursor;
	rtc_offset since = s->h->default_streams[RTC_STREAM_Index].index;
	char entry[RTC_ENCODE_INT_BUF(s->h->free_id) + RTC_ENCODE_INT_BUF(rtc_offset)];
	size_t entryLen = 0;

	for(si = s->h->first_stream; si; si = si->next) {
		if(!si->param->hidden && (full || si->index >= since)) {
			/* Flush out previous entry. */
			check_res(rtc_frame_append(s, &indexFrame, entry, entryLen, 0));
			/* Assembly next entry. */
			entryLen = rtc_encode_int((si->id << 1u) | 1u, entry);
			entryLen += rtc_encode_int((here - si->index) << 1u, entry + entryLen);
		}
	}

	/* Flush out last entry. */
	assert(entryLen); /* At least the Index should be there. */
	check_res(rtc_frame_append(s, &indexFrame, entry, entryLen, RTC_FLAG_FLUSH));
	return 0;
}

static int rtc_Index(rtc_handle* h) {
	rtc_stream* s = &h->default_streams[RTC_STREAM_Index];
	rtc_offset i = h->cursor;
	h->unit_end = h->cursor + h->param->unit;
	check_res(rtc_index_(s, true));

	if(s->index) {
		/* Update index's index such that it shows up in the Index, but not in
		 * other indexes. */
		s->h->default_streams[RTC_STREAM_index].index = s->index - s->h->param->unit;
	}

	s->index = i;
	return 0;
}

static int rtc_index(rtc_handle* h) {
	rtc_stream* s = &h->default_streams[RTC_STREAM_index];
	h->unit_end = h->cursor + h->param->unit;
	return rtc_index_(s, false);
}

static int rtc_Meta_callback(rtc_handle* h, void const* buf, size_t len, int flags) {
	return rtc_frame_append(&h->default_streams[RTC_STREAM_Meta], &frame, buf, len, flags);
}

static int rtc_Meta(rtc_handle* h) {
	rtc_stream* s = &h->default_streams[RTC_STREAM_Meta];
	rtc_offset i = s->h->meta_changed ? s->h->cursor : s->index;
	assert(frame.len == 0);
	check_res(rtc_json(h, rtc_Meta_callback, false));
	s->index = i;
	s->h->meta_changed = false;
	return 0;
}

static int rtc_meta_callback(rtc_handle* h, void const* buf, size_t len, int flags) {
	return rtc_frame_append(&h->default_streams[RTC_STREAM_meta], &frame, buf, len, flags);
}

static int rtc_meta(rtc_stream* s) {
	assert(frame.len == 0);
	return rtc_json_(s, rtc_meta_callback, false);
}

static rtc_marker const buffer[MIN(16, RTC_MIN_UNIT_SIZE / sizeof(rtc_marker))] = {
	RTC_MARKER, RTC_MARKER, RTC_MARKER, RTC_MARKER,
	RTC_MARKER, RTC_MARKER, RTC_MARKER, RTC_MARKER,
	RTC_MARKER, RTC_MARKER, RTC_MARKER, RTC_MARKER,
	RTC_MARKER, RTC_MARKER, RTC_MARKER, RTC_MARKER};

static int rtc_Marker(rtc_handle* h) {
	char hdr[RTC_FRAME_MAX_HEADER_SIZE];
	size_t hdrlen;
	int flags = RTC_FLAG_NEW_UNIT;
	rtc_stream* s = &h->default_streams[RTC_STREAM_Marker];
	size_t len = s->param->frame_length;

	hdrlen = rtc_header(s, len, hdr, false);

	if(h->cursor == 0)
		flags |= RTC_FLAG_START;

	check_res(rtc_emit(h, hdr, hdrlen, flags));

	while(len) {
		check_res(rtc_emit(h, buffer, sizeof(buffer), 0));
		len -= sizeof(buffer);
	}

	return 0;
}

static int rtc_start_Unit(rtc_handle* h) {
	h->Unit_end = h->cursor + h->param->Unit;

	check_res(rtc_Marker(h));
	check_res(rtc_Index(h));
	check_res(rtc_Meta(h));

	return 0;
}

static int rtc_start_unit(rtc_handle* h) {
	return rtc_index(h);
}


/**************************************
 * Generic frame
 */

static void rtc_set_index(rtc_stream* s) {
	if(!s->index || s->index < s->h->unit_end - s->h->param->unit)
		s->index = s->h->cursor;
}

static int rtc_write_(rtc_stream* s, void const* buffer, size_t len, bool more, bool stayInUnit) {
	char hdr[RTC_FRAME_MAX_HEADER_SIZE];
	size_t hdrlen;
	size_t rem;
	size_t chunklen;
	char const* buffer_ = (char const*)buffer;
	rtc_handle* h = s->h;
	bool first = true;

	if(len == 0)
		return 0;

	while(len) {
		chunklen = MIN(len, rtc_default_stream_param[RTC_STREAM_Marker].frame_length);

		hdrlen = rtc_header(s, chunklen, hdr, more || chunklen != len);
		rem = MIN(h->Unit_end, h->unit_end) - h->cursor;

		if(likely(rem >= hdrlen + chunklen)) {
			/* Frame fits. */
			if(first) {
				rtc_set_index(s);
				first = false;
			}
			check_res(rtc_emit(h, hdr, hdrlen, 0));
			check_res(rtc_emit(h, buffer_, chunklen, 0));
			s->used = true;

			buffer_ += chunklen;
			len -= chunklen;
			continue;
		} else if(rem > hdrlen) {
			/* Write first chunk. */
			chunklen = rem - hdrlen;
			hdrlen = rtc_header(s, chunklen, hdr, true);
			assert(hdrlen + chunklen <= rem);

			if(first) {
				rtc_set_index(s);
				first = false;
			}
			check_res(rtc_emit(h, hdr, hdrlen, 0));
			check_res(rtc_emit(h, buffer_, chunklen, 0));
			s->used = true;

			buffer_ += chunklen;
			len -= chunklen;
			rem -= hdrlen + chunklen;
		}

		check_res(rtc_padding(h, rem));

		assert(h->cursor == h->Unit_end || h->cursor == h->unit_end);

		if(unlikely(h->cursor == h->Unit_end)) {
			if(unlikely(stayInUnit))
				return E2BIG;

			/* Start next Unit and retry the remainder of this frame. */
			check_res(rtc_start_Unit(h));
		} else {
			/* Start next unit and retry the remainder of this frame. */
			check_res(rtc_start_unit(h));
		}
	}

	return 0;
}

int rtc_write(rtc_stream* s, void const* buffer, size_t len, bool more) {
	if(!s)
		return EINVAL;
	if(len == 0)
		return 0;
	if(!buffer)
		return EINVAL;

	return rtc_write_(s, buffer, len, more, false);
}

