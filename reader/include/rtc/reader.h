#ifndef RTC_READER_H
#define RTC_READER_H
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

#if __cplusplus < 201103L && defined(_MSVC_LANG) && _MSVC_LANG < 201103L
#  warning "Please compile as C++11 or newer"
#endif

#include "rtc/exception.h"
#include "rtc_writer.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <utility>

namespace rtc {


#ifdef WIN32
	typedef int64_t Offset;
#else
	typedef long Offset;
#endif

	class Stream {
	public:
		typedef int Id;

		static const size_t VariableLength = RTC_STREAM_VARIABLE_LENGTH;

		Stream(Id id, char const* name, size_t frameLength = VariableLength, bool cont = false);

		Id id() const;
		std::string const& name() const;
		size_t frameLength() const;
		bool isFixedLength() const;
		bool isVariableLength() const;
		bool cont() const;
	private:
		Id m_id;
		std::string m_name;
		size_t m_framelength;
		bool m_cont;
	};

	extern Stream const defaultStreams[RTC_STREAM_DEFAULT_COUNT];

	class Frame {
	public:
		/*
		Frame() {}
		Frame(Offset header, Offset payload, size_t length, Stream const* stream)
			: header(header), payload(payload), length(length), stream(stream) {}
		*/

		bool valid() const { return header >= 0; }
		bool empty() const { return !valid() || length == 0; }

		Offset header = -1;
		Offset payload = -1;
		size_t length = 0;
		Stream const* stream = nullptr;
		bool more = false;
	};

	class Reader;

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

		Offset findMarker(bool forward = true);
		Offset findIndex(bool forward = true);
		Offset nextFrame();
		Frame const& currentFrame() const;

		size_t read(void* dst, size_t len);

		bool aligned() const;
		Offset posMarker() const;

		Stream const* stream(Stream::Id id) const;
	protected:
		Frame const& parseFrame();
	private:
		Reader* m_reader;
		Offset m_pos;
		bool m_aligned = false;
		Offset m_Marker = -1;
		Frame m_frame;
		std::map<Stream::Id,Stream*> m_stream;

		friend class Reader;
	};

	class Reader {
	public:
		enum {
			Marker1 = RTC_MARKER_1,
			Marker = Marker1,
			MarkerBlock = RTC_MARKER_BLOCK,
			MaxPayload = MarkerBlock,
		};

		Reader();

		template <typename Arg>
		explicit Reader(Arg&& arg)
			: Reader()
		{
			open(std::forward<Arg>(arg));
		}

		~Reader();

		void open(char const* filename);
		void open(FILE* f);
#ifdef _POSIX_C_SOURCE
		void open(int fd);
#endif
		void close();

		bool isOpen() const;
		Offset pos();

		void seek(Offset offset, int whence);
		size_t read(Offset offset, void* dst, size_t len);
		size_t readInt(Offset offset, uint64_t& dst);

		Cursor cursor();
	protected:
		FILE* file() const;
	private:
		FILE* m_file = nullptr;
		Offset m_offset = 0;
	};
} // namespace

#endif // __cplusplus
#endif // RTC_READER_H
