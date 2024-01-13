cmake_minimum_required(VERSION 3.1)

# Rename this variable to change the project name
# see the last line of this file for info on copying assets
SET(PROJECT_NAME croplug)

# Set up the project
project(${PROJECT_NAME})

# Some default variables which the user may change
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/")
SET(CMAKE_BUILD_TYPE        Debug CACHE STRING  "Choose the type of build (Debug or Release)")

# We're using c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# enable some warnings in debug builds with gcc/clang
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wreorder")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wreorder -Wheader-guard")
endif()

SET (OpenGL_GL_PREFERENCE "GLVND")

# Find crogine
find_package(CROGINE REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# X11 is required on unices
if(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
endif()

if(X11_FOUND)
  include_directories(${X11_INCLUDE_DIRS})
endif()

# Project source files
include_directories(
  ${CROGINE_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  include)

add_subdirectory(include)
add_subdirectory(src)

# Add CRO_DEBUG on Debug builds
if (CMAKE_BUILD_TYPE MATCHES Debug) 
  add_definitions(-DCRO_DEBUG)
endif()

# Create the actual binary (PROJECT_SRC variable is set inside previous steps)
add_library(${PROJECT_NAME} SHARED ${PROJECT_SRC})

# Linker settings
target_link_libraries(${PROJECT_NAME} 
  ${CROGINE_LIBRARIES}
  ${SDL2_LIBRARY}
  ${OPENGL_LIBRARIES})

# Additional include directories
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)

if(X11_FOUND)
  target_link_libraries(${PROJECT_NAME}
    ${X11_LIBRARIES})
endif()

#set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "osgc")
#set_target_properties(${PROJECT_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/osgc/plugins/${PROJECT_NAME}")

#set(dst_path "${CMAKE_BINARY_DIR}/osgc/plugins/${PROJECT_NAME}/")
#FILE(COPY info.xgi DESTINATION ${dst_path} FILE_PERMISSIONS OWNER_READ OWNER_WRITE)
#uncomment this and rename 'assets' to whatever your resource directory is called to copy resources to the output directory
#FILE(COPY assets DESTINATION ${dst_path} FILE_PERMISSIONS OWNER_READ OWNER_WRITE)