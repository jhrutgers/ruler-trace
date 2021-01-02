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
#include "rtc/reader.h"

namespace rtc {

Reader::Reader()
{
}

Reader::~Reader() {
	try {
		close();
	} catch(Exception&) {
	}
}

void Reader::open(char const* filename) {
	close();

	if(!filename)
		throw std::invalid_argument("filename");

	FILE* f = fopen(filename, "rb");
	if(!f)
		throw Exception(errno, "Cannot open '%s';", filename);

	m_file = f;
}

void Reader::open(FILE* f) {
	close();

	if(f)
		throw std::invalid_argument("f");

	m_file = f;
	seek(0, SEEK_SET);
}

#ifdef _POSIX_C_SOURCE
void Reader::open(int fd) {
	close();

	FILE* f = fdopen(fd, "rb");
	if(!f)
		throw Exception(errno, "Cannot open fd %d", fd);

	m_file = f;
	seek(0, SEEK_SET);
}
#endif

void Reader::close() {
	if(!m_file)
		return;

	if(fclose(m_file) == EOF)
		throw Exception(errno, "Cannot close file");

	m_file = nullptr;
}

FILE* Reader::file() const {
	return m_file;
}

bool Reader::eof() const {
	return feof(file()) != 0; // error is also reported as EOF
}

bool Reader::isOpen() const {
	return file() != nullptr;
}

Offset Reader::pos() {
	if(!isOpen())
		throw Exception("File is not open");

#ifdef WIN32
	intptr_t res = (intptr_t)_ftelli64(file());
#else
	intptr_t res = (intptr_t)ftell(file());
#endif

	if(res == -1) {
		int e = errno;
		close();
		throw Exception(e, "Cannot get current file position");
	}

	return res;
}

void Reader::seek(Offset offset, int whence) {
	if(!isOpen())
		throw SeekError("File is not open");

#ifdef WIN32
	if(_fseeki64(file(), offset, whence))
		throw SeekError(errno);
#else
	if(fseek(file(), (long)offset, whence))
		throw SeekError(errno);
#endif
}

Cursor Reader::cursor() {
	return Cursor(*this);
}

size_t Reader::read(Offset offset, void* dst, size_t len) {
	if(!isOpen())
		throw Exception("File is not open");
	if(!len)
		return 0;
	if(!dst)
		throw std::invalid_argument("dst");

	if(m_pos != offset) {
		seek(offset, SEEK_SET);
		m_pos = offset;
	}

	size_t res = fread(dst, 1, len, file());
	m_pos += res;
	if(res < len) {
		if(ferror(file())) {
			clearerr(file());
			throw Exception("Cannot read file");
		}
		// else EOF
	}

	return res;
}

size_t Reader::readInt(Offset offset, uint64_t& dst) {
	unsigned char buf[10]; // Maximum length of encoded 64-bit value.
	size_t buflen = read(offset, buf, sizeof(buf));
	return decodeInt(buf, buflen, dst);
}

size_t Reader::decodeInt(unsigned char* buffer, size_t len, uint64_t& dst) {
	dst = 0;
	unsigned int shift = 0;
	size_t i = 0;
	while(true) {
		if(i >= len)
			throw FormatError("Int truncated");

		unsigned char b = buffer[i++];
		dst |= (uint64_t)(b & 0x7fu) << shift;
		if(!(b & 0x80u))
			return i;

		shift += 7;
	}

	throw FormatError("Int too long");
}

crc_t Reader::crc(Offset start, Offset end) {
#ifdef RTC_NO_CRC
	(void)start;
	(void)end;
	return 0;
#else
	if(start < 0)
		start = 0;
	if(end < 0) {
		seek(0, SEEK_END);
		end = pos();
	}
	if(start >= end)
		return rtc_crc_end(rtc_crc_start());

	crc_t crc = rtc_crc_start();
	char buffer[1u << 12u];
	Offset len = end - start;
	for(Offset i = 0; i < len; i += sizeof(buffer)) {
		size_t r = read(start + i, buffer, std::min<size_t>((size_t)(len - i), sizeof(buffer)));
		crc = rtc_crc(crc, buffer, r);
		i += r;

		if(!r) {
			assert(eof());
			break;
		}
	}

	return rtc_crc_end(crc);
#endif
}

} // namespace

