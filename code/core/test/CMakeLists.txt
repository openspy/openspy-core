cmake_minimum_required (VERSION 3.22)

project(core_test)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories (${CMAKE_CURRENT_SOURCE_DIR})


add_executable (test_buffer ${ALL_SRCS} OS/test_buffer.cpp)
add_test(NAME test_buffer COMMAND test_buffer WORKING_DIRECTORY .)

target_link_libraries(test_buffer openspy)