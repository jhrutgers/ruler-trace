# Ruler Trace Container

**Work in progress...**

A file format to save multiple streams of trace to.
It has a few interesting properties:

- Writing the format is fast.
- One stream contains meta data that describes all streams.
- Trace data in the file is in chronological order.
- If the first or last part of the file is missing, trace samples can still be
  recovered (meta data is repeated once in a while). So, if you connect to a
  system that produces traces, you can just start in the middle of it.
- Searching through the file is fast, as the file structure is such that a
  reader can easily jump through the file without parsing to much data
  structures; you don't have to read the whole file to seek to the interesting
  part.
- The stream is written once; the writer just produces a stream of data, and
  never goes back to update pointers in the internal data structure, for
  example.
- File size is practically unlimited.
- The writer does not do dynamic memory, and is written in ANSI C.
- The writer can be included in your project by just copying and compiling
  `writer/rtc_writer.*`.

The reader is work in progress...

The data structure is more or less like a ruler: you have a major unit (10 MB
in file size), which is subdivided in minor units (100 KB). At every unit
boundary, there is a index table in the file that tells you what data there is
around it, or where the last data of a specific stream can be found. Therefore,
search through the file is easy, if you know the position of one such index,
the rest can be predicted.

Synchronizing to this ruler unit sequence is done by a specific marker frame at
every major unit. From there, the unit size and meta data can be read. To find
a specific sample, a binary search can be done through the indices.

Streams are split in frames. Frames of different streams can be multiplexed
freely. If a frame contains one sample, let's say a 32-bit integer, the
overhead of writing this frame is one byte, which contains the identifier of
the stream.

