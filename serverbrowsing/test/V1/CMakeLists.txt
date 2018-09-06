cmake_minimum_required (VERSION 2.6)

project(serverbrowsing_v1_test)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

add_executable (test_v1_groups ${ALL_SRCS} test_common.cpp test_groups.cpp)
add_test(NAME test_v1_groups COMMAND test_server_message WORKING_DIRECTORY .)
IF(WIN32)	
	target_link_libraries(test_v1_groups ws2_32.lib openspy.lib)
ELSE() #unix
	target_link_libraries(test_v1_groups pthread openspy)
ENDIF()
