cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(cmake/prelude.cmake)

project(
    ndi-rist
    VERSION 0.1.0
    DESCRIPTION "NDI to RTMP streamer using AV1 over MPEGTS"
    HOMEPAGE_URL "https://github.com/patcarter883/ndi-rist-encoder-cpp.git"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive --remote
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init --recursive failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

# ---- Declare library ----

# add_library(
#     ndi-rist_lib OBJECT
#     source/lib.cpp
# )

# target_include_directories(
#     ndi-rist_lib ${warning_guard}
#     PUBLIC
#     "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
# )

# target_compile_features(ndi-rist_lib PUBLIC cxx_std_20)

# find_package(fmt REQUIRED)
# target_link_libraries(ndi-rist_lib PRIVATE fmt::fmt)

IF (WIN32)

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

find_package(NDI REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(FLTK CONFIG REQUIRED)
add_subdirectory(extern/rpclib)
add_subdirectory(extern/rist-cpp)

# ---- Declare executable ----

add_executable(ndi-rist-encoder_exe WIN32 source/ndi-rist-encoder.cpp source/Url.h source/Url.cc)
add_executable(ndi-rist-encoder::exe ALIAS ndi-rist-encoder_exe)

set_property(TARGET ndi-rist-encoder_exe PROPERTY OUTPUT_NAME ndi-rist-encoder)

target_compile_features(ndi-rist-encoder_exe PRIVATE cxx_std_20)

target_include_directories(ndi-rist-encoder_exe PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi-rist-encoder_exe PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(ndi-rist-encoder_exe PRIVATE ${NDI_INCLUDE_DIRS})

target_link_libraries(ndi-rist-encoder_exe PRIVATE fmt::fmt rpc)
target_link_libraries(ndi-rist-encoder_exe PRIVATE fltk fltk_gl fltk_forms fltk_images) 
target_link_libraries(ndi-rist-encoder_exe PRIVATE ${NDI_LIBRARIES}) 

IF (WIN32)

    target_link_libraries(ndi-rist-encoder_exe PRIVATE ${GSTREAMER_LIBRARIES})
    target_link_libraries(ndi-rist-encoder_exe PRIVATE Threads::Threads)

ELSE()

    target_link_libraries(ndi-rist-encoder_exe PRIVATE
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
ENDIF()

target_link_libraries(ndi-rist-encoder_exe PRIVATE rpc ristnet)

add_dependencies(ndi-rist-encoder_exe ristnet rpc)

# ---- Declare executable ----

add_executable(ndi-rist-server_exe WIN32 source/ndi-rist-server.cpp)
add_executable(ndi-rist-server::exe ALIAS ndi-rist-server_exe)

set_property(TARGET ndi-rist-server_exe PROPERTY OUTPUT_NAME ndi-rist-server)

target_compile_features(ndi-rist-server_exe PRIVATE cxx_std_20)

target_include_directories(ndi-rist-server_exe PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(ndi-rist-server_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/)
target_include_directories(ndi-rist-server_exe PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/inc/ ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/include/ ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/include/librist ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/build/include/librist)

IF (WIN32)

    target_link_libraries(ndi-rist-server_exe PRIVATE ${GSTREAMER_LIBRARIES})
    target_link_libraries(ndi-rist-server_exe PRIVATE Threads::Threads)

ELSE()

    target_link_libraries(ndi-rist-server_exe PRIVATE
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
ENDIF()

target_link_libraries(ndi-rist-server_exe PRIVATE rpc ristnet)

add_dependencies(ndi-rist-server_exe rpc ristnet)
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
