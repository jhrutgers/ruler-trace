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

#include "dump.h"

#include <rtc/reader.h>
#include "getopt_mini.h"

#include <cinttypes>

void dump_help(char const* progname, bool standalone) {
	if(standalone)
		fprintf(stderr, "Usage: ");
	fprintf(stderr, "%s dump <input>\n", progname);
	fprintf(stderr, "  Parse and dump a RTC file.\n");
	fprintf(stderr, "    input       The input RTC file.\n\n");
}

static void dump_mem(char const* desc, rtc::Reader& reader, rtc::Offset start, rtc::Offset end) {
	if(start < 0)
		start = 0;
	if(start >= end)
		return;

	if(desc && *desc)
		printf("%s\n", desc);

	uintptr_t p = start & ~(uintptr_t)0xf;
	int pwidth = sizeof(end) * 2;
	if(pwidth > 8 && (uintptr_t)end <= (uintptr_t)0xffffffffUL)
		pwidth = 8;

	for(; p < (uintptr_t)end; p += 0x10) {
		printf("%*" PRIxPTR ": ", pwidth, p);

		unsigned char buf[16];
		size_t len = sizeof(buf);
		size_t offset = 0;

		if(p < (uintptr_t)start)
			offset = (size_t)((uintptr_t)start - p);
		if(p + offset + len >= (uintptr_t)end)
			len = std::min<size_t>(16, (uintptr_t)end - p);

		size_t readlen = len - offset;
		len = reader.read(p + offset, buf, readlen);
		bool eof = readlen > len;

		size_t i = 0;
		while(i < len) {
			if(offset >= 8) {
				printf("%*s", 8 * 3, "");
				offset -= 8;
			} else {
				if(offset > 0)
					printf("%*s", (int)(offset * 3), "");

				for(size_t b = offset; b < 8u && i < len; i++, b++)
					printf(" %02hhx", buf[i]);

				offset = 0;
			}

			printf(" ");
		}

		printf("\n");

		if(eof) {
			printf("<eof>\n");
			break;
		}
	}

}

static void dump_mem(rtc::Reader& reader, rtc::Frame const& frame) {
	if(!frame.valid()) {
		printf("\nInvalid frame\n");
	} else {
		std::string s = "\nStream \"";
		s += frame.stream->name();
		s += "\"";
		if(frame.more)
			s += " (more)";
		s += "\n+header:";
		dump_mem(s.c_str(), reader, frame.header, frame.payload);
		dump_mem("+payload:", reader, frame.payload, frame.payload + frame.length);
	}
}

void dump(int argc, char** argv) {
	Getopt opt(argc - 1, argv + 1, "h");

	while(opt() != -1) {
		switch(opt.optopt) {
		case 'h':
			dump_help(argv[0]);
			exit(0);
		default:
			fprintf(stderr, "Unknown option '%c'\n", opt.optopt);
			dump_help(argv[0]);
			exit(1);
		}
	}

	if(opt.optind + 1 != argc - 1) {
		fprintf(stderr, "Missing input file\n");
		dump_help(argv[0]);
		exit(1);
	}

	char* input = argv[opt.optind + 1];

	rtc::Reader reader;
	reader.open(input);

	printf("Dump %s\n", input);

	auto c = reader.cursor();
	auto prev = c.pos();

	try {
		while(c.nextFrame()) {
			auto const& f = c.currentFrame();
			if(prev != f.header)
				dump_mem("\nUnparseable gap:", reader, prev, f.header);

			dump_mem(reader, f);
			prev = f.payload + f.length;
			if(f.stream && f.stream->id() == RTC_STREAM_Crc) {
				// TODO: compare with frame
				printf("+Unit CRC: %lx\n", (long)c.currentUnitCrc());
			}
		}
		printf("<parsed end>\n");
	} catch(rtc::SeekError&) {
		printf("<terminated>\n");
	}
}

