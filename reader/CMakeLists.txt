add_library(rtc_reader
	include/rtc/reader.h
	include/rtc/exception.h
	include/rtc/cursor.h
	include/rtc/stream.h
	src/rtc_reader.cpp
	src/cursor.cpp
	src/stream.cpp
)

target_include_directories(rtc_reader
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(rtc_reader
	PUBLIC rtc_writer
)

set_property(TARGET rtc_reader PROPERTY CXX_STANDARD 14)

