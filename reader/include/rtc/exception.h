#ifndef RTC_EXCEPTION_H
#define RTC_EXCEPTION_H
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

#include <cstdio>
#include <exception>
#include <string>
#include <cstdarg>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <memory>

#ifdef __GCC__
#  define FORMAT_ATTR(...)	__attribute__((format(__VA_ARGS__)))
#else
#  define FORMAT_ATTR(...)
#endif

namespace rtc {
	class Exception : public std::exception {
	protected:
		Exception()
			: m_error(-1)
		{}

	public:
		explicit FORMAT_ATTR(printf,2,3) Exception(char const* fmt, ...)
			: m_error(-1)
		{
			if(fmt && *fmt) {
				va_list args;
				va_start(args, fmt);
				init(fmt, args);
				va_end(args);
			}
		}

		FORMAT_ATTR(printf,3,4) Exception(int error, char const* fmt = "", ...)
			: m_error(error ? error : -1)
		{
			if(fmt && *fmt) {
				va_list args;
				va_start(args, fmt);
				init(fmt, args);
				va_end(args);
			}
		}

		virtual ~Exception() override = default;
	public:
		int error() const { return m_error; }

		virtual const char* what() const noexcept override {
			if(m_what.empty()) {
				if(error() > 0) {
					// Delayed conversion
					return strerror(error());
				} else {
					return "Unspecified error";
				}
			} else
				return m_what.c_str();
		}

	protected:
		void init(char const* fmt, std::va_list args) {
			try {
				char buffer[256];
				int len = 0;
				if(fmt && *fmt) {
					std::va_list args_copy;
					va_copy(args_copy, args);
					len = vsnprintf(buffer, sizeof(buffer), fmt, args);
					va_end(args_copy);

					if(len < 0)
						// Ignore error.
						len = 0;
				}

				if((size_t)len < sizeof(buffer)) {
					m_what = buffer;
				} else if(len > 0) {
					std::unique_ptr<char[]> b(new char[(size_t)len + 1]);
					vsnprintf(b.get(), len + 1, fmt, args);
					m_what = b.get();
				}

				if(error() > 0) {
					if(!m_what.empty()) {
						if(isalnum(m_what.back()))
							m_what += "; ";
						else if(!isblank(m_what.back()))
							m_what += " ";
					}

					m_what += strerror(error());
				}
			} catch(std::bad_alloc&) {
			}
		}
	private:
		int m_error;
		std::string m_what;
	};

#define RTC_EXCEPTION_CTOR(Exc) \
	explicit FORMAT_ATTR(printf,2,3) Exc(char const* fmt, ...) \
	{ \
		if(fmt && *fmt) { \
			va_list args; \
			va_start(args, fmt); \
			init(fmt, args); \
			va_end(args); \
		} \
	} \
	FORMAT_ATTR(printf,3,4) Exc(int error, char const* fmt = "", ...) \
		: Exception(error) \
	{ \
		if(fmt && *fmt) { \
			va_list args; \
			va_start(args, fmt); \
			init(fmt, args); \
			va_end(args); \
		} \
	}

	class SeekError : public Exception {
	public:
		RTC_EXCEPTION_CTOR(SeekError)
		virtual ~SeekError() override = default;
	};

#undef RTC_EXCEPTION_CTOR

} // namespace

#undef FORMAT_ATTR

#endif // __cplusplus
#endif // RTC_EXCEPTION_H
