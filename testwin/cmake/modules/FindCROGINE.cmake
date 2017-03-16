include(FindPackageHandleStandardArgs)

# Search for the header file
find_path(CROGINE_INCLUDE_DIR NAMES crogine/Config.hpp PATH_SUFFIXES include)

# Search for the library
find_library(CROGINE_LIBRARIES NAMES crogine PATH_SUFFIXES lib)

# Did we find everything we need?
FIND_PACKAGE_HANDLE_STANDARD_ARGS(crogine DEFAULT_MSG CROGINE_LIBRARIES CROGINE_INCLUDE_DIR) 