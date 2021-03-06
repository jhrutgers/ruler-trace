#ifndef RTC_CURSOR_H
#define RTC_CURSOR_H
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

#ifdef __cplusplus

#include "rtc_writer.h"
#include "rtc/stream.h"
#include "rtc/util.h"

#include <map>

namespace rtc {

#ifdef WIN32
	typedef int64_t Offset;
#else
	typedef long Offset;
#endif

	class Reader;

	class Frame {
	public:
		bool valid() const { return header >= 0; }
		operator bool() const { return valid(); }
		bool empty() const { return !valid() || length == 0; }

		Offset header = -1;
		Offset payload = -1;
		size_t length = 0;
		Stream const* stream = nullptr;
		bool more = false;
	};

	class Cursor {
	protected:
		explicit Cursor(Reader& reader);

	public:
		void reset();
		Reader& reader() const;

		Offset pos() const;
		void seek(Offset offset);
		Offset forward(Offset offset);
		Offset backward(Offset offset);
		size_t read(void* dst, size_t len);
		bool eof() const;

		Frame const& nextMarker();
		Frame const& prevMarker();
		Frame const& nextIndex();
		Frame const& prevIndex();
		Frame const& nextMeta();
		Frame const& prevMeta();
		Frame const& nextFrame();
		Frame const& nextFrame(Stream const& stream);
		Cursor& operator++();

		Frame const& currentFrame() const;
		Frame const& operator*() const;

		bool aligned() const;
		Offset Unit() const;
		Offset unit() const;
		Offset currentUnitStart() const;
		Offset currentunitStart() const;
		crc_t currentUnitCrc();

		Stream const* stream(Stream::Id id, bool autoLoadMeta = true);
		Offset index(Stream::Id id);

		template <typename F>
		void fullFrame(F&& f, size_t max = 0) {
			while(currentFrame() && currentFrame().more) {
				f(currentFrame());

				if(max > 0 && --max == 0)
					return;

				nextFrame(*currentFrame().stream);
			}
			if(currentFrame())
				f(currentFrame());
		}

		std::vector<unsigned char> fullFrame();
	protected:
		void seekUnsafe(Offset offset);
		Frame const& findMarker(bool forward);
		Frame const& parseFrame(bool autoLoadMeta = true);
		void loadMeta();
		void loadIndex();
		Scope stashPos();
	private:
		Reader* m_reader;
		Offset m_pos;
		bool m_eof = false;
		bool m_aligned = false;
		Offset m_Marker = -1;
		Offset m_Unit = -1;
		Offset m_unit = -1;
		Frame m_frame;
		std::map<Stream::Id,std::unique_ptr<Stream>> m_streams;
		std::map<Stream::Id,Offset> m_index;
		uint64_t m_IndexCount;

		friend class Reader;
	};

} // namespace
#endif // __cplusplus
#endif // RTC_CURSOR_H
