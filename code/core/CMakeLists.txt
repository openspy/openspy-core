cmake_minimum_required (VERSION 3.22)

project(openspy_core)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(SSL)
find_package(hiredis REQUIRED)
find_package(hiredis_ssl REQUIRED)
find_package(jansson REQUIRED)
find_package(pugixml REQUIRED)
find_package(rabbitmq-c REQUIRED)
find_package(CURL REQUIRED)


# get_cmake_property(_variableNames VARIABLES)
# list (SORT _variableNames)
# foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
# endforeach()

file (GLOB OS_SRCS "OS/*.cpp")
file (GLOB OS_HDRS "OS/*.h")
file (GLOB NET_SRCS "OS/Net/*.cpp")
file (GLOB NET_HDRS "OS/Net/*.h")

file (GLOB CONFIG_SRCS "OS/Config/*.cpp")
file (GLOB CONFIG_HDRS "OS/Config/*.h")

file (GLOB NET_PROCESSORS_SRCS "OS/Net/Processors/*.cpp")
file (GLOB NET_PROCESSORS_HDRS "OS/Net/Processors/*.h")

file (GLOB NET_DRIVERS_SRCS "OS/Net/drivers/*.cpp")
file (GLOB NET_DRIVERS_HDRS "OS/Net/drivers/*.h")


file (GLOB LEGACY_SRCS "OS/gamespy/*.cpp" "OS/gamespy/*.c")
file (GLOB LEGACY_HDRS "OS/gamespy/*.h")
file (GLOB LOGGER_SRCS "OS/Logger/*.cpp")
file (GLOB LOGGER_HDRS "OS/Logger/*.h")


set (ALL_SRCS ${SSL_SRCS} ${SERVER_SRCS} ${SERVER_HDRS} ${LEGACY_SRCS} ${LEGACY_HDRS} ${CONFIG_SRCS} ${CONFIG_HDRS} ${OS_SRCS} ${OS_HDRS} ${NET_SRCS} ${NET_HDRS} ${NET_DRIVERS_SRCS} ${NET_DRIVERS_HDRS} ${LOGGER_SRCS} ${LOGGER_HDRS} ${NET_PROCESSORS_SRCS} ${NET_PROCESSORS_HDRS} )

include_directories (${CMAKE_CURRENT_SOURCE_DIR} ${CURL_INCLUDE_DIR} ${hiredis_INCLUDE_DIRS}  ${hiredis_ssl_INCLUDE_DIRS} ${SSL_INCS})

source_group("Sources\\OS\\Net" FILES ${NET_SRCS})
source_group("Sources\\OS\\Net\\Drivers" FILES ${NET_DRIVERS_SRCS})
source_group("Sources\\OS\\Gamespy" FILES ${LEGACY_SRCS})
source_group("Sources\\OS\\Logger" FILES ${LOGGER_SRCS})


source_group("Headers\\OS\\Gamespy" FILES ${LEGACY_HDRS})
source_group("Headers\\OS\\Net" FILES ${NET_HDRS})
source_group("Headers\\OS\\Net\\Drivers" FILES ${NET_DRIVERS_SRCS})

source_group("Sources\\OS\\Logger" FILES ${LOGGER_HDRS})


source_group("Sources\\OS\\MessageQueue" FILES ${MQ_SRCS})
source_group("Headers\\OS\\MessageQueue" FILES ${MQ_HDRS})


IF(WIN32)
	file(GLOB PLAT_LOGGER_SRCS "OS/Logger/Win32/*.cpp")
	source_group("Sources\\OS\\Logger\\Win32\\" FILES ${PLAT_LOGGER_SRCS})

	file(GLOB PLAT_LOGGER_HDRS "OS/Logger/Win32/*.h")	
	source_group("Headers\\OS\\Logger\\Win32\\" FILES ${PLAT_LOGGER_HDRS})

	set (ALL_PLAT ${PLAT_LOGGER_SRCS} ${PLAT_LOGGER_HDRS})
ELSE() #unix
	file(GLOB PLAT_LOGGER_SRCS "OS/Logger/Unix/*.cpp")
	source_group("Sources\\OS\\Logger\\Unix\\" FILES ${PLAT_LOGGER_SRCS})

	file(GLOB PLAT_LOGGER_HDRS "OS/Logger/Unix/*.h")	
	source_group("Headers\\OS\\Logger\\Unix\\" FILES ${PLAT_LOGGER_HDRS})

	set (ALL_PLAT ${PLAT_LOGGER_SRCS} ${PLAT_LOGGER_HDRS})

	
ENDIF()
add_library (openspy SHARED ${ALL_SRCS} ${ALL_PLAT})
target_link_libraries(openspy ${CURL_LIBRARIES} ZLIB::ZLIB pugixml rabbitmq::rabbitmq-static libuv::uv jansson::jansson hiredis::hiredis hiredis::hiredis_ssl ${SSL_LIBS})

INSTALL(TARGETS openspy
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  COMPONENT library
)
install(DIRECTORY "OS" DESTINATION include FILES_MATCHING PATTERN "*.h")
install(DIRECTORY "SSL/OpenSSL/" DESTINATION include FILES_MATCHING PATTERN "*.h")

if(BUILD_TESTING)
add_subdirectory(test)
endif()
