find_package(NDI)
find_package(FLTK CONFIG REQUIRED)

# ---- Declare executable ----
add_executable(ndi-rist-encoder WIN32 ${CMAKE_CURRENT_LIST_DIR}/main.cpp ${CMAKE_CURRENT_LIST_DIR}/Url.cc ${CMAKE_CURRENT_LIST_DIR}/encode.cpp)

target_compile_features(ndi-rist-encoder PRIVATE cxx_std_20)

target_include_directories(ndi-rist-encoder PUBLIC ${FLTK_INCLUDE_DIRS})
target_include_directories(ndi-rist-encoder PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(ndi-rist-encoder PRIVATE ${NDI_INCLUDE_DIRS})

target_link_libraries(ndi-rist-encoder PRIVATE fmt::fmt rpc)
target_link_libraries(ndi-rist-encoder PRIVATE fltk fltk_gl fltk_forms fltk_images) 
target_link_libraries(ndi-rist-encoder PRIVATE ${NDI_LIBRARIES}) 
target_link_libraries(ndi-rist-encoder PUBLIC ristsender)

IF (WIN32)
    target_link_libraries(ndi-rist-encoder PRIVATE ${GSTREAMER_LIBRARIES})
    target_link_libraries(ndi-rist-encoder PRIVATE Threads::Threads)
ELSE()
    target_link_libraries(ndi-rist-encoder PRIVATE
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
ENDIF()



add_dependencies(ndi-rist-encoder ristsender rpc)
