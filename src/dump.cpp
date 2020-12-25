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

static void dump_mem(char const* desc, rtc::Reader& reader, rtc::Reader::Offset start, rtc::Reader::Offset end) {
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
	auto offMarker = c.findMarker();

	if(offMarker < 0) {
		printf("No Marker found\n");
		return;
	}

	printf("Found marker at %" PRIxPTR "\n", (uintptr_t)offMarker);

	if(offMarker > 0)
		dump_mem("Pre-Marker", reader, 0, offMarker);

	auto offIndex = c.findIndex();
}

