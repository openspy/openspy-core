cmake_minimum_required (VERSION 2.6)

project(openspy_core)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file (GLOB OS_SRCS "OS/*.cpp")
file (GLOB OS_HDRS "OS/*.h")
file (GLOB NET_SRCS "OS/Net/*.cpp")
file (GLOB NET_HDRS "OS/Net/*.h")

file (GLOB CONFIG_SRCS "OS/Config/*.cpp")
file (GLOB CONFIG_HDRS "OS/Config/*.h")

file (GLOB NET_EVENTMGR_SRCS "OS/Net/EventMgrs/*.cpp")
file (GLOB NET_EVENTMGR_HDRS "OS/Net/EventMgrs/*.h")
file (GLOB NET_IOIFACES_SRCS "OS/Net/IOIfaces/*.cpp")
file (GLOB NET_IOIFACES_HDRS "OS/Net/IOIfaces/*.h")
file (GLOB NET_PROCESSORS_SRCS "OS/Net/Processors/*.cpp")
file (GLOB NET_PROCESSORS_HDRS "OS/Net/Processors/*.h")


file (GLOB LEGACY_SRCS "OS/gamespy/*.cpp" "OS/gamespy/*.c")
file (GLOB LEGACY_HDRS "OS/gamespy/*.h")
file (GLOB LOGGER_SRCS "OS/Logger/*.cpp")
file (GLOB LOGGER_HDRS "OS/Logger/*.h")
file (GLOB TIMER_SRCS "OS/Timer/*.cpp")
file (GLOB TIMER_HDRS "OS/Timer/*.h")
file (GLOB TASK_HDRS "OS/Task/*.h")

file (GLOB SHAREDTASK_ACCOUNT_SRCS "OS/SharedTasks/Account/*.cpp")
file (GLOB SHAREDTASK_AUTH_SRCS "OS/SharedTasks/Auth/*.cpp")
file (GLOB SHAREDTASK_CDKEY_SRCS "OS/SharedTasks/CDKey/*.cpp")
file (GLOB SHAREDTASK_ACCOUNT_TASKS_SRCS "OS/SharedTasks/Account/tasks/*.cpp")
file (GLOB SHAREDTASK_AUTH_TASKS_SRCS "OS/SharedTasks/Auth/tasks/*.cpp")
file (GLOB SHAREDTASK_CDKEY_TASKS_SRCS "OS/SharedTasks/CDKey/tasks/*.cpp")
file (GLOB SHAREDTASK_HDRS "OS/SharedTasks/*.h")
file (GLOB SHAREDTASK_ACCOUNT_HDRS "OS/SharedTasks/Account/*.h")
file (GLOB SHAREDTASK_AUTH_HDRS "OS/SharedTasks/Auth/*.h")
file (GLOB SHAREDTASK_CDKEY_HDRS "OS/SharedTasks/CDKey/*.h")


file (GLOB MQ_SRCS "OS/MessageQueue/*.cpp" "OS/MessageQueue/rabbitmq/*.cpp")
file (GLOB MQ_HDRS "OS/MessageQueue/*.h" "OS/MessageQueue/rabbitmq/*.h")

set (ALL_SRCS ${SERVER_SRCS} ${SERVER_HDRS} ${LEGACY_SRCS} ${LEGACY_HDRS} ${CONFIG_SRCS} ${CONFIG_HDRS} ${OS_SRCS} ${OS_HDRS} ${NET_SRCS} ${NET_HDRS} ${LOGGER_SRCS} ${LOGGER_HDRS} ${TIMER_SRCS} ${TIMER_HDRS} ${NET_EVENTMGR_SRCS} ${NET_IOIFACES_SRCS} ${NET_PROCESSORS_SRCS} ${NET_PROCESSORS_HDRS} ${MQ_SRCS} ${MQ_HDRS} ${TASK_HDRS} ${SHAREDTASK_ACCOUNT_SRCS} ${SHAREDTASK_AUTH_SRCS} ${SHAREDTASK_ACCOUNT_TASKS_SRCS} ${SHAREDTASK_AUTH_TASKS_SRCS} ${SHAREDTASK_CDKEY_SRCS} ${SHAREDTASK_CDKEY_TASKS_SRCS} ${SHAREDTASK_CDKEY_HDRS})

