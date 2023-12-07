set(CMAKE_C_STANDARD 99)

include(ExternalProject)

ExternalProject_Add(project_rist
        CONFIGURE_COMMAND ""
        GIT_REPOSITORY https://github.com/patcarter883/librist.git
        GIT_TAG master
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        BINARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/rist
        #BUILD_COMMAND meson setup build --backend $<IF:$<BOOL:${WIN32}>,vs,ninja> -Dbuiltin_cjson=true -Dbuiltin_mbedtls=true --default-library=static && meson compile -C build
        BUILD_COMMAND meson setup build --backend ninja -Dbuiltin_cjson=true -Dbuiltin_mbedtls=true --default-library=static && meson compile -C build
        GIT_PROGRESS 1
        STEP_TARGETS build
        EXCLUDE_FROM_ALL TRUE
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a
        )

add_library(librist STATIC IMPORTED)
set_property(TARGET librist PROPERTY IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/librist.a)
add_dependencies(librist project_rist)

set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4frame.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/lz4hc.c PROPERTIES GENERATED TRUE)
set_source_files_properties( ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/lz4/xxhash.c PROPERTIES GENERATED TRUE)

add_library(ristsender STATIC
        ristsender.cpp
        oob_shared.c
        )
        target_link_libraries(ristsender PRIVATE Threads::Threads)
        target_link_libraries(ristsender PUBLIC librist)
IF (WIN32)
        target_compile_definitions(ristsender PRIVATE WIN32_LEAN_AND_MEAN __STDC_FORMAT_MACROS)
        target_link_libraries(ristsender PRIVATE iphlpapi wsock32 ws2_32)
        target_compile_definitions(ristsender PRIVATE _CRT_SECURE_NO_WARNINGS)

ENDIF()

target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/include/)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/include/librist)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include/librist)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/build)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/mbedtls/include)
target_include_directories(ristsender PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/src)
target_include_directories(ristsender PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(ristreceiver STATIC
        ristreceiver.cpp
        oob_shared.c
        )
target_link_libraries(ristreceiver PRIVATE librist Threads::Threads)
IF (WIN32)
        target_compile_definitions(ristreceiver PRIVATE WIN32_LEAN_AND_MEAN)
        target_link_libraries(ristreceiver PRIVATE iphlpapi wsock32 ws2_32)
        target_compile_definitions(ristreceiver PRIVATE _CRT_SECURE_NO_WARNINGS)
ENDIF()
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/include/)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/include/librist)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include/librist)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/build/include)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/build)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/contrib/mbedtls/include)
target_include_directories(ristreceiver PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/rist/src)
target_include_directories(ristreceiver PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_features(ristsender PRIVATE cxx_std_20)
target_compile_features(ristreceiver PRIVATE cxx_std_20)