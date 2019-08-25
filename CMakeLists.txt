cmake_minimum_required(VERSION 3.5)
project(pvr.octonet)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(p8-platform REQUIRED)
find_package(JsonCpp REQUIRED)

include_directories(
	${p8-platform_INCLUDE_DIRS}
	${KODI_INCLUDE_DIR}
	${JSONCPP_INCLUDE_DIRS})

set(DEPLIBS
	${p8-platform_LIBRARIES}
	${JSONCPP_LIBRARIES})

set(OCTONET_SOURCES
	src/OctonetData.cpp
	src/client.cpp
	src/Socket.cpp
	src/rtsp_client.cpp)

set(OCTONET_HEADERS
	src/client.h
	src/OctonetData.h
	src/Socket.h)

build_addon(pvr.octonet OCTONET DEPLIBS)

if(WIN32)
	if(NOT CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
		target_link_libraries(pvr.octonet wsock32 ws2_32)
	else()
		target_link_libraries(pvr.octonet ws2_32)
	endif()
endif()

include(CPack)
