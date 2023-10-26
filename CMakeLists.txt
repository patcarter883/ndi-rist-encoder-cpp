# CMakeList.txt : CMake project for ndi-rist-encoder, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

project (ndi-rist-encoder)

include(ExternalProject)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PKG_CONFIG_EXECUTABLE $ENV{GSTREAMER_1_0_ROOT_MSVC_X86_64}/bin/pkg-config.exe)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_subdirectory(fmt)

set(FLTK_DIR ../fltk/build)
find_package(FLTK REQUIRED NO_MODULE)

#Include librist
find_package (Threads REQUIRED)
ExternalProject_Add(project_rist
        CONFIGURE_COMMAND meson setup build --backend ninja -Dbuiltin_cjson=true --default-library=static --wipe
        GIT_REPOSITORY https://code.videolan.org/rist/librist.git
        GIT_TAG master
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BUILD_COMMAND meson compile -C build
        # BUILD_COMMAND mkdir -p ${CMAKE_CURRENT_SOURCE_DIR}/rist/build && cd ${CMAKE_CURRENT_SOURCE_DIR}/rist/build && meson -Dbuiltin_cjson=true --default-library=static .. && ninja
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

add_library(rist STATIC IMPORTED)
if(UNIX)
  set_property(TARGET rist PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
endif()
if(WIN32)
  set_property(TARGET rist PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
endif()

add_dependencies(rist project_rist)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include/librist)

set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4frame.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4hc.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/xxhash.c PROPERTIES GENERATED TRUE)


set(NDI_INCLUDE_DIR ./ndi-headers)
find_package(NDI REQUIRED)

if(true)
  find_package(PkgConfig REQUIRED)
  pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
  pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
  pkg_search_module(gstreamer-rtp REQUIRED IMPORTED_TARGET gstreamer-rtp-1.0>=1.4)
  pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
  pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
endif()

if(false)
  find_package(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
  # find_package(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
  # find_package(gstreamer-rtp REQUIRED IMPORTED_TARGET gstreamer-rtp-1.0>=1.4)
  # find_package(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
  # find_package(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)
endif()
# Add source to this project's executable.
add_executable (ndi_rist_encoder WIN32 "ndi-rist-encoder.cpp" "ndi-rist-encoder.h")
add_executable (rist_restreamer WIN32 "rist-restreamer.cpp" "rist-restreamer.h" )
add_library(ristnet STATIC
        RISTNet.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4frame.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4hc.c
        ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/xxhash.c
        )
target_link_libraries(ristnet rist Threads::Threads iphlpapi)

target_include_directories(ndi_rist_encoder PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/inc/)
target_include_directories(ndi_rist_encoder PRIVATE ${NDI_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${GSTREAMER_INCLUDE_DIRS})

target_include_directories(rist_restreamer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/inc/)

target_link_libraries(ndi_rist_encoder ristnet)
target_link_libraries(rist_restreamer ristnet)


if(true)
  target_link_libraries(ndi_rist_encoder
    fmt::fmt
    fltk
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
  target_link_libraries(ndi_rist_encoder rist ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a) 
  target_link_libraries(ndi_rist_encoder ${NDI_LIBRARIES}) 

  target_link_libraries(rist_restreamer
    fmt::fmt
    fltk
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
  target_link_libraries(rist_restreamer rist ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
endif()

if(false)
  target_link_libraries(ndi_rist_encoder
    fmt::fmt
    fltk)
  target_link_libraries(ndi_rist_encoder rist ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a) 
  target_link_libraries(ndi_rist_encoder ${NDI_LIBRARIES}) 
  target_link_libraries(ndi_rist_encoder ${GSTREAMER_LIBRARIES}) 

  target_link_libraries(rist_restreamer
    fmt::fmt
    fltk)
  target_link_libraries(rist_restreamer rist ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
  target_link_libraries(rist_restreamer ${GSTREAMER_LIBRARIES})
endif()

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET ndi_rist_encoder PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
