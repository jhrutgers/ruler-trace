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
#define RTC_FRAME_MAX_SIZE (RTC_FRAME_MAX_HEADER_SIZE + RTC_FRAME_MAX_PAYLOAD)


/**************************************
 * Utilities
 */

#ifndef MAX
#  define MAX(a,b)	((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#  define MIN(a,b)	((a) < (b) ? (a) : (b))
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

#ifndef RTC_NO_CRC
static crc_t const crc_lookup_table[256] = {
	0x00000000UL, 0x04C11DB7UL, 0x09823B6EUL, 0x0D4326D9UL, 0x130476DCUL, 0x17C56B6BUL, 0x1A864DB2UL, 0x1E475005UL,
	0x2608EDB8UL, 0x22C9F00FUL, 0x2F8AD6D6UL, 0x2B4BCB61UL, 0x350C9B64UL, 0x31CD86D3UL, 0x3C8EA00AUL, 0x384FBDBDUL,
	0x4C11DB70UL, 0x48D0C6C7UL, 0x4593E01EUL, 0x4152FDA9UL, 0x5F15ADACUL, 0x5BD4B01BUL, 0x569796C2UL, 0x52568B75UL,
	0x6A1936C8UL, 0x6ED82B7FUL, 0x639B0DA6UL, 0x675A1011UL, 0x791D4014UL, 0x7DDC5DA3UL, 0x709F7B7AUL, 0x745E66CDUL,
	0x9823B6E0UL, 0x9CE2AB57UL, 0x91A18D8EUL, 0x95609039UL, 0x8B27C03CUL, 0x8FE6DD8BUL, 0x82A5FB52UL, 0x8664E6E5UL,
	0xBE2B5B58UL, 0xBAEA46EFUL, 0xB7A96036UL, 0xB3687D81UL, 0xAD2F2D84UL, 0xA9EE3033UL, 0xA4AD16EAUL, 0xA06C0B5DUL,
	0xD4326D90UL, 0xD0F37027UL, 0xDDB056FEUL, 0xD9714B49UL, 0xC7361B4CUL, 0xC3F706FBUL, 0xCEB42022UL, 0xCA753D95UL,
	0xF23A8028UL, 0xF6FB9D9FUL, 0xFBB8BB46UL, 0xFF79A6F1UL, 0xE13EF6F4UL, 0xE5FFEB43UL, 0xE8BCCD9AUL, 0xEC7DD02DUL,
	0x34867077UL, 0x30476DC0UL, 0x3D044B19UL, 0x39C556AEUL, 0x278206ABUL, 0x23431B1CUL, 0x2E003DC5UL, 0x2AC12072UL,
	0x128E9DCFUL, 0x164F8078UL, 0x1B0CA6A1UL, 0x1FCDBB16UL, 0x018AEB13UL, 0x054BF6A4UL, 0x0808D07DUL, 0x0CC9CDCAUL,
	0x7897AB07UL, 0x7C56B6B0UL, 0x71159069UL, 0x75D48DDEUL, 0x6B93DDDBUL, 0x6F52C06CUL, 0x6211E6B5UL, 0x66D0FB02UL,
	0x5E9F46BFUL, 0x5A5E5B08UL, 0x571D7DD1UL, 0x53DC6066UL, 0x4D9B3063UL, 0x495A2DD4UL, 0x44190B0DUL, 0x40D816BAUL,
	0xACA5C697UL, 0xA864DB20UL, 0xA527FDF9UL, 0xA1E6E04EUL, 0xBFA1B04BUL, 0xBB60ADFCUL, 0xB6238B25UL, 0xB2E29692UL,
	0x8AAD2B2FUL, 0x8E6C3698UL, 0x832F1041UL, 0x87EE0DF6UL, 0x99A95DF3UL, 0x9D684044UL, 0x902B669DUL, 0x94EA7B2AUL,
	0xE0B41DE7UL, 0xE4750050UL, 0xE9362689UL, 0xEDF73B3EUL, 0xF3B06B3BUL, 0xF771768CUL, 0xFA325055UL, 0xFEF34DE2UL,
	0xC6BCF05FUL, 0xC27DEDE8UL, 0xCF3ECB31UL, 0xCBFFD686UL, 0xD5B88683UL, 0xD1799B34UL, 0xDC3ABDEDUL, 0xD8FBA05AUL,
	0x690CE0EEUL, 0x6DCDFD59UL, 0x608EDB80UL, 0x644FC637UL, 0x7A089632UL, 0x7EC98B85UL, 0x738AAD5CUL, 0x774BB0EBUL,
	0x4F040D56UL, 0x4BC510E1UL, 0x46863638UL, 0x42472B8FUL, 0x5C007B8AUL, 0x58C1663DUL, 0x558240E4UL, 0x51435D53UL,
	0x251D3B9EUL, 0x21DC2629UL, 0x2C9F00F0UL, 0x285E1D47UL, 0x36194D42UL, 0x32D850F5UL, 0x3F9B762CUL, 0x3B5A6B9BUL,
	0x0315D626UL, 0x07D4CB91UL, 0x0A97ED48UL, 0x0E56F0FFUL, 0x1011A0FAUL, 0x14D0BD4DUL, 0x19939B94UL, 0x1D528623UL,
	0xF12F560EUL, 0xF5EE4BB9UL, 0xF8AD6D60UL, 0xFC6C70D7UL, 0xE22B20D2UL, 0xE6EA3D65UL, 0xEBA91BBCUL, 0xEF68060BUL,
	0xD727BBB6UL, 0xD3E6A601UL, 0xDEA580D8UL, 0xDA649D6FUL, 0xC423CD6AUL, 0xC0E2D0DDUL, 0xCDA1F604UL, 0xC960EBB3UL,
	0xBD3E8D7EUL, 0xB9FF90C9UL, 0xB4BCB610UL, 0xB07DABA7UL, 0xAE3AFBA2UL, 0xAAFBE615UL, 0xA7B8C0CCUL, 0xA379DD7BUL,
	0x9B3660C6UL, 0x9FF77D71UL, 0x92B45BA8UL, 0x9675461FUL, 0x8832161AUL, 0x8CF30BADUL, 0x81B02D74UL, 0x857130C3UL,
	0x5D8A9099UL, 0x594B8D2EUL, 0x5408ABF7UL, 0x50C9B640UL, 0x4E8EE645UL, 0x4A4FFBF2UL, 0x470CDD2BUL, 0x43CDC09CUL,
	0x7B827D21UL, 0x7F436096UL, 0x7200464FUL, 0x76C15BF8UL, 0x68860BFDUL, 0x6C47164AUL, 0x61043093UL, 0x65C52D24UL,
	0x119B4BE9UL, 0x155A565EUL, 0x18197087UL, 0x1CD86D30UL, 0x029F3D35UL, 0x065E2082UL, 0x0B1D065BUL, 0x0FDC1BECUL,
	0x3793A651UL, 0x3352BBE6UL, 0x3E119D3FUL, 0x3AD08088UL, 0x2497D08DUL, 0x2056CD3AUL, 0x2D15EBE3UL, 0x29D4F654UL,
	0xC5A92679UL, 0xC1683BCEUL, 0xCC2B1D17UL, 0xC8EA00A0UL, 0xD6AD50A5UL, 0xD26C4D12UL, 0xDF2F6BCBUL, 0xDBEE767CUL,
	0xE3A1CBC1UL, 0xE760D676UL, 0xEA23F0AFUL, 0xEEE2ED18UL, 0xF0A5BD1DUL, 0xF464A0AAUL, 0xF9278673UL, 0xFDE69BC4UL,
	0x89B8FD09UL, 0x8D79E0BEUL, 0x803AC667UL, 0x84FBDBD0UL, 0x9ABC8BD5UL, 0x9E7D9662UL, 0x933EB0BBUL, 0x97FFAD0CUL,
	0xAFB010B1UL, 0xAB710D06UL, 0xA6322BDFUL, 0xA2F33668UL, 0xBCB4666DUL, 0xB8757BDAUL, 0xB5365D03UL, 0xB1F740B4UL};

