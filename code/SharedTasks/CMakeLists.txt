cmake_minimum_required (VERSION 3.22)

project(openspy_SharedTasks)

find_package(Jansson REQUIRED)
find_package(CURL REQUIRED)

file(GLOB MAIN_SRCS "src/OS/*.cpp" "src/OS/SharedTasks/Account/*.cpp" "src/OS/SharedTasks/Auth/*.cpp" "src/OS/SharedTasks/CDKey/*.cpp" "src/OS/SharedTasks/Geo/*.cpp")

SET(OPENSPY_SharedTasks_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${JANSSON_INCLUDE_DIRS})
include_directories (${OPENSPY_SharedTasks_INCLUDE_DIRS})


add_library (SharedTasks SHARED ${MAIN_SRCS})

target_include_directories(SharedTasks PUBLIC "src/")
target_link_libraries(SharedTasks openspy ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
