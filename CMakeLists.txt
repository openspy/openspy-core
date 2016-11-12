cmake_minimum_required (VERSION 2.6)

project(openspy)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include_directories("core")


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/")

add_subdirectory(core)
add_subdirectory(serverbrowsing)