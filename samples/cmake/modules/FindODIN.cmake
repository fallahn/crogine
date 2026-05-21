# - Try to find the ODIN library
#
# Once done, this will define:
#
#  ODIN_INCLUDE_DIR - the ODIN include directory
#  ODIN_LIBRARIES - The libraries needed to use ODIN

#SET(SEARCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../extlibs/odin-sdk/bin/linux-x86_64)

#include(CMakePrintHelpers)
#cmake_print_variables(SEARCH_DIR)

if(NOT ODIN_INCLUDE_DIR OR NOT ODIN_LIBRARIES)
    set(LIB_SEARCH_PATHS
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/lib
        /usr/lib64
        /usr/local/lib
        /usr/local/lib64
        ${CMAKE_CURRENT_SOURCE_DIR}/../../extlibs/odin-sdk/bin/linux-x86_64
        ${CMAKE_CURRENT_SOURCE_DIR}/../../extlibs/odin-sdk/bin/windows-x86_64
    )
    FIND_PATH(ODIN_INCLUDE_DIR odin.h
        /usr/include
        /usr/local/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../extlibs/odin-sdk/include/
        DOC "Include path for ODIN"
    )



    if(Windows)

        FIND_LIBRARY(ODIN_LIBRARY NAMES odin
            PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/../../extlibs/odin-sdk/bin/windows-x86_64
            DOC "ODIN library name"
        )

    else()
        FIND_LIBRARY(ODIN_LIBRARY NAMES libodin.so
            PATHS ${LIB_SEARCH_PATHS}
            DOC "ODIN library name"
        )
    endif()

    if(ODIN_LIBRARY)
        set(ODIN_LIBRARIES ${ODIN_LIBRARY})
    endif()

    MARK_AS_ADVANCED(ODIN_INCLUDE_DIR ODIN_LIBRARIES)
    
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ODIN DEFAULT_MSG ODIN_INCLUDE_DIR ODIN_LIBRARIES)
