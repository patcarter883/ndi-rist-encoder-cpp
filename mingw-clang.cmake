# Sample toolchain file for building for Windows from an Ubuntu Linux system.
#
# Typical usage:
#    *) install cross compiler: `sudo apt-get install mingw-w64`
#    *) cd build
#    *) cmake -DCMAKE_TOOLCHAIN_FILE=~/mingw-w64-x86_64.cmake ..
# This is free and unencumbered software released into the public domain.

set(CMAKE_SYSTEM_NAME Windows)

# cross compilers to use for C, C++ and Fortran
set(CMAKE_C_COMPILER C:/msys64/clang64/bin/clang.exe)
set(CMAKE_CXX_COMPILER C:/msys64/clang64/bin/clang++.exe)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++ -Wl,-pdb=")

# target environment on the build host system
set(CMAKE_FIND_ROOT_PATH C:/msys64/clang64)

# modify default behavior of FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(PKG_CONFIG_EXECUTABLE x86_64-w64-mingw32-pkgconf.exe)
