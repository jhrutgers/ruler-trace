cmake_minimum_required(VERSION 3.11)

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

include(FetchContent)

FetchContent_Declare(json
	GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
	GIT_TAG v3.9.1)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
	FetchContent_Populate(json)
	add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(rtc_reader PUBLIC nlohmann_json::nlohmann_json)

