cmake_minimum_required (VERSION 2.6)

project(openspy)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories("core")


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

source_group("" FILES ${MAIN_SOURCES})
source_group("Legacy" FILES ${LEGACY_SOURCES})
source_group("Server" FILES ${SERVER_SOURCES})
source_group("socketlib" FILES ${EXTERNAL_SOURCES})


add_subdirectory(core)
add_subdirectory(serverbrowsing)
add_subdirectory(qr)
