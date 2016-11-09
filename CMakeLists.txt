cmake_minimum_required (VERSION 2.6)

project(serverbrowsing)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file (GLOB MAIN_SOURCES "*.cpp" "*.h")
file (GLOB SOCKET_SOURCES "socketlib/*.cpp" "socketlib/*.h")
file (GLOB SERVER_SOURCES "server/*.cpp" "server/*.h")
file (GLOB LEGACY_SOURCES "legacy/*.cpp" "legacy/*.h")


set (ALL_SRCS ${MAIN_SOURCES} ${SOCKET_SOURCES} ${SERVER_SOURCES} ${LEGACY_SOURCES})

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

source_group("" FILES ${MAIN_SOURCES})
source_group("Legacy" FILES ${LEGACY_SOURCES})
source_group("Server" FILES ${SERVER_SOURCES})
source_group("socketlib" FILES ${EXTERNAL_SOURCES})



IF(WIN32)
add_executable (serverbrowsing ${ALL_SRCS})
target_link_libraries(serverbrowsing ws2_32.lib hiredis.lib Win32_Interop.lib)

ELSE() #unix
add_executable (serverbrowsing ${ALL_SRCS})
target_link_libraries(serverbrowsing hiredis)
ENDIF()