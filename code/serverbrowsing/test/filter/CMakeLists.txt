cmake_minimum_required (VERSION 3.22)

project(core_test)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIR})


add_executable (test_filter ${ALL_SRCS} filter_basic_test.cpp)
add_test(NAME test_filter COMMAND test_filter WORKING_DIRECTORY .)
target_link_libraries(test_filter openspy SharedTasks)


add_executable (test_subtract ${ALL_SRCS} filter_subtract_test.cpp)
add_test(NAME test_subtract COMMAND test_subtract WORKING_DIRECTORY .)
target_link_libraries(test_subtract openspy SharedTasks)


add_executable (test_like ${ALL_SRCS} filter_like_test.cpp)
add_test(NAME test_like COMMAND test_like WORKING_DIRECTORY .)
target_link_libraries(test_like openspy SharedTasks)