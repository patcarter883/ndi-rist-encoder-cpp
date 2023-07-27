# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/mnt/data/projects/recast/ndi-rist-encoder/ristwrap"
  "/mnt/data/projects/recast/ndi-rist-encoder/ristwrap"
  "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix"
  "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/tmp"
  "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp"
  "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src"
  "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp${cfgdir}") # cfgdir has leading slash
endif()
