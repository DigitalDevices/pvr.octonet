cmake_minimum_required(VERSION 3.5)
project(pvr.octonet)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR})

find_package(Kodi REQUIRED)
find_package(JsonCpp REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/.. # Hack way with "/..", need bigger Kodi cmake rework to match right include ways
                    ${JSONCPP_INCLUDE_DIRS})

set(DEPLIBS ${JSONCPP_LIBRARIES})

set(OCTONET_SOURCES src/addon.cpp
                    src/OctonetData.cpp
                    src/Socket.cpp
                    src/rtsp_client.cpp)

set(OCTONET_HEADERS src/addon.h
                    src/OctonetData.h
                    src/Socket.h
                    src/rtsp_client.hpp)

addon_version(pvr.octonet OCTONET)
add_definitions(-DOCTONET_VERSION=${OCTONET_VERSION})

build_addon(pvr.octonet OCTONET DEPLIBS)

if(WIN32)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL WindowsStore)
    target_link_libraries(pvr.octonet wsock32 ws2_32)
  else()
    target_link_libraries(pvr.octonet ws2_32)
  endif()
endif()

include(CPack)
