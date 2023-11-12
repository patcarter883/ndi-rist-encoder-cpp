# CMakeList.txt : CMake project for ndi-rist-encoder, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

project (ndi-rist-encoder)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add source to this project's executable.
add_executable (ndi_rist_encoder WIN32 "ndi-rist-encoder.cpp" "ndi-rist-encoder.h")
# add_executable (rist_restreamer WIN32 "rist-restreamer.cpp" "rist-restreamer.h" )

include(ExternalProject)
    
#Include libristnet
ExternalProject_Add(project_libristnet
        GIT_REPOSITORY https://github.com/patcarter883/rist-cpp.git
        GIT_TAG swxtch
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap
        GIT_PROGRESS 1
        CONFIGURE_COMMAND cmake -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -S${CMAKE_CURRENT_SOURCE_DIR}/ristwrap -B${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/build -G "$<IF:$<BOOL:${WIN32}>,NMake Makefiles,Ninja>"
        BUILD_COMMAND cmake --build ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/build --config ${CMAKE_BUILD_TYPE} --target ristnet
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        )

add_library(ristnet STATIC IMPORTED)
if (${WIN32})
    set(RIST_LIB_NAME ristnet.lib)
else()
    set(RIST_LIB_NAME libristnet.a)
endif()
set_property(TARGET ristnet PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/build/${RIST_LIB_NAME})


#Pass information about where to find headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/include/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/include/librist)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/include/librist)

find_package(NDI REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(FLTK CONFIG REQUIRED)

IF (WIN32)

    find_package(GStreamer REQUIRED)
    find_package (Threads)

    target_link_libraries(ndi_rist_encoder PRIVATE ${GSTREAMER_LIBRARIES}) 
    target_link_libraries(ndi_rist_encoder PRIVATE iphlpapi wsock32 ws2_32 Threads::Threads)

ELSE()
    find_package(PkgConfig REQUIRED)
    pkg_search_module(gstreamer REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.4)
    pkg_search_module(gstreamer-sdp REQUIRED IMPORTED_TARGET gstreamer-sdp-1.0>=1.4)
    pkg_search_module(gstreamer-rtp REQUIRED IMPORTED_TARGET gstreamer-rtp-1.0>=1.4)
    pkg_search_module(gstreamer-app REQUIRED IMPORTED_TARGET gstreamer-app-1.0>=1.4)
    pkg_search_module(gstreamer-video REQUIRED IMPORTED_TARGET gstreamer-video-1.0>=1.4)

    target_link_libraries(ndi_rist_encoder PRIVATE
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
ENDIF()


target_include_directories(ndi_rist_encoder PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/)
target_include_directories(ndi_rist_encoder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/inc/)
target_include_directories(ndi_rist_encoder PRIVATE ${NDI_INCLUDE_DIRS})

target_link_libraries(ndi_rist_encoder PRIVATE fmt::fmt)
target_link_libraries(ndi_rist_encoder PRIVATE fltk fltk_gl fltk_forms fltk_images)

target_link_libraries(ndi_rist_encoder PRIVATE ristnet ${CMAKE_CURRENT_SOURCE_DIR}/ristwrap/rist/build/librist.a) 
target_link_libraries(ndi_rist_encoder PRIVATE ${NDI_LIBRARIES}) 

add_dependencies(ndi_rist_encoder ristnet)
add_dependencies(ristnet project_libristnet)

install(TARGETS ndi_rist_encoder RUNTIME DESTINATION bin)
include(CPack)


# TODO: Add tests and install targets if needed.
