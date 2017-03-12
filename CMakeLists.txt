#rename your project here
project(crogine)
SET(PROJECT_NAME crogine)
cmake_minimum_required(VERSION 2.8)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

if(CMAKE_COMPILER_IS_GNUCXX)
  if(PROJECT_STATIC_RUNTIME)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14 -static")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++14")
  endif()
endif()

SET (CMAKE_CXX_FLAGS_DEBUG "-g -D_DEBUG_")
SET (CMAKE_CXX_FLAGS_RELEASE "-O4 -DNDEBUG")

include_directories(
  ${CMAKE_SOURCE_DIR}/shared//include)


SET(PROJECT_DIR ${CMAKE_SOURCE_DIR}/shared/src)
include(${PROJECT_DIR}/CMakeLists.txt)

add_library(${PROJECT_NAME} ${PROJECT_SRC})

target_link_libraries(${PROJECT_NAME})


#install executable
install(TARGETS ${PROJECT_NAME}
  RUNTIME DESTINATION .)