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

#include "help.h"
#include "dump.h"

#include <rtc/exception.h>

#include <cstdlib>
#include <cstring>
#include <cstdio>

int main(int argc, char** argv) {

	try {
		if(argc <= 1) {
			help(argc ? argv[0] : "rulert");
			exit(1);
		}

		char* module = argv[1];
		if(     strcmp(module, "help") == 0) help(argv[0]);
		else if(strcmp(module, "dump") == 0) dump(argc, argv);
		else {
			fprintf(stderr, "Unknown module '%s'\n", module);
			help(argv[0]);
			exit(1);
		}
	} catch(rtc::Exception& e) {
		fprintf(stderr, "ERROR: %s\n", e.what());
		exit(2);
	}

	return 0;
}

