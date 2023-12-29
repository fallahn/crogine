cmake_minimum_required(VERSION 3.5.2)


#Xcode 10 will default to Mojave (must be done BEFORE project())
if (APPLE)
 SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
 SET(MACOS_BUNDLE False CACHE BOOL "")

  if(MACOS_BUNDLE)
    SET(BUNDLE_NAME "Super Video Golf")
  endif()

endif()


project(golf)
SET(PROJECT_NAME golf)


if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

if(APPLE)
SET(USE_NEWSFEED False CACHE STRING "Set to true to attempt to parse RSS newsfeed from itchio. Requires libcurl")
else()
SET(USE_NEWSFEED True CACHE STRING "Set to true to attempt to parse RSS newsfeed from itchio. Requires libcurl")
endif()

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/")
SET (PROJECT_STATIC_RUNTIME FALSE)
SET (USE_GNS FALSE CACHE STRING "Can't use newsfeed with this")

if(CMAKE_COMPILER_IS_GNUCXX OR APPLE)
  if(PROJECT_STATIC_RUNTIME)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-invalid-offsetof -std=c++17 -static")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-invalid-offsetof -std=c++17")
  endif()
endif()

SET(CMAKE_CXX_FLAGS_DEBUG "-g -DCRO_DEBUG_")
SET(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

if(USE_NEWSFEED)
  #add_definitions(-DUSE_RSS) #doesn't work?
  add_compile_definitions(USE_RSS)
  #add_compile_definitions(-DUSE_RSS) #can't find a definitive answer on this...
endif()



# We're using c++17
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(OpenGL_GL_PREFERENCE "GLVND")

find_package(CROGINE REQUIRED)
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
#TODO specify version 2.83
find_package(Bullet REQUIRED)
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

  add_compile_definitions(USE_GNS)


  SET(GNS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libgns)
  SET(SOCIAL_DIR ${GNS_DIR}/golf_ach/src)
  SET(SOCIAL_INCLUDE_DIR ${GNS_DIR}/golf_ach/include)
  SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${GNS_DIR}/cmake/modules/")
  find_package(STEAMWORKS REQUIRED)

else()
  SET(SOCIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libsocial/src)
  SET(SOCIAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libsocial/include)
endif()


include_directories(
  ${CROGINE_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${BULLET_INCLUDE_DIRS}
  ${SQLite3_INCLUDE_DIRS}
  ${SOCIAL_INCLUDE_DIR}
  src)

if(USE_NEWSFEED)
  include_directories(${CURL_INCLUDE_DIR})
endif()

if(USE_GNS)
  include_directories(${STEAMWORKS_INCLUDE_DIR})
  include_directories(${GNS_DIR}/golf_workshop/include)
  include_directories(${GNS_DIR}/libgns/include)
endif()

SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/golf/CMakeLists.txt)
include(${PROJECT_DIR}/editor/CMakeLists.txt)
include(${PROJECT_DIR}/sqlite/CMakeLists.txt)
include(${SOCIAL_DIR}/CMakeLists.txt)

SET(GOLF_SRC ${GOLF_SRC} ${SOCIAL_SRC} ${EDITOR_SRC} ${SQLITE_SRC})
#if(USE_NEWSFEED)
  include(${PROJECT_DIR}/rss/CMakeLists.txt)
  SET(GOLF_SRC ${GOLF_SRC} ${RSS_SRC})
#endif()

if(USE_GNS)
  #include source of workshop, libgns
  include(${GNS_DIR}/golf_workshop/src/CMakeLists.txt)
  include(${GNS_DIR}/libgns/src/CMakeLists.txt)

  SET(GOLF_SRC ${GOLF_SRC} ${GNS_SRC} ${WORKSHOP_SRC})
endif()


# If on apple, create a nice bundle
if(APPLE AND MACOS_BUNDLE)

  # use, i.e. don't skip the full RPATH for the build tree
  SET(CMAKE_SKIP_BUILD_RPATH FALSE)

  # when building, don't use the install RPATH already
  # (but later on when installing)
  SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

  SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

  # add the automatically determined parts of the RPATH
  # which point to directories outside the build tree to the install RPATH
  SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

  # the RPATH to be used when installing, but only if it's not a system directory
  LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
  IF("${isSystemDir}" STREQUAL "-1")
     SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  ENDIF("${isSystemDir}" STREQUAL "-1")

  set_source_files_properties( 
      ${CMAKE_SOURCE_DIR}/assets PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
  set_source_files_properties(
      ${CMAKE_SOURCE_DIR}/icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources )
  add_executable(${PROJECT_NAME} MACOSX_BUNDLE 
                 ${PROJECT_SRC} 
                 ${GOLF_SRC} 
                 ${CMAKE_SOURCE_DIR}/assets 
                 ${CMAKE_SOURCE_DIR}/macOS/icon.icns)


  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)



else()


  #regular make files
  add_executable(${PROJECT_NAME}
                 ${PROJECT_SRC}
                 ${GOLF_SRC})
endif()




target_link_libraries(${PROJECT_NAME}
  ${CROGINE_LIBRARIES}
  ${SDL2_LIBRARY}
  ${BULLET_LIBRARIES}
  ${SQLite3_LIBRARIES}  
  ${OPENGL_LIBRARIES})

if(USE_NEWSFEED)
  target_link_libraries(${PROJECT_NAME} ${CURL_LIBRARIES})
endif()

if(USE_GNS)
  target_link_libraries(${PROJECT_NAME} ${STEAMWORKS_LIBRARIES})
endif()

# install to the bundle
if(APPLE AND MACOS_BUNDLE)

  install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION .
    BUNDLE DESTINATION ${CMAKE_SOURCE_DIR}/bundle)


  install(CODE " include(BundleUtilities)
    fixup_bundle(${CMAKE_SOURCE_DIR}/bundle/${PROJECT_NAME}.app \"\" \"/Library/Frameworks\")
    verify_app(${CMAKE_SOURCE_DIR}/bundle/${PROJECT_NAME}.app)")
    SET(CPACK_GENERATOR "DragNDrop")

  include(CPack)

endif()