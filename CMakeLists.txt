cmake_minimum_required(VERSION 3.0)
project(libstored)

if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
  message(FATAL_ERROR "CMake generation is not allowed within the source directory! \
    Remove the CMakeCache.txt file and try again from another folder, e.g.: \
    \
      rm CMakeCache.txt \
      mkdir build \
      cd build \
      cmake ..")
endif()

add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wconversion -ffunction-sections -fdata-sections)
add_subdirectory(writer)
add_subdirectory(reader)
add_subdirectory(src)

option(RTC_EXAMPLES "Build examples" ON)
if(RTC_EXAMPLES)
	add_subdirectory(examples)
endif()

find_program(CPPCHECK_CMD NAMES cppcheck)
if(CPPCHECK_CMD)
	if(RTC_EXAMPLES)
		set(CPPCHECK_EXAMPLES
			examples
		)
	endif()

	add_custom_target(cppcheck
		COMMAND
			${CPPCHECK_CMD}
			--enable=warning,style,information
			--force
			--inline-suppr
			--quiet
			--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr
			--error-exitcode=1
#			--xml
			-Ubool
			${CPPCHECK_EXAMPLES}
			-I writer
			-I reader/include
			writer
			reader
			src
#			--check-config
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)

	option(RTC_CPPCHECK "Run cppcheck by default" ON)
	if(RTC_CPPCHECK)
		add_custom_target(cppcheck-all ALL DEPENDS cppcheck)
	endif()
endif()
