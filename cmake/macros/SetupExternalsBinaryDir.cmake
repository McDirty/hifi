# 
#  SetupExternalsBinaryDir.cmake
#  cmake/macros
# 
#  Copyright 2015 High Fidelity, Inc.
#  Created by Stephen Birarda on February 19, 2014
#
#  Distributed under the Apache License, Version 2.0.
#  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
# 

macro(SETUP_EXTERNALS_BINARY_DIR)

  # get a short name for the generator to use in the path
  STRING(REGEX REPLACE " " "-" CMAKE_GENERATOR_FOLDER_NAME ${CMAKE_GENERATOR})

  if (CMAKE_GENERATOR_FOLDER_NAME STREQUAL "Unix-Makefiles")
    set(CMAKE_GENERATOR_FOLDER_NAME "makefiles")
  elseif (CMAKE_GENERATOR_FOLDER_NAME STREQUAL "Visual-Studio-12")
    set(CMAKE_GENERATOR_FOLDER_NAME "vs12")
  endif ()

  set(EXTERNALS_BINARY_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build-externals")
  if (ANDROID)
    set(EXTERNALS_BINARY_DIR "${EXTERNALS_BINARY_ROOT_DIR}/android/${CMAKE_GENERATOR_FOLDER_NAME}")
  else ()
    set(EXTERNALS_BINARY_DIR "${EXTERNALS_BINARY_ROOT_DIR}/${CMAKE_GENERATOR_FOLDER_NAME}")
  endif ()

endmacro()