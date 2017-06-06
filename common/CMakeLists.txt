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

SET (CMAKE_CXX_FLAGS_DEBUG "-g -D_DEBUG_ -DUSE_IMGUI")
SET (CMAKE_CXX_FLAGS_RELEASE "-O4 -DNDEBUG")
SET (CMAKE_DEBUG_POSTFIX -d)

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(
  ${SDL2_INCLUDE_DIR}
  ${SDL2_MIXER_INCLUDE_DIR}
  ${SDL2_TTF_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/common/include)


SET(PROJECT_DIR ${CMAKE_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)

if(PROJECT_STATIC_RUNTIME)
  add_library(${PROJECT_NAME} STATIC ${PROJECT_SRC})
else()
  add_library(${PROJECT_NAME} SHARED ${PROJECT_SRC})
endif()

target_link_libraries(${PROJECT_NAME}
  ${SDL2_LIBRARY}
  ${SDL2_MIXER_LIBRARY}
  ${SDL2_TTF_LIBRARIES}
  ${OPENGL_LIBRARIES})


target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/crogine DESTINATION include)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/glm DESTINATION include)
if(CROGINE_STATIC_LIB)
  install(TARGETS ${PROJECT_NAME} EXPORT crogine-targets DESTINATION lib)
else()
  install(TARGETS ${PROJECT_NAME} EXPORT crogine-targets 
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)
endif()
