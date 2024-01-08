# This file downloaded from https://raw.githubusercontent.com/m-a-d-n-e-s-s/madness/master/cmake/modules/FindLibunwind.cmake
# - Try to find Libunwind
# Input variables:
#  LIBUNWIND_ROOT_DIR     - The libunwind install directory
#  LIBUNWIND_INCLUDE_DIR  - The libunwind include directory
#  LIBUNWIND_LIBRARY      - The libunwind library directory
# Output variables:
#  LIBUNWIND_FOUND        - System has libunwind
#  LIBUNWIND_INCLUDE_DIRS - The libunwind include directories
#  LIBUNWIND_LIBRARIES    - The libraries needed to use libunwind
#  LIBUNWIND_VERSION      - The version string for libunwind

include(FindPackageHandleStandardArgs)
  
if(NOT DEFINED LIBUNWIND_FOUND)

  # Set default sarch paths for libunwind
  if(LIBUNWIND_ROOT_DIR)
    set(LIBUNWIND_INCLUDE_DIR ${LIBUNWIND_ROOT_DIR}/include CACHE PATH "The include directory for libunwind")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
      set(LIBUNWIND_LIBRARY ${LIBUNWIND_ROOT_DIR}/lib64;${LIBUNWIND_ROOT_DIR}/lib CACHE PATH "The library directory for libunwind")
    else()
      set(LIBUNWIND_LIBRARY ${LIBUNWIND_ROOT_DIR}/lib CACHE PATH "The library directory for libunwind")
    endif()
  endif()
  
  find_path(LIBUNWIND_INCLUDE_DIRS NAMES libunwind.h
      HINTS ${LIBUNWIND_INCLUDE_DIR})
  
  find_library(LIBUNWIND_LIBRARIES unwind 
      HINTS ${LIBUNWIND_LIBRARY})
  
  # Get libunwind version
  if(EXISTS "${LIBUNWIND_INCLUDE_DIRS}/libunwind-common.h")
    file(READ "${LIBUNWIND_INCLUDE_DIRS}/libunwind-common.h" _libunwind_version_header)
    string(REGEX REPLACE ".*define[ \t]+UNW_VERSION_MAJOR[ \t]+([0-9]+).*" "\\1" 
        LIBUNWIND_MAJOR_VERSION "${_libunwind_version_header}")
    string(REGEX REPLACE ".*define[ \t]+UNW_VERSION_MINOR[ \t]+([0-9]+).*" "\\1"
        LIBUNWIND_MINOR_VERSION "${_libunwind_version_header}")
    string(REGEX REPLACE ".*define[ \t]+UNW_VERSION_EXTRA[ \t]+([0-9]*).*" "\\1"
        LIBUNWIND_MICRO_VERSION "${_libunwind_version_header}")
    if(LIBUNWIND_MICRO_VERSION)
      set(LIBUNWIND_VERSION "${LIBUNWIND_MAJOR_VERSION}.${LIBUNWIND_MINOR_VERSION}.${LIBUNWIND_MICRO_VERSION}")
    else()
      set(LIBUNWIND_VERSION "${LIBUNWIND_MAJOR_VERSION}.${LIBUNWIND_MINOR_VERSION}")
    endif()
    unset(_libunwind_version_header)
  endif()

  # handle the QUIETLY and REQUIRED arguments and set LIBUNWIND_FOUND to TRUE
  # if all listed variables are TRUE
  find_package_handle_standard_args(Libunwind
      FOUND_VAR LIBUNWIND_FOUND
      VERSION_VAR LIBUNWIND_VERSION 
      REQUIRED_VARS LIBUNWIND_LIBRARIES LIBUNWIND_INCLUDE_DIRS)

  mark_as_advanced(LIBUNWIND_INCLUDE_DIR LIBUNWIND_LIBRARY 
      LIBUNWIND_INCLUDE_DIRS LIBUNWIND_LIBRARIES)

endif()

if(LIBUNWIND_FOUND)
  if(NOT TARGET LIBUNWIND::LIBUNWIND)
    add_library(LIBUNWIND::LIBUNWIND UNKNOWN IMPORTED)
    set_target_properties(LIBUNWIND::LIBUNWIND PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LIBUNWIND_INCLUDE_DIRS}"
        IMPORTED_LOCATION "${LIBUNWIND_LIBRARIES}" )
  endif()
endif()
