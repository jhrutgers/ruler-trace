#ifndef RTC_WRITER_H
#define RTC_WRITER_H
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
#  if !defined(bool) && !defined(__cplusplus)
#    ifdef RTC_HAVE_STDBOOL_H
#      include <stdbool.h>
#    else
typedef enum { false, true } rtc_bool;
#      define bool char
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
#define RTC_MARKER_BLOCK 1024

struct rtc_handle;

typedef int (rtc_write_callback)(struct rtc_handle* h, void const* buf, size_t len, int flags);

enum {
	RTC_MIN_UNIT_SIZE = 64
};

typedef struct rtc_param {
	/*! \brief Size of Unit in bytes. Must be a power of 2. */
	size_t Unit;
	/*! \brief Size of unit in bytes. Must be a power of 2. */
	size_t unit;
	/*! \brief Callback to receive frame data. */
	rtc_write_callback* write;
	/*! \brief User-defined value. */
	void* arg;
} rtc_param;

#define RTC_STREAM_VARIABLE_LENGTH ((size_t)-1)

typedef struct rtc_stream_param {
	/*! \brief Name of the stream. Must be unique. */
	char const* name;

	/*!
	 * \brief Fixed length of this frame.
	 *
	 * Set to #RTC_STREAM_VARIABLE_LENGTH have a variable-length frame.
	 */
	size_t frame_length;

	/*!
	 * \brief All fields (except \c id) that defines the stream.
	 *
	 * Only define the body of a JSON object (without { and } ).
	 * Also provide \c name and \c frame_length, even though this is redundant with the
	 * other fields in this struct.
	 */
	char const* json;

	/*!
	 * \brief If \c true, do not add this stream to the Meta's JSON output.
	 *
	 * Should be \c false for normal streams.
	 */
	bool hidden;
} rtc_stream_param;

typedef struct rtc_stream {
	rtc_stream_param const* param;
	struct rtc_handle* h;
	unsigned int open;
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
	rtc_offset Unit_count;
	bool meta_changed;
	rtc_offset Unit_end;
	rtc_offset unit_end;
#ifndef RTC_NO_CRC
	crc_t crc;
#endif
} rtc_handle;

/*!
 * \def
 * \brief Return the user-supplied \c arg from a #rtc_handle.
 */
#define rtc_arg(h)	((h) ? (h)->param->arg : NULL)



/*!
 * \brief Initialize with default parameters.
 * \param param the parameters to be initialized
 */
void rtc_param_default(rtc_param* param);

/*!
 * \brief Start a new RTC file.
 * \param h the handle to initialize
 * \param param the parameters to use to initialize \p h
 * \return 0 on success, otherwise an errno. In case of all errors, the RTC is not opened and \p h is left in undefined state.
 */
int rtc_start(rtc_handle* h, rtc_param const* param);

/*!
 * \brief Stop and close a RTC file.
 * \param h the handle to close
 * \return 0 on success, otherwise an errno. In any case, \p h should not be used afterwards.
 */
int rtc_stop(rtc_handle* h);

/*!
 * \brief Pass the meta JSON to the given callback.
 * \param h the RTC to get the meta data from, should be opened
 * \param cb the callback to call
 * \param defaults if \c true, also pass the default streams to \p cb
 * \return 0 on success, otherwise an errno.
 */
int rtc_json(rtc_handle* h, rtc_write_callback* cb, bool defaults
#ifdef __cplusplus
	= true
#endif
	);

/*!
 * \brief Create a new stream.
 *
 * Streams can be opened multiple times. For every #rtc_create() and #rtc_open(), call #rtc_close().
 *
 * \param h the RTC, should be opened
 * \param s the stream, which will be initialized
 * \param param the parameters of the new stream
 * \return 0 on success, otherwise an errno. In case of all errors, \p s is left in an undefined state.
 */
int rtc_create(rtc_handle* h, rtc_stream* s, rtc_stream_param const* param);

/*!
 * \brief Open an existing stream.
 *
 * Streams can be opened multiple times. For every #rtc_create() and #rtc_open(), call #rtc_close().
 *
 * \param h the RTC, should be opened
 * \param s will receive the stream pointer
 * \param name the name of the stream to be opened
 * \return 0 on success, otherwise an errno.
 */
int rtc_open(rtc_handle* h, rtc_stream** s, char const* name);

/*!
 * \brief Close an open stream.
 *
 * Streams can opened by either #rtc_create() or #rtc_open().
 *
 * \param s the stream to close
 * \return 0 on success, otherwise an errno. In case of an error, the stream configuration is not affected.
 */
int rtc_close(rtc_stream* s); /* may return that it cannot delete it yet */

/*!
 * \brief Write to a stream.
 * \param s the stream to write to, must be open
 * \param buffer the frame's payload
 * \param len the length of the data in \p buffer
 * \param more if \c true, call #rtc_write() again to add more data to the previous frame(s).
 *             Make sure to call #rtc_write() the last time with \p more set to \c false.
 * \return 0 on success, otherwise an errno. If an error is returned, the RTC
 *         may be left in an undefined state. Advice to #rtc_stop() and restart.
 */
int rtc_write(rtc_stream* s, void const* buffer, size_t len, bool more
#ifdef __cplusplus
	= false
#endif
	);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* RTC_WRITER_H */
