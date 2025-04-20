cmake_minimum_required (VERSION 3.22)

project(openspy_SharedTasks)

find_package(jansson REQUIRED)
find_package(CURL REQUIRED)

file(GLOB MAIN_SRCS "src/OS/*.cpp" "src/OS/SharedTasks/Account/*.cpp" "src/OS/SharedTasks/Account/tasks/*.cpp" "src/OS/SharedTasks/Auth/*.cpp" "src/OS/SharedTasks/Auth/tasks/*.cpp" "src/OS/SharedTasks/CDKey/*.cpp" "src/OS/SharedTasks/CDKey/tasks/*.cpp" "src/OS/SharedTasks/Geo/*.cpp" "src/OS/SharedTasks/Geo/tasks/*.cpp")

SET(OPENSPY_SharedTasks_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} ${JANSSON_INCLUDE_DIRS} ${CURL_INCLUDE_DIR})
include_directories (${OPENSPY_SharedTasks_INCLUDE_DIRS})


add_library (SharedTasks SHARED ${MAIN_SRCS})

target_include_directories(SharedTasks PUBLIC "src/")
target_link_libraries(SharedTasks openspy ${CURL_LIBRARIES} ${JANSSON_LIBRARIES})
INSTALL(TARGETS SharedTasks
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  COMPONENT library
)
install(DIRECTORY "src/OS" DESTINATION include FILES_MATCHING PATTERN "*.h")
