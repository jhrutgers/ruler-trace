/*
 * Ruler Trace Container
 * Copyright (C) 2020-2021  Jochem Rutgers
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
	m_index.clear();
	m_IndexCount = 0;
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
			seekUnsafe(currentUnitStart() + Unit());
			auto const* f = &parseFrame();
			if(*f) {
				m_Marker = f->header;
				return *f;
			}

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

			Offset here = pos();
			Offset currentMarker = currentUnitStart();
			if(currentMarker < here)
				seekUnsafe(currentMarker);
			else
				seekUnsafe(currentMarker - Unit());

			auto const* f = &parseFrame();
			if(*f) {
				m_Marker = f->header;
				return *f;
			}

			// Marker not here... Go find in another way.
			seekUnsafe(here - 1);
		}
	}

	return findMarker(false);
}

Offset Cursor::currentUnitStart() const {
	if(!aligned() || m_Marker < 0)
		return -1;
	else if(Unit() <= 0)
		return m_Marker;
	else if(pos() >= m_Marker)
		return (pos() - m_Marker) / Unit() * Unit() + m_Marker;
	else
		return std::max<Offset>(-1, (pos() - m_Marker - Unit() + 1) / Unit() * Unit() + m_Marker);
}

Offset Cursor::currentunitStart() const {
	if(!aligned() || m_Marker <= 0)
		return -1;
	else if(unit() <= 0)
		return m_Marker + MARKER_FRAME_SIZE;
	else if(pos() >= m_Marker)
		return (pos() - m_Marker - MARKER_FRAME_SIZE) / unit() * unit() + m_Marker + MARKER_FRAME_SIZE;
	else
		return std::max<Offset>(-1, (pos() - m_Marker - MARKER_FRAME_SIZE - unit() + 1) / unit() * unit() + m_Marker + MARKER_FRAME_SIZE);
}

Frame const& Cursor::nextIndex() {
	Offset thisUnitsIndex = currentUnitStart();

	if(!aligned() || thisUnitsIndex < 0) {
		auto const& fp = prevMarker();
		if(!fp) {
			auto const& fn = nextMarker();
			if(!fn)
				return fn;
		}

		thisUnitsIndex = currentUnitStart();
		assert(thisUnitsIndex >= 0);
	}

	thisUnitsIndex += MARKER_FRAME_SIZE;

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
	auto const& f = parseFrame(false);
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

Frame const& Cursor::nextMeta() {
	// Move two Units forward, as its Index holds the reference to the Meta we need.
	if(!nextIndex() || !nextIndex())
		return m_frame = Frame();

	Offset pos = index(RTC_STREAM_Meta);

	if(pos < 0)
		return m_frame = Frame();

	seekUnsafe(pos);
	return parseFrame();
}

Frame const& Cursor::prevMeta() {
	Offset pos = index(RTC_STREAM_Meta);

	if(pos < 0 || pos < Unit())
		return m_frame = Frame();

	assert(Unit() > 0);
	seekUnsafe(pos - Unit());
	return parseFrame();
}

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
	if(parseFrame())
		return currentFrame();

	// Unknown next frame. Skip to next Marker.
	return nextMarker();
}

Frame const& Cursor::nextFrame(Stream const& stream) {
	// TODO: do smarter; skip full [Uu]nits when there are no frames expected
	// in there of the given stream.

	while(nextFrame())
		if(currentFrame().stream->id() == stream.id())
			return currentFrame();

	return currentFrame();
}

Cursor& Cursor::operator++() {
	nextFrame();
	return *this;
}

Frame const& Cursor::currentFrame() const {
	assert(aligned() || !m_frame.valid());
	return m_frame;
}

Frame const& Cursor::operator*() const {
	return currentFrame();
}

Stream const* Cursor::stream(Stream::Id id, bool autoLoadMeta) {
	if(id < 0)
		return nullptr;
	if(id < RTC_STREAM_DEFAULT_COUNT)
		return &defaultStreams[id];

	auto it = m_streams.find(id);
	if(it != m_streams.end())
		return it->second.get();
	if(!autoLoadMeta)
		return nullptr;
	if(!aligned())
		if(!findMarker(true))
			return nullptr;
	assert(aligned());

	// Unknown ID. Try loading the Meta stream.
	// This may happen during other frame scanning, so
	// save the current offset, so we can return to it later on.
	auto here = stashPos();

	assert(m_Marker >= 0);

	try {
		// Move to next Meta, which is probably of the next Unit.
		// This one is more accurate. If that does not work, fall back to the
		// Meta of this Unit.
		if(nextMeta() || prevMeta()) {
			// Load the frame that is at the current cursor.
			loadMeta();
		}

	} catch(SeekError&) {
		// Too bad. Give up.
	}

	// Retry lookup.
	// TODO: fix the corner case that the stream is only mentioned in a meta.
	it = m_streams.find(id);
	return it == m_streams.end() ? nullptr : it->second.get();
}

Frame const& Cursor::parseFrame(bool autoLoadMeta) {
	m_eof = false;

	try {
		uint64_t x = 0;

		Frame frame;
		Offset o = frame.header = pos();
		o += reader().readInt(o, x);
		Stream::Id id = (Stream::Id)(x >> 1u);

		frame.more = (x & 1u);

		if(!(frame.stream = stream(id, autoLoadMeta))) {
			// Unknown stream.
			return m_frame = Frame();
		}

		if(frame.stream->isVariableLength()) {
			frame.payload = o += reader().readInt(o, x);
			if(x > Reader::MaxPayload) {
				// Invalid length.
				return m_frame = Frame();
			}

			frame.length = (size_t)x;
		} else {
			frame.payload = o;
			frame.length = frame.stream->frameLength();
		}

		return m_frame = frame;
	} catch(FormatError&) {
		// Cannot parse bytes.
		m_eof = reader().eof();
		return m_frame = Frame();
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

Offset Cursor::index(Stream::Id id) {
	Offset here = pos();
	auto scope = stashPos();

	// Make sure the index is up to date with the current pos().
	if(!aligned()) {
		m_index.clear();
		m_Unit = -1;
		m_unit = -1;
		if(!nextIndex())
			return -1;
	}

	if(Unit() <= 0) {
rebuild_Index:
		// No Index loaded yet.
		m_index.clear();
		m_Unit = -1;
		m_unit = -1;

		// Go back to the current Unit's Marker.
		if(!prevMarker())
			return -1;

		// Now, go to the Index.
		if(!nextIndex())
			return -1;

		// Load it.
		loadIndex();
	} else {
		// The Index should be in the index.
		auto it = m_index.find(RTC_STREAM_Index);
		if(it == m_index.end())
			// Huh? Bad index?
			goto rebuild_Index;

		if(here < it->second || here >= it->second + Unit())
			// Out of Unit sync.
			goto rebuild_Index;
	}

	// If we get here, we know m_index is in sync with the current Unit.
	// Check if the delta is valid too.
	assert(unit() > 0);

	auto it = m_index.find(RTC_STREAM_index);
	if(it == m_index.end() || it->second > here) {
		// Fully replay index frames from last Index.
		goto rebuild_Index;
	} else if(it->second + unit() < here) {
		// Replay the last (few) index frame(s).
		Offset replay = it->second;

		while(replay < here) {
			seekUnsafe(replay);
			loadIndex();
			replay += unit();
		}
	}

	// Ok, index is up to date now.
	it = m_index.find(id);
	return it == m_index.end() ? -1 : it->second;
}

std::vector<unsigned char> Cursor::fullFrame() {
	std::vector<unsigned char> buffer;

	fullFrame([&](Frame const& f) {
		if(f.length > 0) {
			assert(f.payload >= 0);
			size_t offset = buffer.size();
			buffer.resize(buffer.size() + f.length);
			if(reader().read(f.payload, &buffer[offset], f.length) != f.length)
				throw FormatError("EOF");
		}
	});

	return buffer;
}

void Cursor::loadIndex() {
	// The Index and index are always one ore more consecutive frames.
	// There are never interrupted by other frames.

	if(!parseFrame())
		throw FormatError("Wrong frame");

	Offset here = pos();

	bool haveCount = false;
	switch(currentFrame().stream->id()) {
	case RTC_STREAM_Index:
		haveCount = true;
		break;
	case RTC_STREAM_index:
		break;
	default:
		throw FormatError("Wrong stream");
	}

	// Copy full index to a buffer.
	auto buffer = fullFrame();

	size_t decoded = 0;

	if(haveCount)
		decoded += Reader::decodeInt(&buffer[decoded], buffer.size() - decoded, m_IndexCount);
	while(decoded < buffer.size()) {
		uint64_t id;
		uint64_t off;

		decoded += Reader::decodeInt(&buffer[decoded], buffer.size() - decoded, id);
		if(!(id & 1u))
			throw FormatError("Wrong entry ID");
		id >>= 1u;

		decoded += Reader::decodeInt(&buffer[decoded], buffer.size() - decoded, off);
		if((off & 1))
			throw FormatError("Wrong entry offset");
		off >>= 1u;

		if(off)
			m_index[(Stream::Id)id] = here - (Offset)off;
	}

	if(haveCount) {
		auto it = m_index.find(RTC_STREAM_Index);
		if(it != m_index.end())
			m_Unit = here - it->second;

		it = m_index.find(RTC_STREAM_index);
		if(it != m_index.end())
			m_unit = here - it->second;
	}

	// Override index to point to the last parsed [Ii]ndex frame.
	m_index[RTC_STREAM_index] = here;
}

void Cursor::loadMeta() {
	if(!parseFrame())
		throw FormatError("Wrong frame");

	switch(currentFrame().stream->id()) {
	case RTC_STREAM_Meta:
	case RTC_STREAM_meta:
		break;
	default:
		throw FormatError("Wrong stream");
	}

	// Copy full index to a buffer.
	auto buffer = fullFrame();

	auto j = json::parse((char const*)buffer.data());
	if(!j.is_array() || j.size() < 1)
		throw FormatError("JSON format error");

	try {
		for(auto const& s : j) {
			if(!s.is_object())
				continue;

			auto id = (Stream::Id)s["id"];
			auto stream = m_streams.find(id);
			if(stream == m_streams.end())
				m_streams[id].reset(new Stream(s));
			else
				*stream->second = s;
		}
	} catch(json::exception const& e) {
		throw FormatError(e.what());
	}
}

Scope Cursor::stashPos() {
	Offset here = pos();
	bool a = aligned();
	return Scope([this,here,a](){ seekUnsafe(here); m_aligned = a; });
}

crc_t Cursor::currentUnitCrc() {
#ifdef RTC_NO_CRC
	return 0;
#else
	auto pos = stashPos();
	Offset end = this->pos();
	Offset start = prevMarker() + MARKER_FRAME_SIZE;
	return reader().crc(start, end);
#endif
}

} // namespace

