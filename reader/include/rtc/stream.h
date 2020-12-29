#ifndef RTC_STREAM_H
#define RTC_STREAM_H
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

#include "rtc_writer.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <string>

namespace rtc {

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
		size_t m_frameLength;
		bool m_cont;
	};

	extern Stream const defaultStreams[RTC_STREAM_DEFAULT_COUNT];
} // namespace
#endif // __cplusplus
#endif // RTC_STREAM_H
