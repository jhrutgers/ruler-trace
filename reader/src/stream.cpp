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

#include "rtc/stream.h"
#include "rtc/reader.h"

namespace rtc {

Stream::Stream()
	: m_id(-1), m_frameLength(), m_cont()
{
}

Stream::Stream(Stream::Id id, char const* name, size_t frameLength, bool cont, char const* format)
	: m_id(id), m_name(name), m_frameLength(frameLength), m_cont(cont), m_format(format && *format ? format : "raw")
{
	m_meta["id"] = this->id();
	m_meta["name"] = this->name().c_str();
	if(this->frameLength() != VariableLength)
		m_meta["length"] = this->frameLength();
	if(this->cont())
		m_meta["cont"] = true;
	if(this->format() != "raw")
		m_meta["format"] = this->format().c_str();
}

Stream::Stream(json const& meta)
	: Stream()
{
	*this = meta;
}

int Stream::id() const {
	return m_id;
}

std::string const& Stream::name() const {
	return m_name;
}

bool Stream::isFixedLength() const {
	return frameLength() != VariableLength;
}

bool Stream::isVariableLength() const {
	return frameLength() == VariableLength;
}

size_t Stream::frameLength() const {
	return m_frameLength;
}

bool Stream::cont() const {
	return m_cont;
}

std::string const& Stream::format() const {
	return m_format;
}

json const& Stream::meta() const {
	return m_meta;
}

Stream& Stream::operator=(json const& meta) {
	m_id = (Id)meta["id"];
	m_name = meta.value("name", "");
	m_frameLength = meta.value("length", (size_t)VariableLength);
	m_cont = meta.value("cont", false);
	m_format = meta.value("format", "raw");
	m_meta = meta;
	return *this;
}

Stream const defaultStreams[RTC_STREAM_DEFAULT_COUNT] = {
	Stream(RTC_STREAM_nop, "nop", 0),
	Stream(RTC_STREAM_padding, "padding", Stream::VariableLength),
	Stream(RTC_STREAM_Marker, "Marker", Reader::MarkerBlock),
	Stream(RTC_STREAM_Index, "Index", Stream::VariableLength),
	Stream(RTC_STREAM_index, "index", Stream::VariableLength),
	Stream(RTC_STREAM_Meta, "Meta", Stream::VariableLength),
	Stream(RTC_STREAM_meta, "meta", Stream::VariableLength),
	Stream(RTC_STREAM_Platform, "Platform", 4),
	Stream(RTC_STREAM_Crc, "Crc", sizeof(crc_t)),
};

} // namespace

