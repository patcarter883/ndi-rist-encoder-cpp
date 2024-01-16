# ---- Declare executable ----

add_executable(ndi-rist-server WIN32 ${CMAKE_CURRENT_LIST_DIR}/main.cpp)

target_compile_features(ndi-rist-server PRIVATE cxx_std_20)

target_include_directories(ndi-rist-server PUBLIC ${GSTREAMER_INCLUDE_DIRS})
target_include_directories(ndi-rist-server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/)
target_include_directories(ndi-rist-server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/inc/ ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/include/ ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/include/librist ${CMAKE_CURRENT_SOURCE_DIR}/extern/rist-cpp/rist/build/include/librist)

IF (WIN32)

    target_link_libraries(ndi-rist-server PRIVATE ${GSTREAMER_LIBRARIES})
    target_link_libraries(ndi-rist-server PRIVATE Threads::Threads)

ELSE()

    target_link_libraries(ndi-rist-server PRIVATE
    PkgConfig::gstreamer
    PkgConfig::gstreamer-sdp
    PkgConfig::gstreamer-rtp
    PkgConfig::gstreamer-app
    PkgConfig::gstreamer-video)
ENDIF()

target_link_libraries(ndi-rist-server PRIVATE rpc fmt::fmt ristreceiver)

add_dependencies(ndi-rist-server rpc ristreceiver)
