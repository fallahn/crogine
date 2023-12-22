add_library(crogine)

option(USE_OPENAL  "Choose whether to use OpenAL for audio or SDL_Mixer." TRUE)
option(TARGET_ANDROID "Build the library for Android devices" FALSE)
option(USE_GL_41 "Use OpenGL 4.1 instead of 4.6 on desktop builds." FALSE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules/")

if(USE_OPENAL)
  target_compile_definitions(crogine PRIVATE AL_AUDIO)
else()
  target_compile_definitions(crogine PRIVATE SDL_AUDIO)
endif()

if(NOT TARGET_ANDROID)
  if(USE_GL_41)
  target_compile_definitions(crogine PRIVATE GL41)
  endif()
endif()

if (MSVC)
  target_compile_definitions(crogine PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if (APPLE)
  target_compile_definitions(crogine PRIVATE GL_SILENCE_DEPRECATION)
endif()

if (NOT BUILD_SHARED_LIBS)
  target_compile_definitions(crogine PRIVATE CRO_STATIC)
else()
  target_compile_definitions(crogine PRIVATE CRO_BUILD)
endif()

target_compile_definitions(crogine PRIVATE $<$<CONFIG:Debug>:CRO_DEBUG>)

set_target_properties(crogine PROPERTIES DEBUG_POSTFIX -d)

target_compile_features(crogine PRIVATE cxx_std_17)

set (OpenGL_GL_PREFERENCE "GLVND")

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)
find_package(OpenGL REQUIRED)

if(USE_OPENAL)
#use mojo AL on mac
  if(NOT APPLE)
    find_package(OpenAL REQUIRED)
  endif()
else()
  find_package(SDL2_mixer REQUIRED)
endif()

#used in dumping stack traces
if(MSVC)
  find_package(DbgHelp REQUIRED)
elseif(LINUX)
  find_package(Libunwind REQUIRED)
endif()

include_directories(
  ${SDL2_INCLUDE_DIR}  
  ${FREETYPE_INCLUDE_DIRS} 
  ${CMAKE_SOURCE_DIR}/crogine/include)
  
if(MSVC)
  include_directories(${CMAKE_SOURCE_DIR}/extlibs/openal/include ${DBGHELP_INCLUDE_DIR})
elseif(LINUX)
  include_directories(${LIBUNWIND_INCLUDE_DIRS})
endif()

SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)

if(USE_OPENAL)
  if(NOT APPLE)
    include_directories(${OPENAL_INCLUDE_DIR})
  endif()
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

# add the Resourcepath source, mojoal source and GL41 loader for apple, else load GL46
if (APPLE)
  set(PROJECT_SRC ${PROJECT_SRC} ${project_src_macos})
else()
  SET(PROJECT_SRC ${PROJECT_SRC} src/detail/glad.c)
endif()

target_link_libraries(crogine
  ${SDL2_LIBRARY} 
  ${FREETYPE_LIBRARIES}
  ${OPENGL_LIBRARIES})

if(USE_OPENAL)
  if(NOT APPLE)
    target_link_libraries(crogine ${OPENAL_LIBRARY})
  endif()
else()
  target_link_libraries(crogine ${SDL2_MIXER_LIBRARY})
endif()

if(MSVC)
  target_link_libraries(crogine winmm ws2_32 IPHLPAPI ${DBGHELP_LIBRARIES} shlwapi)
elseif(LINUX)
  target_link_libraries(crogine ${LIBUNWIND_LIBRARIES})
endif()

target_include_directories(crogine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp)
target_sources(crogine PUBLIC FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
  FILES ${HEADERS}
  PRIVATE ${PROJECT_SRC})


install(TARGETS crogine EXPORT crogine-targets FILE_SET HEADERS)
