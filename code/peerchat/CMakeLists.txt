cmake_minimum_required (VERSION 3.22)

project(peerchat)
find_package(SSL)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file (GLOB MAIN_SRCS "*.cpp") 
file (GLOB MAIN_HDRS "*.h")
file (GLOB SERVER_SRCS "server/*.cpp")
file (GLOB SERVER_HDRS "server/*.h")
file (GLOB SERVER_TASKS_SRCS "tasks/*.cpp")
file (GLOB SERVER_TASKS_HDRS "tasks/*.h")
file (GLOB SERVER_CMDS_SRCS "server/commands/*.cpp")
file (GLOB SERVER_CMDS_HDRS "server/commands/*.h")


set (ALL_SRCS ${MAIN_SRCS} ${MAIN_HDRS} ${SERVER_SRCS} ${SERVER_HDRS} ${SERVER_CMDS_SRCS} ${SERVER_CMDS_HDRS} ${SERVER_TASKS_SRCS} ${SERVER_TASKS_HDRS})

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${SSL_OS_INC} ${SSL_INCS})

source_group("Sources" FILES ${MAIN_SRCS})
source_group("Sources\\Server" FILES ${SERVER_SRCS})
source_group("Sources\\Server\\Tasks" FILES ${SERVER_TASKS_SRCS})
source_group("Sources\\Server\\Commands" FILES ${SERVER_CMDS_SRCS})

source_group("Headers" FILES ${MAIN_HDRS})
source_group("Headers\\Server" FILES ${SERVER_HDRS})
source_group("Headers\\Server\\Tasks" FILES ${SERVER_TASKS_HDRS})
source_group("Headers\\Server\\Commands" FILES ${SERVER_CMDS_HDRS})

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIR})


add_executable (peerchat ${ALL_SRCS})

IF(WIN32)	
	target_link_libraries(peerchat openspy.lib SharedTasks.lib ZLIB::ZLIB)
ELSE() #unix
	target_link_libraries(peerchat openspy SharedTasks ZLIB::ZLIB)
ENDIF()