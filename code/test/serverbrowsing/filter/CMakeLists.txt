cmake_minimum_required (VERSION 3.22)

project(serverbrowsing_filter_test)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIR})

add_executable (test_filter ${ALL_SRCS} test_filter.cpp)
add_test(NAME test_filter COMMAND test_sbv1_groups WORKING_DIRECTORY .)

target_link_libraries(test_filter openspy)