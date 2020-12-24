add_executable(rulert
	getopt_mini.h
	dump.h
	dump.cpp
	help.h
	help.cpp
	rulert.cpp
)

target_link_libraries(rulert rtc_reader)
set_property(TARGET rulert PROPERTY CXX_STANDARD 14)

