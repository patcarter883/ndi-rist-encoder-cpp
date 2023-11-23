cmake_minimum_required(VERSION 3.14)

include(cmake/prelude.cmake)

project(
    ndi-rist
    VERSION 0.1.0
    DESCRIPTION "Short description"
    HOMEPAGE_URL "https://example.com/"
    LANGUAGES CXX
)

include(cmake/project-is-top-level.cmake)
include(cmake/variables.cmake)

# ---- Declare library ----

add_library(
    ndi-rist_lib OBJECT
    source/lib.cpp
)

target_include_directories(
    ndi-rist_lib ${warning_guard}
    PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/source>"
)

target_compile_features(ndi-rist_lib PUBLIC cxx_std_20)

find_package(fmt REQUIRED)
target_link_libraries(ndi-rist_lib PRIVATE fmt::fmt)

# ---- Declare executable ----

add_executable(ndi-rist_exe source/main.cpp)
add_executable(ndi-rist::exe ALIAS ndi-rist_exe)

set_property(TARGET ndi-rist_exe PROPERTY OUTPUT_NAME ndi-rist)

target_compile_features(ndi-rist_exe PRIVATE cxx_std_20)

target_link_libraries(ndi-rist_exe PRIVATE ndi-rist_lib)

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
