cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/CPM.cmake)
include(cmake/prelude.cmake)

project(
    ndi-rist
    VERSION 0.2.0
    DESCRIPTION "NDI to RTMP streamer using AV1 over MPEGTS"
    HOMEPAGE_URL "https://github.com/patcarter883/ndi-rist-encoder-cpp.git"
    LANGUAGES C CXX
)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)
 

IF (WIN32)
    add_compile_definitions(WIN32 WIN32_LEAN_AND_MEAN)
    find_package(GStreamer REQUIRED)
    find_package (Threads)

ELSE()
    find_package(PkgConfig REQUIRED)
    pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
    pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
    pkg_search_module(gstreamer-rtp REQUIRED IMPORTED_TARGET gstreamer-rtp-1.0>=1.4)
    pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
    pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

ENDIF()

CPMAddPackage("gh:rpclib/rpclib#v2.3.0")
find_package(fmt CONFIG REQUIRED)
add_subdirectory(libs)
add_subdirectory(apps/ndi-rist-encoder)
add_subdirectory(apps/ndi-rist-server)


# ---- Install rules ----

if(NOT CMAKE_SKIP_INSTALL_RULES)
  include(cmake/install-rules.cmake)
endif()

# ---- Developer mode ----

if(NOT ndi-rist_DEVELOPER_MODE)
  return()
elseif(NOT PROJECT_IS_TOP_LEVEL)
  message(
      AUTHOR_WARNING
      "Developer mode is intended for developers of ndi-rist"
  )
endif()

include(cmake/dev-mode.cmake)
