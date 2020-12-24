add_executable(inputs main.cpp)
target_link_libraries(inputs rtc_writer)
set_property(TARGET inputs PROPERTY C_STANDARD 11)

