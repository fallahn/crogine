#rename your project here
project(crogine)
SET(PROJECT_NAME crogine)
cmake_minimum_required(VERSION 3.2.2)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

SET(USE_OPENAL TRUE CACHE BOOL "Choose whether to use OpenAL for audio or SDL_Mixer.")
SET(TARGET_ANDROID FALSE CACHE BOOL "Build the library for Android devices")

if(${TARGET_ANDROID})
  SET(${CMAKE_TOOLCHAIN_FILE} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchains/android-arm.cmake")
endif()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

if(CMAKE_COMPILER_IS_GNUCXX)
  if(PROJECT_STATIC_RUNTIME)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17 -static")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++17")
  endif()

  if(USE_OPENAL)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DAL_AUDIO")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSDL_AUDIO")
  endif()
endif()

SET (CMAKE_CXX_FLAGS_DEBUG "-g -DCRO_DEBUG_ -DHAS_SOCKLEN_T")
SET (CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -DHAS_SOCKLEN_T")
SET (CMAKE_DEBUG_POSTFIX -d)

# We're using c++17
SET (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_STANDARD_REQUIRED ON)

SET (OpenGL_GL_PREFERENCE "GLVND")

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)

if(USE_OPENAL)
find_package(OpenAL REQUIRED)
else()
find_package(SDL2_mixer REQUIRED)
endif()

include_directories(
  ${SDL2_INCLUDE_DIR}  
  ${FREETYPE_INCLUDE_DIRS} 
  ${CMAKE_SOURCE_DIR}/crogine/include)
  
if(MSVC)
  include_directories(${CMAKE_SOURCE_DIR}/extlibs/openal/include)
endif()

SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)

if(USE_OPENAL)
  include_directories(${OPENAL_INCLUDE_DIR})
  SET(PROJECT_SRC ${PROJECT_SRC} ${project_src_openal})
else()
  include_directories(${SDL2_MIXER_INCLUDE_DIR})
  SET(PROJECT_SRC ${PROJECT_SRC} ${project_src_sdlmixer})
endif()


#add platform specific source files
if(WIN32)
  SET(PROJECT_SRC ${PROJECT_SRC} ${project_src_win32})
else()
  SET(PROJECT_SRC ${PROJECT_SRC} ${project_src_nix})
endif()

# Add the Resourcepath source
if (APPLE)
  set(PROJECT_SRC ${PROJECT_SRC} src/detail/ResourcePath.mm)
endif()


if(PROJECT_STATIC_RUNTIME)
  add_library(${PROJECT_NAME} STATIC ${PROJECT_SRC})
else()
  add_library(${PROJECT_NAME} SHARED ${PROJECT_SRC})
endif()


if(USE_OPENAL)
target_link_libraries(${PROJECT_NAME}
  ${SDL2_LIBRARY} 
  ${FREETYPE_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${OPENAL_LIBRARY})
else()
target_link_libraries(${PROJECT_NAME}
  ${SDL2_LIBRARY} 
  ${FREETYPE_LIBRARIES}
  ${OPENGL_LIBRARIES}
  ${SDL2_MIXER_LIBRARY})
endif()


target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/crogine DESTINATION include)

if(CROGINE_STATIC_LIB)
  install(TARGETS ${PROJECT_NAME} EXPORT crogine-targets DESTINATION lib)
else()
  install(TARGETS ${PROJECT_NAME} EXPORT crogine-targets 
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)
endif()