static crc_t rtc_crc_start() {
	return 0xffffffffUL;
}

static crc_t rtc_crc(crc_t crc, void const* buffer, size_t len) {
	char const* buf = (char const*)buffer;
	for(; len; len--, buf++)
		crc = (crc >> 8u) ^ crc_lookup_table[(crc ^ (crc_t)(unsigned char)*buf) & 0xffu];
	return crc;
}

static crc_t rtc_crc_end(crc_t crc) {
	return crc ^ 0xffffffffUL;
}
#endif


/**************************************
 * Stream config
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
		/* .json = */ "name:\"Index\",format:\"index\"",
		/* .hidden = */ false
	},
	{
		/* .name = */ "index",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"index\",format:\"index\"",
		/* .hidden = */ false
	},
	{
		/* .name = */ "Meta",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"Meta\",format:\"json\"",
		/* .hidden = */ false
	},
	{
		/* .name = */ "meta",
		/* .frame_length = */ RTC_STREAM_VARIABLE_LENGTH,
		/* .json = */ "name:\"meta\",format:\"json\"",
		/* .hidden = */ true
	},
	{
		/* .name = */ "Platform",
		/* .frame_length = */ sizeof(crc_t),
		/* .json = */ "name:\"Platform\",format:\"platform\"",
		/* .hidden = */ false
	},
	{
		/* .name = */ "Crc",
		/* .frame_length = */ sizeof(crc_t),
		/* .json = */ "name:\"Crc\",format:\"uint32\"",
		/* .hidden = */ false
	}
};



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

