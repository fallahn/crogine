add_executable(golf)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/")

option(USE_NEWSFEED "Set to true to attempt to parse RSS newsfeed from itchio. Requires libcurl" ON)
option(USE_GNS "Use Game Networking Sockets library" OFF)

if(USE_NEWSFEED)
  target_compile_definitions(golf PRIVATE USE_RSS)
endif()

find_package(Bullet 3.25)
find_package(SQLite3 REQUIRED)

if(USE_NEWSFEED)
  find_package(CURL REQUIRED)
endif()

if(NOT CROGINE_FOUND)
  #build it from source
  add_subdirectory(../../crogine)

  #SET (CROGINE_INCLUDE_DIR ../../crogine/include)
  #SET (CROGINE_LIBRARIES TODO set lib output dir)

endif()


if(USE_GNS)
  if(USE_NEWSFEED)
    message(FATAL_ERROR "Can't use newsfeed with GNS builds. Set USE_NEWSFEED to false")
  endif()

  target_compile_definitions(golf PRIVATE USE_GNS)
  SET(GNS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libgns)
  SET(SOCIAL_DIR ${GNS_DIR}/golf_ach/src)
  SET(SOCIAL_INCLUDE_DIR ${GNS_DIR}/golf_ach/include)
  SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${GNS_DIR}/cmake/modules/")
  find_package(STEAMWORKS REQUIRED)

else()
  set(SOCIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libsocial/src)
  set(SOCIAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libsocial/include)
endif()


target_include_directories(golf PRIVATE
  ${BULLET_INCLUDE_DIRS}
  ${SQLite3_INCLUDE_DIRS}
  ${SOCIAL_INCLUDE_DIR}
  src)

if(USE_NEWSFEED)
  target_include_directories(golf PRIVATE ${CURL_INCLUDE_DIR})
endif()

if(USE_GNS)
  target_include_directories(golf PRIVATE 
    ${STEAMWORKS_INCLUDE_DIR}
    ${GNS_DIR}/golf_workshop/include
    ${GNS_DIR}/libgns/include)
endif()

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/golf/CMakeLists.txt)
include(${PROJECT_DIR}/editor/CMakeLists.txt)
include(${PROJECT_DIR}/sqlite/CMakeLists.txt)
include(${SOCIAL_DIR}/CMakeLists.txt)

set(GOLF_SRC ${GOLF_SRC} ${SOCIAL_SRC} ${EDITOR_SRC} ${SQLITE_SRC})
if(USE_NEWSFEED)
  include(${PROJECT_DIR}/rss/CMakeLists.txt)
  set(GOLF_SRC ${GOLF_SRC} ${RSS_SRC})
endif()

if(USE_GNS)
  #include source of workshop, libgns
  include(${GNS_DIR}/golf_workshop/src/CMakeLists.txt)
  include(${GNS_DIR}/libgns/src/CMakeLists.txt)

  set(GOLF_SRC ${GOLF_SRC} ${GNS_SRC} ${WORKSHOP_SRC})
endif()

target_sources(golf PRIVATE
  ${PROJECT_SRC}
  ${GOLF_SRC})

target_link_libraries(golf PRIVATE
  crogine
  ${SDL2_LIBRARY}
  ${BULLET_LIBRARIES}
  ${SQLite3_LIBRARIES}  
  ${OPENGL_LIBRARIES})

if(USE_NEWSFEED)
  target_link_libraries(golf PRIVATE ${CURL_LIBRARIES})
endif()

if(USE_GNS)
  target_link_libraries(golf PRIVATE ${STEAMWORKS_LIBRARIES})
endif()

if (APPLE)
  # For each resource file we need to add it as a source file to the project
  file(GLOB_RECURSE RESOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/*)
  foreach(RESOURCE ${RESOURCES})
    target_sources(golf PRIVATE ${RESOURCE})
    # We also need to set the package location so it goes in the correct folder when bundled
    # and set HEADER_FILE_ONLY so the build system doesn't try to build them (some share extensions with cpp modules)
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/${RESOURCE} PROPERTIES MACOSX_PACKAGE_LOCATION ${RESOURCE} HEADER_FILE_ONLY ON)
  endforeach()

  #Then finally fix up the bundle first when installing
  install(CODE "include(BundleUtilities)\nfixup_bundle($<TARGET_BUNDLE_DIR:golf> $<TARGET_FILE:golf> $<TARGET_FILE_DIR:golf>)")
endif()

set_target_properties(golf PROPERTIES 
  MACOSX_BUNDLE ON
  XCODE_GENERATE_SCHEME ON 
  XCODE_SCHEME_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS golf BUNDLE DESTINATION . RESOURCE)
