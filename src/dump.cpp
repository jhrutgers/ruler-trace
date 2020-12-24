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

#include <rtc_reader.h>
#include "getopt_mini.h"

void dump_help(char const* progname, bool standalone) {
	if(standalone)
		fprintf(stderr, "Usage: ");
	fprintf(stderr, "%s dump <input>\n", progname);
	fprintf(stderr, "  Parse and dump a RTC file.\n");
	fprintf(stderr, "    input       The input RTC file.\n\n");
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

	rtc::RtcReader reader;
	if(reader.open(input)) {
		fprintf(stderr, "%s\n", reader.lastErrorStr().c_str());
		exit(2);
	}

	printf("Dump %s\n", input);
}

