# RTC file format

The Ruler Trace Container is like a ruler:

	----- 0      ^
	-- 0.1       |
	-- 0.2   (major) Unit
	-- 0.3       |
	----- 1      v
	-- 1.1
	-- 1.2   <-- (minor) unit
	-- 1.3
	----- 2
	-- 2.1
	-- 2.2
	-- 2.3
	----- 3  <-- Marker, with Index and Meta
	-- 3.1   <-- index
	-- 3.2
	-- 3.3
	----- 4
	...

What is measures is the RTC file size, so at (configurable, but) predictable
positions in the file, a `Unit` and `unit` boundary exists. At the start of a
`Unit`, `Meta` data is repeated, a full `Index` exists, and other details are
injected in the file. A `unit` is like a delta on the previous `Unit` and/or
`unit`. Because the reader knows where it can find the index of every unit,
parsing and searching the file is fast, as only the part of the file is to be
loaded and parsed that is of interest.

A file may be truncated, and remains readable, as there is no special beginning
or end of the file which contains things like a pointers to data structures.
`Unit` boundaries can be recognized, wherever you start reading the file.
Moreover, a `Unit` is protected by a CRC.

In between the `Unit` and `unit` indices, frames are injected, which hold the
actual data.  A frame has a maximum size. If larger data is to be embedded,
multiple consecutive frames should be concatenated while reading it. For example:

	----- 4       Marker frame  // fixed size
	              Index frame   // variable size, but must fit within unit
	              Meta frame    // variable size
	              data frame    // configured as fixed or variable size
	              data frame
	-- 4.1        index frame   // variable size, but must fit within unit
	              data frame
	              data frame
	              data frame
	-- 4.2        index frame
	              data frame    // Data is longer than maximum frame length.
	              data frame (continued)
	              data frame (continued)
	-- 4.3        index frame   // Even the index may be in between.
	              data frame (continued)
	              data frame
	              padding       // To align the Marker, padding may be required.
	---- 5        Marker frame
	              Index frame
	...

The data frame belong to a stream, which can be a stdout stream, measurement
data stream, event stream, etc.  Multiple streams are freely multiplexed.

## File format grammar

EBNF:

	rtc = { Unit } ;
	Unit = Marker, Index, Meta, { frame }, { unit }, [ Crc ];
	unit = index, frame, { frame } ;
	Marker = { padding }, frame<Marker> ;
	Index = frame<Index>, { frame<Index> } ;
	Meta = frame<Meta>, index, frame<Meta> } ;
	Crc = frame<Crc> ;
	index = { padding }, frame<index>, { frame<index> } ;
	padding = frame<nul> | frame<padding> ;
	frame<type> = id, [ length ], payload ;
	id = int ;              (* LSb: 'more': indicates that the following frame should appended *)
	int = byte, { byte } ;  (* Unsigned LEB128 *)
	length = int ;          (* length of payload in bytes *)
	payload = { byte } ;

`Marker` marks the tick of the major units (10 MB?), filled with repeating
magic words (which includes file format version) to resync corrupt file. No
frame can be larger than the `Marker` frame. If a chunk of data looks like a
`Marker` frame, it is one, as it is not possible to embed a `Marker` in any
other frame.

`Index` and `index` mark the tick of the minor units (100 KB?).  `Index` is a
ordered list of all frame types and the byte offset from start of the index
frame to the start of the first occurrence in the last `unit`, except for
`nul`, `padding`, and `meta`, which are never in the index. `Meta` points to
the previous `Meta` that was _different_ from the current one.  `index` is a
ordered list of all frame types which has a changed offset with respect to the
last `Index`.

`Meta` holds all used frame types of the previous `Unit`. Unused entries are
discarded, but IDs are never reused. If during a `Unit` a new stream is added,
`meta` describes the difference. The last value in the `Meta` or `meta` array
indicates the id that is free to use.

`Crc` holds a CRC32-IEEE of all bytes starting after the last `Marker` till the
start of the `Crc` frame.

## Meta stream

A meta stream is a JSON format with an array of objects, and the next free id
as last element in the array. If not specified otherwise, these are the default
properties of every object:

	{
		id: null,
		name: null,
		length: null,
		cont: false,
		format: "raw"
	}

The special frames, which always exist, and are therefore omitted from the
`Meta`, are these:

	{
		id: 0,
		name: "nul",
		length: 0
	},
	{
		id: 1,
		name: "padding"
	},
	{
		id: 2,
		name: "Marker",
		length: 1024,
	},
	{
		id: 3,
		name: "Index",
		format: "index"
	},
	{
		id: 4,
		name: "index",
		format: "index"
	},
	{
		id: 5,
		name: "Meta",
		format: "json"
	},
	{
		id: 6,
		name: "meta",
		format: "json"
	},
	{
		id: 7,
		name: "platform",
		format: "platform"
	},
	{
		id: 8,
		name: "Crc",
		format: "uint32"
	}

The application could define objects, such as:

	{
		name: "stdout",
		cont: true,
		format: "utf-8"
	},
	{
		name: "screenshot",
		format: "png"
	},
	{
		name: "sample",
		format: "uint16le",
		foo: "bar"
	},
	{
		name: "foo",
		format: "bar"
	}

