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

#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <utility>

namespace rtc {

	class RtcReader {
	public:
		RtcReader();

		template <typename Arg>
		explicit RtcReader(Arg&& arg)
			: RtcReader()
		{
			open(std::forward<Arg>(arg));
		}

		~RtcReader();

		int open(char const* filename);
		int open(FILE* f);
#ifdef _POSIX_C_SOURCE
		int open(int fd);
#endif
		int close();

		bool isOpen() const;
		intptr_t pos();

		int lastError() const;
		std::string const& lastErrorStr() const;
	protected:
		int forward(int64_t offset);
		int backward(int64_t offset);
		int seek(int64_t offset, int whence);

		int error(int e = 0);
		int error(int e, char const* fmt, ...);
		int errorv(int e, char const* fmt, std::va_list args);

		FILE* file() const;
	private:
		int m_lastError = 0;
		std::string m_lastErrorStr;
		FILE* m_file = nullptr;
	};
} // namespace

#endif // __cplusplus
#endif // RTC_READER_H
