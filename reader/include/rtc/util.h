#ifndef RTC_UTIL_H
#define RTC_UTIL_H
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

#include <utility>
#include <functional>

namespace rtc {

	class Scope {
	public:
		template <typename F>
		explicit Scope(F&& out)
			: m_out(std::forward<F>(out))
		{}

		Scope(Scope const& scope) = delete;
		Scope(Scope&& scope) {
			*this = std::move(scope);
		}

		void operator=(Scope const& scope) = delete;
		Scope& operator=(Scope&& scope) {
			m_out();
			m_out = scope.m_out;
			scope.m_out = nullptr;
			return *this;
		}

		~Scope() {
			m_out();
		}

	private:
		std::function<void()> m_out;
	};

} // namespace
#endif // __cplusplus
#endif // RTC_UTIL_H
