cmake_minimum_required(VERSION 3.5.0)


if(APPLE)
  #required for std::filesystem
  SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
  SET(CMAKE_PREFIX_PATH 
	"/opt/homebrew"
	"/opt/homebrew/opt/openal-soft/include"
	"/opt/homebrew/opt/openal-soft/lib"
	"/usr/local"
	"/usr/local/opt/openal-soft/include"
	"/usr/local/opt/openal-soft/lib")

  SET(CMAKE_FIND_FRAMEWORK LAST)
endif()

project(crogine)
SET(PROJECT_NAME crogine)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()
option(BUILD_SHARED_LIBS "Whether to build shared libraries" ON)

SET(USE_OPENAL TRUE CACHE BOOL "Choose whether to use OpenAL for audio or SDL_Mixer.")
SET(TARGET_ANDROID FALSE CACHE BOOL "Build the library for Android devices")

SET(USE_GL_41 FALSE CACHE BOOL "Use OpenGL 4.1 instead of 4.6 on desktop builds.")
SET(USE_PARALLEL_EXECUTION TRUE CACHE BOOL "Enable parallel execution, requires compiler support")

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
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-potentially-evaluated-expression")
endif()

if(USE_OPENAL)
  add_definitions(-DAL_AUDIO)
else()
  add_defnitions(-DSDL_AUDIO)
endif()

if(NOT TARGET_ANDROID)
  if(USE_GL_41)
    add_definitions(-DGL41)
  endif()
endif()

if (MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  if (NOT USE_PARALLEL_EXECUTION)
    add_definitions(-DPARALLEL_GLOBAL_DISABLE)
  endif()
endif()

if (APPLE)
#silence opengl/openal deprecation warnings
#plus apple clang doesn't support parallel exection policies
  add_definitions(-DGL_SILENCE_DEPRECATION -DPARALLEL_GLOBAL_DISABLE)
endif()

#versions of gcc older than 9 don't support parallel execution policies
if(CMAKE_COMPILER_IS_GNUCC AND
  CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8 AND
  USE_PARALLEL_EXECUTION)
  SET(USE_TBB true)
else()
  add_definitions(-DPARALLEL_GLOBAL_DISABLE)
endif()



if (NOT BUILD_SHARED_LIBS)
  add_definitions(-DCRO_STATIC)
else()
  add_definitions(-DCRO_BUILD)
endif()

SET (CMAKE_DEBUG_POSTFIX -d)

# We're using c++17
SET (CMAKE_CXX_STANDARD 17)
SET (CMAKE_CXX_STANDARD_REQUIRED ON)

SET (OpenGL_GL_PREFERENCE "GLVND")

find_package(SDL2 MODULE REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OPUS REQUIRED)

if(USE_OPENAL)
  find_package(OpenAL REQUIRED)
else()
  find_package(SDL2_mixer REQUIRED)
endif()

#used in dumping stack traces
if(MSVC)
  find_package(DbgHelp REQUIRED)
elseif(LINUX)
  find_package(Libunwind REQUIRED)
  if(USE_TBB)
    find_package(TBB REQUIRED)
  endif()
endif()

include_directories(
  ${SDL2_INCLUDE_DIR}  
  ${FREETYPE_INCLUDE_DIRS} 
  ${OPUS_INCLUDE_DIRS}
  ${CMAKE_SOURCE_DIR}/crogine/include)
  
if(MSVC)
  include_directories(${CMAKE_SOURCE_DIR}/extlibs/openal/include ${DBGHELP_INCLUDE_DIR})
elseif(LINUX)
  include_directories(${LIBUNWIND_INCLUDE_DIRS})
elseif(APPLE)
  include_directories(/usr/local/opt/openal-soft/include)
  link_directories(/usr/local/opt/openal-soft/lib)
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


#add platform specific source files for enet
if(WIN32)
  SET(PROJECT_SRC ${PROJECT_SRC} ${project_src_win32})
else()
  SET(PROJECT_SRC ${PROJECT_SRC} ${project_src_nix})
endif()

# add the Resourcepath source, and GL41 loader for apple, else load GL46
if (APPLE)
  set(PROJECT_SRC ${PROJECT_SRC} ${project_src_macos})
else()
  SET(PROJECT_SRC ${PROJECT_SRC} src/detail/glad.c)
endif()

# this might be built by a project using crogine
# so skip this step in that case
if(NOT TARGET crogine)
  if(NOT BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} STATIC ${PROJECT_SRC})
  else()
    add_library(${PROJECT_NAME} SHARED ${PROJECT_SRC})
  endif()



  target_compile_definitions(${PROJECT_NAME} PUBLIC $<$<CONFIG:Debug>:CRO_DEBUG_ PARALLEL_GLOBAL_DISABLE>)

  target_link_libraries(${PROJECT_NAME}
    ${SDL2_LIBRARY} 
    ${FREETYPE_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${OPENGL_LIBRARIES})

  if(USE_OPENAL)
    target_link_libraries(${PROJECT_NAME} ${OPENAL_LIBRARY})
  else()
    target_link_libraries(${PROJECT_NAME} ${SDL2_MIXER_LIBRARY})
  endif()

  if(MSVC)
    target_link_libraries(${PROJECT_NAME} winmm ws2_32 IPHLPAPI ${DBGHELP_LIBRARIES} shlwapi)
  elseif(LINUX)
    target_link_libraries(${PROJECT_NAME} ${LIBUNWIND_LIBRARIES})

    if(USE_TBB)
      target_link_libraries(${PROJECT_NAME} TBB::tbb)
    endif()
  endif()

  target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()


install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/crogine DESTINATION include)

if(NOT BUILD_SHARED_LIBS)
  install(TARGETS ${PROJECT_NAME} EXPORT crogine-targets DESTINATION lib)
else()
  install(TARGETS ${PROJECT_NAME} EXPORT crogine-targets 
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin)
endif()
