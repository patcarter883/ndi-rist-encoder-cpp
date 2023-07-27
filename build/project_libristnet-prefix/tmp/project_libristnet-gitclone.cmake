
if(NOT "/home/patcarter/code/ndi-rist-encoder-cpp/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitinfo.txt" IS_NEWER_THAN "/home/patcarter/code/ndi-rist-encoder-cpp/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt")
  message(STATUS "Avoiding repeated git clone, stamp file is up to date: '/home/patcarter/code/ndi-rist-encoder-cpp/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt'")
  return()
endif()

execute_process(
  COMMAND ${CMAKE_COMMAND} -E rm -rf "/home/patcarter/code/ndi-rist-encoder-cpp/ristwrap"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to remove directory: '/home/patcarter/code/ndi-rist-encoder-cpp/ristwrap'")
endif()

# try the clone 3 times in case there is an odd git clone issue
set(error_code 1)
set(number_of_tries 0)
while(error_code AND number_of_tries LESS 3)
  execute_process(
    COMMAND "/usr/bin/git"  clone --no-checkout --progress --config "advice.detachedHead=false" "https://code.videolan.org/rist/rist-cpp.git" "ristwrap"
    WORKING_DIRECTORY "/home/patcarter/code/ndi-rist-encoder-cpp"
    RESULT_VARIABLE error_code
    )
  math(EXPR number_of_tries "${number_of_tries} + 1")
endwhile()
if(number_of_tries GREATER 1)
  message(STATUS "Had to git clone more than once:
          ${number_of_tries} times.")
endif()
if(error_code)
  message(FATAL_ERROR "Failed to clone repository: 'https://code.videolan.org/rist/rist-cpp.git'")
endif()

execute_process(
  COMMAND "/usr/bin/git"  checkout master --
  WORKING_DIRECTORY "/home/patcarter/code/ndi-rist-encoder-cpp/ristwrap"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to checkout tag: 'master'")
endif()

set(init_submodules TRUE)
if(init_submodules)
  execute_process(
    COMMAND "/usr/bin/git"  submodule update --recursive --init 
    WORKING_DIRECTORY "/home/patcarter/code/ndi-rist-encoder-cpp/ristwrap"
    RESULT_VARIABLE error_code
    )
endif()
if(error_code)
  message(FATAL_ERROR "Failed to update submodules in: '/home/patcarter/code/ndi-rist-encoder-cpp/ristwrap'")
endif()

# Complete success, update the script-last-run stamp file:
#
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy
    "/home/patcarter/code/ndi-rist-encoder-cpp/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitinfo.txt"
    "/home/patcarter/code/ndi-rist-encoder-cpp/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt"
  RESULT_VARIABLE error_code
  )
if(error_code)
  message(FATAL_ERROR "Failed to copy script-last-run stamp file: '/home/patcarter/code/ndi-rist-encoder-cpp/build/project_libristnet-prefix/src/project_libristnet-stamp/project_libristnet-gitclone-lastrun.txt'")
endif()

