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

#ifdef __cplusplus
extern "C" {
#endif


/*
trace ::= Unit*
Unit ::= Marker Index Meta unit*
Marker ::= frame<Marker>
Index ::= frame<Index>
Meta ::= (frame<Meta> padding? index)+
padding ::= nul | frame<padding>
index ::= frame<index>
unit ::= frame+ padding? index

frame<type> ::= id length? payload
id ::= int       # LSb: 'more': indicates that the following frame should appended
int ::= byte+    # Unsigned LEB128
length ::= int   # length of payload in bytes
payload ::= byte*

Marker marks the tick of the major units (10 MB?), filled with repeating magic
words (which includes file format version) to resync corrupt file. No frame can
be larger than the Marker frame. If a chunk of data looks like a marker frame,
it is one, as it is not possible to embed a Marker in any other frame.

Index and index mark the tick of the minor units (100 KB?)
Index is a ordered list of all frame types and the offset to its first occurrence in the last unit,
except for nul, padding, and meta (which are never in the index).
Meta points to the previous Meta that was different from the current one.
index is a ordered list of all frame types which has a changed offset w.r.t. the last Index.

Meta holds all used frame types of the previous Unit. Unused entries are discarded, but IDs are never reused.
The last value in the Meta/meta array indicates the id that is free to use.






handling:

view all:
	- seek to next marker
	- seek to next meta
	- parse meta
	- process frames

view specific type:
	- seek to next marker
	- seek to next meta
	- parse meta
	- binary search in rest of file until index found with valid type offset
	- binary search back to index that does not hold the type offset (so, before it starts)
	- progress frames
	- at end of current index interval, jump to next index
		- if in index, process frames in previous interval
		- if not in index, exponential back-off jumping next indices

find time index:
	- seek to next marker
	- seek to next meta
	- parse meta
	- binary search in file based on index frames to find data frames with given timestamp

default:
{
	id: null,
	name: null,
	length: null,
	cont: false,
	mime: "application/octet-stream"
}


meta stream:

[
# implicit
{
# Always one byte
	id: 0,
	name: "nul",
	length: 0,
},
{
	id: 1,
	name: "padding"
},
{
	id: 2,
	name: "Marker",
	length: 1024,
	mime: "application/x-marker"
},
{
	id: 3,
	name: "Index"
	mime: "application/x-index"
},
{
	id: 4,
	name: "index"
	mime: "application/x-index"
}
{
	id: 5,
	name: "Meta",
	mime: "application/json"
},
{
	id: 6,
	name: "meta"
	mime: "application/json"
},
7 # indicates next free id


# Application-defined
{
	id: 7,
	name: "stdout",
	cont: true,
	mime: "text/plain"
},
{
	id: 8,
	name: "image",
	mime: "image/png"
},
9, # next free id
]


application/x-marker
	Repeating magic word. The magic word determines the file format version. If a tool supports two different file format versions,
	it should scan for two words.

application/x-index
	index ::= idsize entry*
	idsize ::= int # in bits
	entry ::= int  # shifted offset, such that the id (with size idsize) is in the LSb

	Binary search in index is possible: start somewhere in the middle, seek backwards till MSb is 0,
	decode int after current byte. This includes the id. Repeat on smaller chunks.

*/




#include <stddef.h>
#include <stdbool.h>

#if __STDC_VERSION__ > 199901L && defined(RTC_64BIT_SIZE)
typedef unsigned long long rtc_offset;
#else
typedef size_t rtc_offset;
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
