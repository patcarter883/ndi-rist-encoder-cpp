# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

if(EXISTS "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt" AND EXISTS "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitinfo.txt" AND
  "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt" IS_NEWER_THAN "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitinfo.txt")
  message(STATUS
    "Avoiding repeated git clone, stamp file is up to date: "
    "'/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt'"
  )
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/mnt/data/projects/recast/ndi-rist-encoder/ristwrap"
  RESULT_VARIABLE error_code
)
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/mnt/data/projects/recast/ndi-rist-encoder/ristwrap'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git" 
            clone --no-checkout --progress --config "advice.detachedHead=false" "https://code.videolan.org/rist/rist-cpp.git" "ristwrap"
    WORKING_DIRECTORY "/mnt/data/projects/recast/ndi-rist-encoder"
    RESULT_VARIABLE error_code
  )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once: ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://code.videolan.org/rist/rist-cpp.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git" 
          checkout "master" --
  WORKING_DIRECTORY "/mnt/data/projects/recast/ndi-rist-encoder/ristwrap"
  RESULT_VARIABLE error_code
)
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'master'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git" 
            submodule update --recursive --init 
    WORKING_DIRECTORY "/mnt/data/projects/recast/ndi-rist-encoder/ristwrap"
    RESULT_VARIABLE error_code
  )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/mnt/data/projects/recast/ndi-rist-encoder/ristwrap'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitinfo.txt" "/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
)
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/mnt/data/projects/recast/ndi-rist-encoder/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt'")
endif()
