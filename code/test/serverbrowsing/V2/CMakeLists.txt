cmake_minimum_required (VERSION 3.22)

project(serverbrowsing_v2_test)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIR})


add_executable (test_sbv2_server_message ${ALL_SRCS} test_common.cpp test_server_message.cpp)
add_test(NAME test_sbv2_server_message COMMAND test_sbv2_server_message WORKING_DIRECTORY .)

add_executable (test_sbv2_groups ${ALL_SRCS} test_common.cpp test_groups.cpp)
add_test(NAME test_sbv2_groups COMMAND test_sbv2_groups WORKING_DIRECTORY .)
target_link_libraries(test_sbv2_server_message openspy)
target_link_libraries(test_sbv2_groups openspy)
