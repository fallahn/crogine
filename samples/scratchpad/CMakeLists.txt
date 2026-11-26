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
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++20 -static")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++20")
  endif()
endif()

SET (CMAKE_CXX_FLAGS_DEBUG "-g -DCRO_DEBUG_")
SET (CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# We're using c++20
SET (CMAKE_CXX_STANDARD 20)
SET (CMAKE_CXX_STANDARD_REQUIRED ON)

SET (OpenGL_GL_PREFERENCE "GLVND")

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
find_package(CROGINE REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Bullet REQUIRED)
find_package(ReactPhysics3D REQUIRED)

include_directories(
  SDL3::Headers
  ${CROGINE_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${BULLET_INCLUDE_DIRS}
  ../../extlibs/polyvox
  src)

# TODO this needs to be kept up to date.
SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/animblend/CMakeLists.txt)
include(${PROJECT_DIR}/batcat/CMakeLists.txt)
include(${PROJECT_DIR}/billiards/CMakeLists.txt)
include(${PROJECT_DIR}/bsp/CMakeLists.txt)
include(${PROJECT_DIR}/collision/CMakeLists.txt)
include(${PROJECT_DIR}/voxels/CMakeLists.txt)
include(${PROJECT_DIR}/vats/CMakeLists.txt)
include(${PROJECT_DIR}/retro/CMakeLists.txt)
include(${PROJECT_DIR}/bush/CMakeLists.txt)
include(${PROJECT_DIR}/frustum/CMakeLists.txt)
include(${PROJECT_DIR}/rolling/CMakeLists.txt)
include(${PROJECT_DIR}/ssao/CMakeLists.txt)
include(${PROJECT_DIR}/swingput/CMakeLists.txt)

add_executable(${PROJECT_NAME}
               ${PROJECT_SRC}
               ${BLEND_SRC}
               ${BATCAT_SRC}
               ${BILLIARDS_SRC}
               ${BSP_SRC}
               ${COLLISION_SRC}
               ${VOXEL_SRC}
	             ${RETRO_SRC}
	             ${BUSH_SRC}
               ${ROLLING_SRC}
               ${SSAO_SRC}
               ${SWING_SRC}
               ${FRUSTUM_SRC}
               ${VATS_SRC})

target_link_libraries(${PROJECT_NAME}
  SDL3::SDL3
  ${CROGINE_LIBRARIES}
  ${BULLET_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ReactPhysics3D::ReactPhysics3D)
