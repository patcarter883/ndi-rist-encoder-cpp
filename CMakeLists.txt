# CMakeList.txt : CMake project for ndi-rist-encoder, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

project (ndi-rist-encoder)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(fmt)

set(FLTK_DIR ../fltk/build/${CMAKE_BUILD_TYPE})
find_package(FLTK REQUIRED NO_MODULE)

#Include libristnet

add_library(rist-cpp STATIC IMPORTED)
set_property(TARGET rist-cpp PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/libristnet.a)


#Pass information about where to find headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/rist/build/include/librist)

set(NDI_INCLUDE_DIR ./ndi-headers)
find_package(NDI REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-rtp REQUIRED IMPORTED_TARGET gstreamer-rtp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

# Add source to this project's executable.
add_executable (ndi_rist_encoder WIN32 "ndi-rist-encoder.cpp" "ndi-rist-encoder.h"  "ristsender.c")

target_include_directories(ndi_rist_encoder PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/)
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/rist/inc/)
target_include_directories(ndi_rist_encoder PRIVATE ${NDI_INCLUDE_DIRS})

target_link_libraries(ndi_rist_encoder
  fmt::fmt
  fltk
  PkgConfig::gstreamer
  PkgConfig::gstreamer-sdp
  PkgConfig::gstreamer-rtp
  PkgConfig::gstreamer-app
  PkgConfig::gstreamer-video)
target_link_libraries(ndi_rist_encoder rist-cpp ${CMAKE_CURRENT_SOURCE_DIR}/rist-cpp/rist/build/librist.a) 
target_link_libraries(ndi_rist_encoder ${NDI_LIBRARIES}) 


if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ndi_rist_encoder PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
