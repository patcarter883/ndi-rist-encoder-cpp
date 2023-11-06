# CMakeList.txt : CMake project for ndi-rist-encoder, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

project (ndi-rist-encoder)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(ExternalProject)
#Include libristnet
# ExternalProject_Add(project_libristnet
#         GIT_REPOSITORY https://github.com/patcarter883/rist-cpp.git
#         GIT_TAG swxtch
#         SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
#         BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
#         GIT_PROGRESS 1
#         CONFIGURE_COMMAND cmake -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -S${CMAKE_CURRENT_SOURCE_DIR}/ristwrap -B${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/build -G "NMake Makefiles"
#         BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/build --config ${CMAKE_BUILD_TYPE} --target ristnet
#         STEP_TARGETS build
#         EXCLUDE_FROM_ALL TRUE
#         INSTALL_COMMAND ""
#         )

add_library(ristnet STATIC IMPORTED)
set_property(TARGET ristnet PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/build/ristnet.lib)


#Pass information about where to find headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/include/librist)

find_package(NDI REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(FLTK CONFIG REQUIRED)
find_package(GStreamer REQUIRED)
find_package (Threads)

# Add source to this project's executable.
add_executable (ndi_rist_encoder WIN32 "ndi-rist-encoder.cpp" "ndi-rist-encoder.h")
# add_executable (rist_restreamer WIN32 "rist-restreamer.cpp" "rist-restreamer.h" )

target_include_directories(ndi_rist_encoder PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/)
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/inc/)
target_include_directories(ndi_rist_encoder PRIVATE ${NDI_INCLUDE_DIRS})

# target_include_directories(rist_restreamer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/)
# target_include_directories(rist_restreamer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/inc/)

target_link_libraries(ndi_rist_encoder PRIVATE fmt::fmt)
target_link_libraries(ndi_rist_encoder PRIVATE fltk fltk_gl fltk_forms fltk_images)

target_link_libraries(ndi_rist_encoder PRIVATE ristnet ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/librist.a) 
target_link_libraries(ndi_rist_encoder PRIVATE ${NDI_LIBRARIES}) 
target_link_libraries(ndi_rist_encoder PRIVATE ${GSTREAMER_LIBRARIES}) 
target_link_libraries(ndi_rist_encoder PRIVATE iphlpapi wsock32 ws2_32 Threads::Threads)

# target_link_libraries(rist_restreamer
#   fmt::fmt
#   fltk
#   PkgConfig::gstreamer
#   PkgConfig::gstreamer-sdp
#   PkgConfig::gstreamer-rtp
#   PkgConfig::gstreamer-app
#   PkgConfig::gstreamer-video)
# target_link_libraries(rist_restreamer ristnet ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/librist.a)

add_dependencies(ndi_rist_encoder ristnet)
add_dependencies(ristnet project_libristnet)


# TODO: Add tests and install targets if needed.
