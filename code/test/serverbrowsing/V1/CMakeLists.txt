cmake_minimum_required (VERSION 3.22)

project(serverbrowsing_v1_test)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

add_executable (test_sbv1_groups ${ALL_SRCS} test_common.cpp test_groups.cpp)
add_test(NAME test_sbv1_groups COMMAND test_sbv1_groups WORKING_DIRECTORY .)
IF(WIN32)	
	target_link_libraries(test_sbv1_groups openspy.lib)
ELSE() #unix
	target_link_libraries(test_sbv1_groups openspy)
ENDIF()
