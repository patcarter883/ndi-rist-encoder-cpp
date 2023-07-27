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
include(ExternalProject)
ExternalProject_Add(project_libristnet
        GIT_REPOSITORY https://code.videolan.org/rist/rist-cpp.git
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
        GIT_PROGRESS 1
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap --config ${CMAKE_BUILD_TYPE} --target ristnet
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )
add_library(ristnet STATIC IMPORTED)
set_property(TARGET ristnet PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/libristnet.a)


#Pass information about where to find headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/include/librist)

find_package(NDI REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

# Add source to this project's executable.
add_executable (ndi_rist_encoder WIN32 "ndi-rist-encoder.cpp" "ndi-rist-encoder.h"  "ristsender.c")

target_include_directories(ndi_rist_encoder PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/)
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/inc/)
target_include_directories(ndi_rist_encoder PRIVATE ${NDI_INCLUDE_DIRS})

target_link_libraries(ndi_rist_encoder
  fmt::fmt
  fltk
  PkgConfig::gstreamer
  PkgConfig::gstreamer-sdp
  PkgConfig::gstreamer-app
  PkgConfig::gstreamer-video)
target_link_libraries(ndi_rist_encoder ristnet ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/librist.a) 
target_link_libraries(ndi_rist_encoder ${NDI_LIBRARIES}) 

add_dependencies(ristnet project_libristnet)
add_dependencies(ndi_rist_encoder ristnet)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ndi_rist_encoder PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