int rtc_create(rtc_handle* h, rtc_stream* s, rtc_stream_param const* param) {
	rtc_stream* es;

	if(!h)
		return EINVAL;
	if(!s)
		return EINVAL;
	if(!param)
		return EINVAL;
	if(h->free_id && (h->free_id << 1u) == 0)
		return ENOMEM;

	for(es = h->first_stream; es; es = es->next)
		if(strcmp(es->param->name, param->name) == 0)
			return EEXIST;

	memset(s, 0, sizeof(*s));
	s->param = param;
	s->h = h;
	s->open = 1;
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

int rtc_open(rtc_handle* h, rtc_stream** s, char const* name) {
	if(!h)
		return EINVAL;
	if(!s)
		return EINVAL;
	if(!name || !*name)
		return EINVAL;

	for(*s = h->first_stream; *s; *s = (*s)->next)
		if(strcmp((*s)->param->name, name) == 0) {
			(*s)->open++;
			return 0;
		}

	return ESRCH;
}

int rtc_close(rtc_stream* s) {
	if(!s)
		return EINVAL;
	if(s->open == 0)
		/* Already closed. */
		return 0;
	if(s->open > 1) {
		/* Do not really close yet. */
		s->open--;
		return 0;
	}
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
#ifndef RTC_NO_CRC
	h->crc = rtc_crc(h->crc, buffer, len);
#endif
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
	rtc_stream* s = &h->default_streams[RTC_STREAM_padding];

	while(len) {
		size_t hdrlen;
		size_t chunk;
		size_t payload;

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

	if(full) {
		entryLen = rtc_encode_int(s->h->Unit_count, entry);
		check_res(rtc_frame_append(s, &indexFrame, entry, entryLen, 0));
		entryLen = 0;
	}

	for(si = s->h->first_stream; si; si = si->next) {
		if(si->param->hidden)
			continue;
		if(!si->param->hidden && (full || si->index >= since)) {
			/* Flush out previous entry. */
			check_res(rtc_frame_append(s, &indexFrame, entry, entryLen, 0));
			/* Assemble next entry. */
			entryLen = rtc_encode_int((si->id << 1u) | 1u, entry);
			entryLen += rtc_encode_int(si->index ? (here - si->index) << 1u : 0, entry + entryLen);
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

	if(unlikely(!s->index)) {
		/* First Index, reflect params in Index's and index's index,
		 * even though they point to before the beginning of the file. */
		s->index = h->cursor - h->param->Unit;
		h->default_streams[RTC_STREAM_index].index = h->cursor - h->param->unit;
	}

	check_res(rtc_index_(s, true));

	s->index = i;
	return 0;
}

static int rtc_index(rtc_handle* h) {
	rtc_stream* s = &h->default_streams[RTC_STREAM_index];
	rtc_offset i = h->cursor;

	/* Update index such that it it does not show up in the index. */
	if(likely(s->index))
		s->index = s->h->default_streams[RTC_STREAM_Index].index - 1;

	check_res(rtc_index_(s, false));

	s->index = i;
	return 0;
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

#ifndef RTC_NO_CRC
#  define RTC_FRAME_CRC_SIZE 5
static int rtc_Crc(rtc_handle* h) {
	crc_t crc = rtc_crc_end(h->crc);
	rtc_stream* s = &h->default_streams[RTC_STREAM_Crc];

	return rtc_write_(s, &crc, sizeof(crc), false, true);
}
#endif /* !RTC_NO_CRC */

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

#ifndef RTC_NO_CRC
	h->crc = rtc_crc_start();
#endif
	return 0;
}

static int rtc_Platform(rtc_handle* h) {
	crc_t x = 0x01020304;
	rtc_stream* s = &h->default_streams[RTC_STREAM_Platform];

	assert(sizeof(x) == 4u);
	return rtc_write_(s, &x, sizeof(x), false, false);
}

static int rtc_start_Unit(rtc_handle* h) {
	h->Unit_end = h->cursor + h->param->Unit;
#ifndef RTC_NO_CRC
	h->Unit_end -= RTC_FRAME_CRC_SIZE;
#endif

	if(likely(h->cursor > 0)) {
#ifndef RTC_NO_CRC
		check_res(rtc_Crc(h));
#endif
		h->Unit_count++;
		if(unlikely(h->Unit_count == 0u))
			return ENOMEM;
	}

	check_res(rtc_Marker(h));
	h->unit_end = h->cursor + h->param->unit;
	check_res(rtc_Index(h));
	check_res(rtc_Meta(h));
	check_res(rtc_Platform(h));

	return 0;
}

static int rtc_start_unit(rtc_handle* h) {
	h->unit_end = h->cursor + h->param->unit;
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
	char const* buffer_ = (char const*)buffer;
	rtc_handle* h = s->h;
	bool first = true;

	if(len == 0)
		return 0;

	while(len) {
		size_t chunklen = MIN(len, rtc_default_stream_param[RTC_STREAM_Marker].frame_length);
		size_t hdrlen = rtc_header(s, chunklen, hdr, more || chunklen != len);
		size_t rem = MIN(h->Unit_end, h->unit_end) - h->cursor;

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


/**************************************
 * Trace
 */

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
	if(param->unit <= RTC_FRAME_MAX_SIZE)
		/* A unit must be able to contain a Marker and Index. */
		return EINVAL;

	memset(h, 0, sizeof(*h));
	h->param = param;

	for(i = 0; i < RTC_STREAM_DEFAULT_COUNT; i++)
		check_res(rtc_create(h, &h->default_streams[i], &rtc_default_stream_param[i]));

	return 0;
}

int rtc_stop(rtc_handle* h) {
	if(!h)
		return EINVAL;

#ifndef RTC_NO_CRC
	if(h->cursor > 0)
		rtc_Crc(h);
#endif

	h->param->write(h, NULL, 0, RTC_FLAG_STOP | RTC_FLAG_FLUSH);
	return 0;
}

