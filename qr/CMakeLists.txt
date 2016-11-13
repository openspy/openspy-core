cmake_minimum_required (VERSION 2.6)

project(qr)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file (GLOB MAIN_SRCS "*.cpp")
file (GLOB MAIN_HDRS "*.h")
file (GLOB SERVER_SRCS "server/*.cpp")
file (GLOB SERVER_HDRS "server/*.h")


set (ALL_SRCS ${MAIN_SRCS} ${MAIN_HDRS} ${SERVER_SRCS} ${SERVER_HDRS})

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

source_group("Sources" FILES ${MAIN_SRCS})
source_group("Sources\\Server" FILES ${SERVER_SRCS})

source_group("Headers" FILES ${MAIN_HDRS})
source_group("Headers\\Server" FILES ${SERVER_HDRS})

add_executable (qr ${ALL_SRCS})

IF(WIN32)	
	target_link_libraries(qr ws2_32.lib hiredis.lib Win32_Interop.lib openspy.lib)
ELSE() #unix
	target_link_libraries(qr hiredis event pthread openspy)
ENDIF()