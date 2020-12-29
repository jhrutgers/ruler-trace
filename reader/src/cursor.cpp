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

#include "rtc/cursor.h"
#include "rtc/stream.h"
#include "rtc/reader.h"

#include <cassert>

#define MARKER_FRAME_SIZE ((size_t)1 + Reader::MarkerBlock)

namespace rtc {

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
	m_eof = false;
	m_aligned = false;
	m_Marker = -1;
	m_Unit = -1;
	m_unit = -1;
	m_frame = Frame();
	m_streams.clear();
}

Offset Cursor::pos() const {
	return m_pos;
}

void Cursor::seek(Offset offset) {
	m_aligned = false;
	if(offset >= 0)
		reader().seek(offset, SEEK_SET);

	seekUnsafe(offset);
}

void Cursor::seekUnsafe(Offset offset) {
	if(offset >= 0) {
		m_pos = offset;
	} else {
		reader().seek(offset, SEEK_END);
		m_pos = reader().pos();
	}
}

Offset Cursor::forward(Offset offset) {
	seek(this->pos() + offset);
	return pos();
}

Offset Cursor::backward(Offset offset) {
	seek(std::max<Offset>(0, pos() - offset));
	return pos();
}

size_t Cursor::read(void* dst, size_t len) {
	return reader().read(pos(), dst, len);
}

Frame const& Cursor::findMarker(bool forward) {
	m_aligned = false;
	m_eof = false;
	m_frame = Frame();

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
			assert(reader().eof());
			m_eof = true;
			return m_frame;
		}

		if(word != magic) {
try_next:
			// We are not within a Marker currently.
			if(forward)
				this->forward(jump);
			else if((size_t)pos() < sizeof(magic))
				// Reached start of file.
				return m_frame;
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
				assert(reader().eof());
				m_eof = true;
				return m_frame;
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
		m_Marker = m_pos = start;
		return m_frame = Frame{start, start + 1, Reader::MarkerBlock, &defaultStreams[RTC_STREAM_Marker]};
	}
}

Frame const& Cursor::nextMarker() {
	if(aligned()) {
		assert(m_Marker >= 0);

		if(Unit() > 0) {
			seekUnsafe(((pos() - m_Marker) / Unit() + 1) * Unit() + m_Marker);
			auto const* f = &parseFrame();
			if(*f)
				return *f;

			// Marker not here... Go find in another way.
			seekUnsafe(pos() - Unit() + MARKER_FRAME_SIZE);
		} else {
			// Don't know the Unit (yet).
			if(pos() == m_Marker) {
				// We are at the marker, move to after it.
				seekUnsafe(m_Marker + MARKER_FRAME_SIZE);
			} else {
				// We are not at the marker. Move a bit forward and start looking.
				seekUnsafe(pos() + Reader::MarkerBlock - sizeof(uintptr_t));
			}
		}
	}

	return findMarker(true);
}

Frame const& Cursor::prevMarker() {
	if(aligned()) {
		assert(m_Marker >= 0);

		if(Unit() > 0) {
			if(m_Marker < Unit() || pos() < Unit()) {
				// No other marker in the file.
				m_aligned = false;
				m_eof = false;
				seekUnsafe(0);
				return m_frame = Frame();
			}

			seekUnsafe(((pos() - m_Marker) / Unit() - 1) * Unit() + m_Marker);
			auto const* f = &parseFrame();
			if(*f)
				return *f;

			// Marker not here... Go find in another way.
			seekUnsafe(pos() + Unit());
		}
	}

	return findMarker(false);
}

Frame const& Cursor::nextIndex() {
	if(!aligned()) {
		auto const& f = nextMarker();
		if(!f)
			return f;
	}

	Offset thisUnitsIndex = m_Marker + MARKER_FRAME_SIZE;

	if(pos() < thisUnitsIndex) {
		// We are probably at the Marker.
		seekUnsafe(thisUnitsIndex);
	} else if(Unit() >= 0) {
		// We are in between this Unit's Index and the next Marker.
		Offset nextUnitsIndex = thisUnitsIndex + Unit();
		seekUnsafe(nextUnitsIndex);
	} else {
		// Unit still unknown.
		auto const& f = nextMarker();
		if(!f)
			return f;

		seekUnsafe(f.header + MARKER_FRAME_SIZE);
	}

	// We are at the start of the (probable) Index frame.
	auto const& f = parseFrame();
	if(!f)
		// No valid frame after the Marker
		return f;
	if(!f.stream)
		// No stream related to this frame.
		return m_frame = Frame();
	if(f.stream->id() != RTC_STREAM_Index)
		// Wrong stream.
		return m_frame = Frame();

	// Got it.
	return f;
}

Frame const& Cursor::prevIndex() {
	auto const& f = prevMarker();
	if(!f)
		return f;

	return nextIndex();
}

/*
Frame const& Cursor::nextMeta() {
}

Frame const& Cursor::prevMeta() {
}
*/

Frame const& Cursor::nextFrame() {
	if(!aligned()) {
		// Return the Marker as first frame.
		return findMarker(true);
	}

	// pos() points to the start of a frame. Parse header and proceed to next.
	if(!currentFrame().valid() || currentFrame().header != pos()) {
		// Out of date, reparse.
		parseFrame();
	}

	if(!currentFrame().valid()) {
		// Not parseable. Skip to next Marker.
		return nextMarker();
	}

	seekUnsafe(currentFrame().payload + currentFrame().length);
	return parseFrame();
}

/*Frame const& Cursor::nextFrame(Stream const& stream) {
}*/

Cursor& Cursor::operator++() {
	nextFrame();
	return *this;
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
	m_frame = Frame();
	m_eof = false;

	try {
		uint64_t x = 0;

		Frame frame;
		Offset o = frame.header = pos();
		o += reader().readInt(o, x);
		Stream::Id id = (Stream::Id)(x >> 1u);

		frame.more = (x & 1u);

		if(!(frame.stream = stream(id)))
			// Unknown stream.
			return m_frame;

		if(frame.stream->isVariableLength()) {
			frame.payload = o += reader().readInt(o, x);
			if(x > Reader::MaxPayload)
				// Invalid length.
				return m_frame;

			frame.length = (size_t)x;
		} else {
			frame.payload = o;
			frame.length = frame.stream->frameLength();
		}

		return m_frame = frame;
	} catch(FormatError&) {
		// Cannot parse bytes.
		m_eof = reader().eof();
		return m_frame;
	}
}

bool Cursor::aligned() const {
	return m_aligned;
}

Offset Cursor::Unit() const {
	return m_Unit;
}

Offset Cursor::unit() const {
	return m_unit;
}

bool Cursor::eof() const {
	return m_eof;
}

} // namespace