Any additional attributes can be added to the object definition, which passed
to the reader while decoding.

## Default formats

Any format can be defined in the JSON definition. If it is one of the default
formats, any tool can decode it. Otherwise, the reader must supply a decoding
function for it.

### raw

Data, not to be processed.

### index

EBNF:

	index = [ count ], { entry } ;
	count = int ;
	entry = index_id, index_offset ;
	index_id = int ;      (* (id << 1 | 1) *)
	index_offset = int ;  (* offset << 1 *)

The index id has the LSb set to 1, where the offset never has. Therefore, a
binary search through the index is possible, even if the entry length is
unknown; decode the `int`, check the LSb to determine where you are in the
entry and compensate for that.

`count` indicates the `Unit` count from the beginning of the file. The `Index`
always has the `count`, the `index` never has.

### json

JSON format.

### u?int(8|16|32|64)(|le|be)

A signed or unsigned int, with specified bit length and endianness. If no
endianness is provided, native endianness of the writer is assumed.  If the
object has a gain and/or offset field, they are applied to the value in the
frame while decoding.

### float(32|64)(|le|be)

A floating point with specified bit length and endianness. If no endianness is
provided, native endianness of the writer is assumed. If the object has a gain
and/or offset field, they are applied to the value in the frame while decoding.

### u?leb128

Signed or Unsigned LEB128. If the object has a gain and/or offset field, they
are applied to the value in the frame while decoding.

### timespec

A `uint64` and `uint32` field resembling a `struct timespec`, with the
meaning as populated by `timespec_get(..., TIME_UTC)` (C11).

### utf-8

UTF-8 string.

### platform

Platform details. For now, it only contains the value 0x01020304 in the
platform endianness. This frame may be extended in the future for more
information.

### annotate/.\*

Annotation. The format of the frame is the clock as `timespec`, and the
remainder of the frame is the type as indicated after `annotate/`. If the
annotation's `clock` field refers to a clock that has no relation to the Epoch,
the `timespec` value is relative to the related stream's clock.

## Timestamp

Every frame belongs to the last specified timestamp frame.
For this, the writer must specify a clock object, such as:

	{
		name: "clk",
		clock: true,
		type: "timespec"
	}

Note the `clock` field set to `true`.

This is an absolute timestamp in seconds since the Epoch.  However, the
application may have a different clock source, such as a cycle counter.  In
that case, one can define a 100 MHz clock like this:

	{
		name: "clk",
		clock: true,
		type: "uint64le",
		gain: 1e-6
	}

A clock must be increasing, a wrap-around is not allowed.  Note that the offset
is undefined, so it is not known what the relation to the Epoch is. It is up to
the reader how to handle that, such as ask the user or visualize it
differently.

Multiple clocks may exist in the trace simultaneously. An object has to
indicate to which clock it belongs. To do this, it must set its `clock` field
to the name of the (previously defined) clock object. If `clock` is not set (or
`null`), the frame does not belong to any clock. For example, to insert
arbitrary strings as events, like log entries:

	{
		name: "event",
		clock: "clk",
		type: "utf-8"
	}

The relation between different clocks is undefined. The relation may be
deducted from the fact that they are both in the same trace, but the file
format does not enforce it.

A timestamp will be in the trace very often. If the interval is short, it is a
waste of space to repeat the full timestamp if it is almost the same as the
last time. For this, a clock delta can be defined:

	{
		name: "clk delta",
		clock: true,
		type: "uleb128",
		gain: 1e-6,
		delta: "clk"
	}

The `delta` field determines that it is a delta on another clock.

This is a new clock, which is a delta on `clk` in seconds (after conversion to
a `timespec`), encoded as Unsigned LEB128.  Now, objects can use `clk delta` as
their clock instead of `clk`. The trace file only has to have the actual `clk`
timestamp once in a while (like every minute), and emit `clk delta` frames for
updates.

For example, assume `sample` has set its `clock` to `clk delta`, the trace file
may contain the following sequence now:

	frame<clk>          // E.g. 14:15:30.556
	frame<clk delta>    // E.g. +.100
	frame<sample>       // Effective timestamp: 14:15:30.656
	frame<sample>       // Idem
	frame<clk delta>    // E.g. +.200
	frame<sample>       // Effective timestamp: 14:15:30.756
	frame<clk delta>    // E.g. +.300
	frame<clk>          // E.g. 14:15:31.034
	frame<sample>       // clk delta was not updated yet, so 14:15:30.856
	frame<clk delta>
	...

## Annotation

To insert an annotation to a specific stream, add a stream like:

	{
		name: "remarks",
		stream: "stdout",
		type: "annotate/utf-8"
	}

Note the `stream` field, which refers to another stream. If omitted, the
annotation is placed on the given timestamp. The frame will hold a timestamp,
which is related to `stdout`'s clock, followed by a string.

A `remarks` frame is appended to the RTC file. The frame itself has no relation
to a clock (the `clock` field is not set), but adds some note to the past. So
the position in the RTC file is irrelevant.

Annotations can be used by the user to add comments to events in the trace, but
also to show when an application started, or what the version is of the
software or hardware.  It is up to the application how this information is
formatted.