include_directories (${CMAKE_CURRENT_SOURCE_DIR})

source_group("Sources\\OS\\Net" FILES ${NET_SRCS})
source_group("Sources\\OS\\Net\\EventMgrs" FILES ${NET_EVENTMGR_SRCS})
source_group("Sources\\OS\\Net\\IOFaces" FILES ${NET_IOIFACES_SRCS})
source_group("Sources\\OS\\Gamespy" FILES ${LEGACY_SRCS})
source_group("Sources\\OS\\Logger" FILES ${LOGGER_SRCS})


source_group("Headers\\OS\\Gamespy" FILES ${LEGACY_HDRS})
source_group("Headers\\OS\\Net" FILES ${NET_HDRS})
source_group("Headers\\OS\\Net\\EventMgrs" FILES ${NET_EVENTMGR_HDRS})
source_group("Headers\\OS\\Net\\IOFaces" FILES ${NET_IOIFACES_HDRS})

source_group("Sources\\OS\\Logger" FILES ${LOGGER_HDRS})


source_group("Sources\\OS\\MessageQueue" FILES ${MQ_SRCS})
source_group("Headers\\OS\\MessageQueue" FILES ${MQ_HDRS})


IF(WIN32)

	file(GLOB PLAT_THREAD_SRCS "OS/Threads/Win32/*.cpp")
	source_group("Sources\\OS\\Threads\\Win32\\" FILES ${PLAT_THREAD_SRCS})

	file(GLOB PLAT_THREAD_HDRS "OS/Threads/Win32/*.h")	
	source_group("Headers\\OS\\Threads\\Win32\\" FILES ${PLAT_THREAD_HDRS})

	file(GLOB PLAT_LOGGER_SRCS "OS/Logger/Win32/*.cpp")
	source_group("Sources\\OS\\Logger\\Win32\\" FILES ${PLAT_LOGGER_SRCS})

	file(GLOB PLAT_LOGGER_HDRS "OS/Logger/Win32/*.h")	
	source_group("Headers\\OS\\Logger\\Win32\\" FILES ${PLAT_LOGGER_HDRS})

	file(GLOB PLAT_TIMER_SRCS "OS/Timer/Win32/*.cpp")
	source_group("Sources\\OS\\Timer\\Win32\\" FILES ${PLAT_TIMER_SRCS})

	file(GLOB PLAT_TIMER_HDRS "OS/Timer/Win32/*.h")	
	source_group("Headers\\OS\\Timer\\Win32\\" FILES ${PLAT_TIMER_HDRS})

	set (ALL_PLAT ${PLAT_THREAD_SRCS} ${PLAT_THREAD_HDRS} ${PLAT_LOGGER_SRCS} ${PLAT_LOGGER_HDRS} ${PLAT_TIMER_SRCS} ${PLAT_TIMER_HDRS})
	add_library (openspy STATIC ${ALL_SRCS} ${ALL_PLAT})
	target_link_libraries(openspy ws2_32.lib curl jansson libcurl_imp pugixml libeay32 ssleay32)

ELSE() #unix
	
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

	set (ALL_PLAT ${PLAT_THREAD_SRCS} ${PLAT_THREAD_HDRS} ${PLAT_LOGGER_SRCS} ${PLAT_LOGGER_HDRS} ${PLAT_TIMER_SRCS} ${PLAT_TIMER_HDRS})
	add_library (openspy STATIC ${ALL_SRCS} ${ALL_PLAT})
	target_link_libraries(openspy pthread ssl crypto curl jansson pugixml rabbitmq)
ENDIF()
