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

#include "rtc_reader.h"

namespace rtc {

RtcReader::RtcReader()
{
}

RtcReader::~RtcReader() {
	close();
}

int RtcReader::lastError() const {
	return m_lastError;
}

std::string const& RtcReader::lastErrorStr() const {
	return m_lastErrorStr;
}

int RtcReader::error(int e) {
	if(!e) {
		m_lastError = 0;
		m_lastErrorStr.clear();
		return 0;
	} else
		return error(e, nullptr);
}

int RtcReader::error(int e, char const* fmt, ...) {
	std::va_list args;
	va_start(args, fmt);
	int res = errorv(e, fmt, args);
	va_end(args);
	return res;
}

int RtcReader::errorv(int e, char const* fmt, std::va_list args) {
	m_lastErrorStr.clear();

	char buffer[1024];
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
		m_lastErrorStr = buffer;
	} else if(len > 0) {
		char* b = new char[(size_t)len];
		vsnprintf(b, len, fmt, args);
		m_lastErrorStr = b;
		delete[] b;
	}

	if(!m_lastErrorStr.empty()) {
		if(isalnum(m_lastErrorStr.back()))
			m_lastErrorStr += "; ";
		else if(!isblank(m_lastErrorStr.back()))
			m_lastErrorStr += " ";
	}

	m_lastErrorStr += strerror(e);
	return m_lastError = e;
}

int RtcReader::open(char const* filename) {
	close();

	if(!filename)
		return error(EINVAL);

	FILE* f = fopen(filename, "rb");
	if(!f)
		return error(errno, "Cannot open '%s';", filename);

	m_file = f;
	return 0;
}

int RtcReader::open(FILE* f) {
	close();

	if(f)
		return error(EINVAL);

	m_file = f;
	return 0;
}

#ifdef _POSIX_C_SOURCE
int RtcReader::open(int fd) {
	close();

	FILE* f = fdopen(fd, "rb");
	if(!f)
		return error(errno, "Cannot open fd %d", fd);

	m_file = f;
	return 0;
}
#endif

int RtcReader::close() {
	error();

	if(!m_file)
		return 0;

	if(fclose(m_file) == EOF)
		error(errno, "Cannot close file");

	m_file = nullptr;
	return lastError();
}

FILE* RtcReader::file() const {
	return m_file;
}

bool RtcReader::isOpen() const {
	return file() != nullptr;
}

intptr_t RtcReader::pos() {
	error();

	if(!isOpen()) {
		error(EINVAL, "File is not open");
		return -1;
	}

#ifdef WIN32
	intptr_t res = (intptr_t)_ftelli64(file());
#else
	intptr_t res = (intptr_t)ftell(file());
#endif

	if(res == -1) {
		int e = errno;
		close();
		error(e, "Cannot get current file position");
	}

	return res;
}

int RtcReader::seek(int64_t offset, int whence) {
	if(!isOpen())
		return error(EINVAL, "File is not open");

#ifdef WIN32
	if(_fseeki64(file(), offset, whence))
		return errno;
#else
	if(fseek(file(), (long)offset, whence))
		return errno;
#endif

	return 0;
}

int RtcReader::forward(int64_t offset) {
	return seek(offset, SEEK_CUR);
}

int RtcReader::backward(int64_t offset) {
	return seek(-offset, SEEK_CUR);
}

} // namespace

