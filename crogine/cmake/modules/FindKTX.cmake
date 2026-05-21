# - Try to find the KTX library
#
# Once done, this will define:
#
#  KTX_INCLUDE_DIR - the KTX include directory
#  KTX_LIBRARIES - The libraries needed to use KTX


if(NOT KTX_INCLUDE_DIR OR NOT KTX_LIBRARIES)
    set(LIB_SEARCH_PATHS
        #~/Library/Frameworks
        #/Library/Frameworks
        #/usr/lib
        #/usr/lib64
        #/usr/local/lib
        #/usr/local/lib64
        ${CMAKE_CURRENT_SOURCE_DIR}/../extlibs/ktx/linux-x86_64/lib
        ${CMAKE_CURRENT_SOURCE_DIR}/../extlibs/ktx/win64/lib
    )
    FIND_PATH(KTX_INCLUDE_DIR ktx.h
        /usr/include
        /usr/local/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../extlibs/ktx/include/
        DOC "Include path for KTX"
    )



    if(Windows)

        FIND_LIBRARY(KTX_LIBRARY NAMES ktx.lib
            PATHS
            ${CMAKE_CURRENT_SOURCE_DIR}/../extlibs/ktx/win64/lib
            DOC "KTX library name"
        )

    else()
        FIND_LIBRARY(KTX_LIBRARY NAMES libktx.so
            PATHS ${LIB_SEARCH_PATHS}
            DOC "KTX library name"
        )
    endif()

    if(KTX_LIBRARY)
        set(KTX_LIBRARIES ${KTX_LIBRARY})
    endif()

    MARK_AS_ADVANCED(KTX_INCLUDE_DIR KTX_LIBRARIES)
    
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(KTX DEFAULT_MSG KTX_INCLUDE_DIR KTX_LIBRARIES)
