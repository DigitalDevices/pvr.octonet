project(pvr.octonet)

cmake_minimum_required(VERSION 2.6)

enable_language(CXX)

find_package(kodi REQUIRED)
find_package(kodiplatform REQUIRED)
find_package(platform REQUIRED)

include_directories(
	${kodiplatform_INCLUDE_DIRS}
	${platform_INCLUDE_DIRS}
	${KODI_INCLUDE_DIR})

set(DEPLIBS
	${platform_LIBRARIES})

set(OCTONET_SOURCES
	src/client.cpp)

build_addon(pvr.octonet OCTONET DEPLIBS)

include(CPack)
