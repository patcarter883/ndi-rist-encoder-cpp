# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(ndi-ristTests LANGUAGES CXX)

# ---- Dependencies ----

find_package(Catch2 REQUIRED)
include(Catch)

# ---- Tests ----

add_executable(ndi-rist_test source/ndi-rist_test.cpp)
target_link_libraries(
    ndi-rist_test PRIVATE
    ndi-rist_lib
    Catch2::Catch2WithMain
)
target_compile_features(ndi-rist_test PRIVATE cxx_std_20)

catch_discover_tests(ndi-rist_test)

# ---- End-of-file commands ----

add_folders(Test)
