project(scratchpad)
SET(PROJECT_NAME scratchpad)
cmake_minimum_required(VERSION 3.2.2)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/")
SET (PROJECT_STATIC_RUNTIME FALSE)

if(CMAKE_COMPILER_IS_GNUCXX OR APPLE)
  if(PROJECT_STATIC_RUNTIME)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17 -static")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")
  endif()
endif()

SET (CMAKE_CXX_FLAGS_DEBUG "-g -DCRO_DEBUG_")
SET (CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# We're using c++17
SET (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_STANDARD_REQUIRED ON)

SET (OpenGL_GL_PREFERENCE "GLVND")

find_package(CROGINE REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Bullet REQUIRED)

include_directories(
  ${CROGINE_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${BULLET_INCLUDE_DIRS}
  src)

SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/batcat/CMakeLists.txt)
include(${PROJECT_DIR}/bsp/CMakeLists.txt)
include(${PROJECT_DIR}/collision/CMakeLists.txt)
include(${PROJECT_DIR}/voxels/CMakeLists.txt)

add_executable(${PROJECT_NAME}
               ${PROJECT_SRC}
               ${BATCAT_SRC}
               ${BSP_SRC}
               ${COLLISION_SRC}
               ${VOXEL_SRC})

target_link_libraries(${PROJECT_NAME}
  ${CROGINE_LIBRARIES}
  ${SDL2_LIBRARY}
  ${BULLET_LIBRARIES}
  ${OPENGL_LIBRARIES})
