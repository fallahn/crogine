# - FindOpus.cmake
# Find the native opus includes and libraries
#
# OPUS_INCLUDE_DIRS - where to find opus/opus.h, etc.
# OPUS_LIBRARIES - List of libraries when using libopus(file).
# OPUS_FOUND - True if libopus found.

if(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)
    # Already in cache, be silent
    set(OPUS_FIND_QUIETLY TRUE)
endif(OPUS_INCLUDE_DIR AND OPUS_LIBRARY)


SET(SEARCH_PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw # Fink
    /opt/homebrew
    /opt/local # DarwinPorts
    /opt/csw # Blastwave
    /opt
    ../extlibs/opus
    ../extlibs/opus/include
    ../extlibs/opus/lib/x64
)


find_path(OPUS_INCLUDE_DIR
    NAMES opus.h
    PATH_SUFFIXES opus
    PATHS ${SEARCH_PATHS}
)

# MSVC built opus may be named opus_static
# The provided project files name the library with the lib prefix.
find_library(OPUS_LIBRARY
    NAMES opus opus_static libopus libopus_static
    PATHS ${SEARCH_PATHS}
)


# Handle the QUIETLY and REQUIRED arguments and set OPUS_FOUND
# to TRUE if all listed variables are TRUE.
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OPUS DEFAULT_MSG
    OPUS_LIBRARY OPUS_INCLUDE_DIR
)

if(OPUS_FOUND)
    set(OPUS_LIBRARIES ${OPUS_LIBRARY})
    set(OPUS_INCLUDE_DIRS ${OPUS_INCLUDE_DIR})
endif(OPUS_FOUND)