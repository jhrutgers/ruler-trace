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
#include "rtc/cursor.h"
#include "rtc/stream.h"

#include "rtc_writer.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <utility>

namespace rtc {

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
		static size_t decodeInt(unsigned char* buffer, size_t len, uint64_t& dst);
		bool eof() const;

		Cursor cursor();

		crc_t crc(Offset start, Offset end);
	protected:
		FILE* file() const;
	private:
		FILE* m_file = nullptr;
		Offset m_pos = 0;
	};
} // namespace

#endif // __cplusplus
#endif // RTC_READER_H
