cmake_minimum_required (VERSION 2.6)

project(openspy_core)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file (GLOB OS_SRCS "OS/*.cpp")
file (GLOB OS_HDRS "OS/*.h")
file (GLOB NET_SRCS "OS/Net/*.cpp")
file (GLOB NET_HDRS "OS/Net/*.h")

file (GLOB NET_EVENTMGR_SRCS "OS/Net/EventMgrs/*.cpp")
file (GLOB NET_EVENTMGR_HDRS "OS/Net/EventMgrs/*.h")
file (GLOB NET_IOIFACES_SRCS "OS/Net/IOIfaces/*.cpp")
file (GLOB NET_IOIFACES_HDRS "OS/Net/IOIfaces/*.h")

file (GLOB LEGACY_SRCS "OS/legacy/*.cpp" "OS/legacy/*.c")
file (GLOB LEGACY_HDRS "OS/legacy/*.h")
file (GLOB SEARCH_SRCS "OS/Search/*.cpp")
file (GLOB SEARCH_HDRS "OS/Search/*.h")
file (GLOB LOGGER_SRCS "OS/Logger/*.cpp")
file (GLOB LOGGER_HDRS "OS/Logger/*.h")
file (GLOB ANALYTICS_SRCS "OS/Analytics/*.cpp")
file (GLOB ANALYTICS_HDRS "OS/Analytics/*.h")
file (GLOB TIMER_SRCS "OS/Timer/*.cpp")
file (GLOB TIMER_HDRS "OS/Timer/*.h")


set (ALL_SRCS ${SERVER_SRCS} ${SERVER_HDRS} ${LEGACY_SRCS} ${LEGACY_HDRS} ${OS_SRCS} ${OS_HDRS} ${NET_SRCS} ${NET_HDRS} ${SEARCH_SRCS} ${SEARCH_HDRS} ${LOGGER_SRCS} ${LOGGER_HDRS} ${ANALYTICS_SRCS} ${ANALYTICS_HDRS} ${TIMER_SRCS} ${TIMER_HDRS} ${NET_EVENTMGR_SRCS} ${NET_IOIFACES_SRCS})

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

source_group("Sources\\OS\\Net" FILES ${NET_SRCS})
source_group("Sources\\OS\\Net\\EventMgrs" FILES ${NET_EVENTMGR_SRCS})
source_group("Sources\\OS\\Net\\IOFaces" FILES ${NET_IOIFACES_SRCS})
source_group("Sources\\OS\\Legacy" FILES ${LEGACY_SRCS})
source_group("Sources\\OS\\Search" FILES ${SEARCH_SRCS})
source_group("Sources\\OS\\Logger" FILES ${LOGGER_SRCS})


source_group("Headers\\OS\\Legacy" FILES ${LEGACY_HDRS})
source_group("Headers\\OS\\Net" FILES ${NET_HDRS})
source_group("Headers\\OS\\Net\\EventMgrs" FILES ${NET_EVENTMGR_HDRS})
source_group("Headers\\OS\\Net\\IOFaces" FILES ${NET_IOIFACES_HDRS})

source_group("Sources\\OS\\Search" FILES ${SEARCH_HDRS})
source_group("Sources\\OS\\Logger" FILES ${LOGGER_HDRS})


IF(WIN32)
	file(GLOB PLAT_SOCKETLIB_SRCS "OS/socketlib/Win32/*.cpp")
	source_group("Sources\\OS\\socketlib\\Win32\\" FILES ${PLAT_SOCKETLIB_SRCS})


	file(GLOB PLAT_THREAD_SRCS "OS/Threads/Win32/*.cpp")
	source_group("Sources\\OS\\Threads\\Win32\\" FILES ${PLAT_THREAD_SRCS})

	file(GLOB PLAT_THREAD_HDRS "OS/Threads/Win32/*.h")	
	source_group("Headers\\OS\\Threads\\Win32\\" FILES ${PLAT_THREAD_HDRS})


	set (ALL_PLAT ${PLAT_SOCKETLIB_SRCS} ${PLAT_THREAD_SRCS} ${PLAT_THREAD_HDRS})
	add_library (openspy STATIC ${ALL_SRCS} ${ALL_PLAT})
	target_link_libraries(openspy ws2_32.lib curl jansson)

ELSE() #unix
	file(GLOB PLAT_SOCKETLIB_SRCS "OS/socketlib/Unix/*.cpp")
	source_group("Sources\\OS\\socketlib\\Unix\\" FILES ${PLAT_SOCKETLIB_SRCS})
	
	file(GLOB PLAT_THREAD_SRCS "OS/Threads/PThreads/*.cpp")
	source_group("Sources\\OS\\Threads\\PThreads\\" FILES ${PLAT_THREAD_SRCS})

	file(GLOB PLAT_THREAD_HDRS "OS/Threads/PThreads/*.h")	
	source_group("Headers\\OS\\Threads\\PThreads\\" FILES ${PLAT_THREAD_HDRS})

	file(GLOB PLAT_LOGGER_SRCS "OS/Logger/Unix/*.cpp")
	source_group("Sources\\OS\\Logger\\Unix\\" FILES ${PLAT_LOGGER_SRCS})

	file(GLOB PLAT_LOGGER_HDRS "OS/Logger/Unix/*.h")	
	source_group("Headers\\OS\\Logger\\Unix\\" FILES ${PLAT_LOGGER_HDRS})

	file(GLOB PLAT_TIMER_SRCS "OS/Timer/Unix/*.cpp")
	source_group("Sources\\OS\\Timer\\Unix\\" FILES ${PLAT_TIMER_SRCS})

	file(GLOB PLAT_TIMER_HDRS "OS/Timer/Unix/*.h")	
	source_group("Headers\\OS\\Timer\\Unix\\" FILES ${PLAT_TIMER_HDRS})

	set (ALL_PLAT ${PLAT_SOCKETLIB_SRCS} ${PLAT_THREAD_SRCS} ${PLAT_THREAD_HDRS} ${PLAT_LOGGER_SRCS} ${PLAT_LOGGER_HDRS} ${PLAT_TIMER_SRCS} ${PLAT_TIMER_HDRS})
	add_library (openspy STATIC ${ALL_SRCS} ${ALL_PLAT})
	target_link_libraries(openspy pthread ssl crypto curl jansson)
ENDIF()
