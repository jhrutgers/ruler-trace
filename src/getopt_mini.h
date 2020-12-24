#ifndef GETOPT_MINI_H
#define GETOPT_MINI_H

class Getopt {
public:
	Getopt(int argc, char* const* argv, char const* options)
		: argc(argc)
		, argv(argv)
		, options(options)
	{}

	int operator()() {
		if(optind >= argc || !argv || !options)
			return -1;

		char* a = argv[optind];

		if(a[0] != '-')
			// Stop parsing.
			return -1;

		switch((optopt = a[1])) {
		case '\0':
		case ':':
			// Not an option.
		case '-':
			// Stop parsing.
			return -1;
		default:;
		}

		optind++;

		// Check if option exists.
		int i = 0;
		for(; options[i] && options[i] != optopt; i++);

		if(!options[i])
			// Unknown.
			return '?';

		if(options[i + 1] != ':')
			// No argument, ok.
			return optopt;

		if(a[2] != '\0')
			// Argument is merged.
			optarg = &a[2];
		else if(optind < argc)
			// Argument is next arg.
			optarg = argv[optind++];
		else
			return options[0] == ':' ? ':' : '?';

		// Ok.
		return optopt;
	}

	int opterr = 1;
	int optopt = 0;
	int optind = 1;
	char* optarg = nullptr;

private:
	int const argc;
	char* const* const argv;
	char const* const options;
};

#endif // GETOPT_MINI_H
