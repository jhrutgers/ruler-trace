add_library(rtc_writer rtc_writer.c)

target_include_directories(rtc_writer
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(rtc_writer PRIVATE -ansi -Wpedantic)

