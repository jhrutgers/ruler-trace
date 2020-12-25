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

#include "rtc/reader.h"

namespace rtc {

Reader::Reader()
{
}

Reader::~Reader() {
	try {
		close();
	} catch(Exception&) {
	}
}

void Reader::open(char const* filename) {
	close();

	if(!filename)
		throw std::invalid_argument("filename");

	FILE* f = fopen(filename, "rb");
	if(!f)
		throw Exception(errno, "Cannot open '%s';", filename);

	m_file = f;
}

void Reader::open(FILE* f) {
	close();

	if(f)
		throw std::invalid_argument("f");

	m_file = f;
	seek(0, SEEK_SET);
}

#ifdef _POSIX_C_SOURCE
void Reader::open(int fd) {
	close();

	FILE* f = fdopen(fd, "rb");
	if(!f)
		throw Exception(errno, "Cannot open fd %d", fd);

	m_file = f;
	seek(0, SEEK_SET);
}
#endif

void Reader::close() {
	if(!m_file)
		return;

	if(fclose(m_file) == EOF)
		throw Exception(errno, "Cannot close file");

	m_file = nullptr;
}

FILE* Reader::file() const {
	return m_file;
}

bool Reader::isOpen() const {
	return file() != nullptr;
}

Reader::Offset Reader::pos() {
	if(!isOpen())
		throw Exception("File is not open");

#ifdef WIN32
	intptr_t res = (intptr_t)_ftelli64(file());
#else
	intptr_t res = (intptr_t)ftell(file());
#endif

	if(res == -1) {
		int e = errno;
		close();
		throw Exception(e, "Cannot get current file position");
	}

	return res;
}

void Reader::seek(Reader::Offset offset, int whence) {
	if(!isOpen())
		throw SeekError("File is not open");

#ifdef WIN32
	if(_fseeki64(file(), offset, whence))
		throw SeekError(errno);
#else
	if(fseek(file(), (long)offset, whence))
		throw SeekError(errno);
#endif
}

Cursor Reader::cursor() {
	return Cursor(*this);
}

size_t Reader::read(Offset offset, void* dst, size_t len) {
	if(!isOpen())
		throw Exception("File is not open");
	if(!len)
		return 0;
	if(!dst)
		throw std::invalid_argument("dst");

	if(m_offset != offset) {
		seek(offset, SEEK_SET);
		m_offset = offset;
	}

	size_t res = fread(dst, 1, len, file());
	m_offset += res;
	if(res < len) {
		if(ferror(file())) {
			clearerr(file());
			throw Exception("Cannot read file");
		}
	}

	return res;
}

uint64_t Reader::readInt(Offset offset) {
}






Stream::Stream(int id, char const* name, size_t frameLength, bool cont)
	: m_id(id), m_name(name), m_frameLength(frameLength), m_cont(cont)
{
}

int Stream::id() const {
	return m_id;
}

std::string const& Stream::name() const {
	return m_name;
}

bool Stream::isFixedLength() const {
	return frameLength() != VariableLength;
}

bool Stream::isVariableLength() const {
	return frameLength() == VariableLength;
}

size_t Stream::frameLength() const {
	return m_frameLength;
}

bool Stream::cont() const {
	return m_cont;
}

Stream const defaultStreams[RTC_STREAM_DEFAULT_COUNT] = {
	Stream(RTC_STREAM_nop, "nop", 0),
	Stream(RTC_STREAM_padding, "padding", Stream::VariableLength),
	Stream(RTC_STREAM_Marker, "Marker", Reader::MarkerBlock),
	Stream(RTC_STREAM_Index, "Index", Stream::VariableLength),
	Stream(RTC_STREAM_index, "index", Stream::VariableLength),
	Stream(RTC_STREAM_Meta, "Meta", Stream::VariableLength),
	Stream(RTC_STREAM_meta, "meta", Stream::VariableLength),
	Stream(RTC_STREAM_Platform, "Platform", Stream::VariableLength),
	Stream(RTC_STREAM_Crc, "Crc", Stream::VariableLength),
};





Cursor::Cursor(Reader& reader)
	: m_reader(&reader)
{
	reset();
}

Reader& Cursor::reader() const {
	return *m_reader;
}

void Cursor::reset() {
	m_pos = reader().pos();
	m_aligned = false;
	m_Marker = -1;
	m_frame = Frame{};
}

Offset Cursor::pos() const {
	return m_pos;
}

void Cursor::seek(Offset offset) {
	if(offset >= 0) {
		reader().seek(offset, SEEK_SET);
		m_pos = offset;
	} else {
		reader().seek(offset, SEEK_END);
		m_pos = reader().pos();
	}
}

Offset Cursor::forward(Offset offset) {
	Offset pos = m_pos + offset;
	reader().seek(pos, SEEK_SET);
	return m_pos = pos;
}

Offset Cursor::backward(Offset offset) {
	Offset pos = m_pos - offset;
	reader().seek(pos, SEEK_SET);
	return m_pos = pos;
}

size_t Cursor::read(void* dst, size_t len) {
	return reader().read(pos(), dst, len);
}

Offset Cursor::findMarker(bool forward) {
	m_aligned = false;

	uintptr_t magic;
	memset(&magic, Reader::Marker, sizeof(magic));
	static_assert(sizeof(magic) * 2 <= Reader::MarkerBlock, "");

	// Go check where we are
	uintptr_t word = 0;

	// Jump a full block, minus one word to fix misalignment.
	Offset const jump = Reader::MarkerBlock - sizeof(magic);

	while(true) {
		size_t res = read(&word, sizeof(word));
		if(res != sizeof(word)) {
			// End of file, cannot find Marker.
			return -1;
		}

		if(word != magic) {
try_next:
			// We are not within a Marker currently.
			if(forward)
				this->forward(jump);
			else if((size_t)pos() < sizeof(magic))
				// Reached start of file.
				return -1;
			else
				this->backward(jump);

			continue;
		}

		// We found a magic word. Find beginning.
		Offset possibleMarker = pos();
		Offset start = possibleMarker;
		unsigned char b = 0;
		while(start >= 0) {
			b = 0;
			if(reader().read(start, &b, 1) != 1) {
				// Error, start not found.
				goto try_next;
			}

			if(b != Reader::Marker)
				// Got it.
				break;

			// Try previous byte.
			start--;
		}

		// We got the start of a possible Marker.
		if(b != (RTC_STREAM_Marker << 1))
			// Proper frame header is not there.
			goto try_next;

		// Ok, the start looks good. Try to find the proper end.
		Offset end = possibleMarker;
		while(true) {
			b = 0;
			if(reader().read(end, &b, 1) != 1) {
				// Error end not found. End of file.
				return -1;
			}

			if(b != Reader::Marker) {
				// Got it
				break;
			}

			// Try next byte.
			end++;
		}

		// We have a range [start,end[ of frame header and Magic bytes.
		if(end - start != Reader::MarkerBlock + 1)
			goto try_next;

		// Found it.
		m_aligned = true;
		m_frame = Frame(start, start + 1, MarkerBlock);
		return m_Marker = m_pos = start;
	}
}

Frame const& Cursor::currentFrame() const {
	assert(aligned() || !m_frame.valid());
	return m_frame;
}

Stream const* Cursor::stream(Stream::Id id) const {
	if(id < 0)
		return nullptr;
	if(id < RTC_STREAM_DEFAULT_COUNT)
		return &defaultStreams[id];

	auto it = m_streams.find(id);
	return it == m_streams.end() ? nullptr : it->second;
}

Frame const& Cursor::parseFrame() {
	uint64_t x = 0;

	m_frame = Frame();

	Frame frame;
	Offset o = frame.header = pos();
	o += reader().readInt(o, &x);
	Stream::Id id = (Stream::Id)(x >> 1u);

	frame.more = (x & 1u);

	if(!(frame.stream = stream(id)))
		// Unknown stream.
		return m_frame;

	size_t len = 0;
	if(frame.stream->isVariableLength()) {
		frame.payload = o += reader().readInt(o, &x);
		if(x > Reader::MaxPayload)
			// Invalid length.
			return m_frame;

		frame.length = (size_t)x;
	} else {
		frame.payload = o;
		frame.length = frame.stream->frameLength();
	}

	return m_frame = frame;
}

Offset Cursor::nextFrame() {
	if(!aligned()) {
		if(findMarker() < 0)
			// Cannot find Marker.
			return -1;

		// Return the Marker as first frame.
		return pos();
	}

	// pos() points to the start of a frame. Parse header and proceed to next.
	if(!currentFrame().valid() || currentFrame.header != pos()) {
		// Out of date, reparse.
		parseFrame();
	}

	if(!currentFrame.valid()) {
		// Not parseable. Skip to next Marker.
		return findMarker();
	}

	seek(currentFrame.payload + currentFrame.length);
	return parseFrame().header;
}

Offset Cursor::findIndex(bool forward) {
	Offset pos = posMarker();
	if(pos < 0) {
		if((pos = findMarker(forward)) < 0)
			// Cannot find Marker.
			return -1;
	}

	assert(aligned());

	// Move to Marker
	seek(pos);

	// Skip the Marker
	if(nextFrame() == -1)
		// Cannot skip.
		return -1;

	Frame const& f = currentFrame();
	if(!f.valid())
		// No valid frame after the Marker
		return -1;
	if(!f.stream)
		// No stream related to this frame.
		return -1;
	if(f.stream->id() != RTC_STREAM_Index)
		// Wrong stream.
		return -1;

	// Got it.
	return f.header;
}

bool Cursor::aligned() const {
	return m_aligned;
}

Offset Cursor::posMarker() const {
	return m_Marker;
}



} // namespace

