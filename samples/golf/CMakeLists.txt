cmake_minimum_required(VERSION 3.5.2)

#Xcode 10 will default to Mojave (must be done BEFORE project())
if (APPLE)
 SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")

 SET(BUNDLE_NAME "Super Video Golf")

endif()


project(golf)
SET(PROJECT_NAME golf)

if(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

#option(USE_NEWSFEED "Set to true to attempt to parse RSS newsfeed from itchio. Requires libcurl" ON)
option(USE_STEAM "Enable the steamworks SDK" OFF)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/")
SET(PROJECT_STATIC_RUNTIME FALSE)

SET(USE_PARALLEL_EXECUTION TRUE CACHE BOOL "Enable parallel execution, requires compiler support")

if(CMAKE_COMPILER_IS_GNUCXX OR APPLE)
  if(PROJECT_STATIC_RUNTIME)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-invalid-offsetof -Wno-comment -std=c++17 -static")
  else()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-invalid-offsetof -Wno-comment -std=c++17")
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-potentially-evaluated-expression")
endif()

if (NOT USE_PARALLEL_EXECUTION OR APPLE)
  add_definitions(-DPARALLEL_GLOBAL_DISABLE)
endif()

if(NOT GNS_TAG)
  SET(GNS_TAG "origin/master")
endif()

#TODO websockets uses this?
#if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
#  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
#endif()


# We're using c++17
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(OpenGL_GL_PREFERENCE "GLVND")

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
#TODO specify version 2.83
find_package(Bullet REQUIRED)
find_package(SQLite3 REQUIRED)

if(NOT USE_STEAM)
  find_package(CURL REQUIRED)
endif()

# If the crogine target exists then we're being built as part of the crogine project
# so can link to it directly. If not, we must find a pre-installed version
if(NOT TARGET crogine)
  # We are configuring golf standalone, so must find a crogine installation
  find_package(CROGINE REQUIRED)
endif()


if(USE_STEAM)

  add_compile_definitions(USE_GNS USE_WORKSHOP)

  include(FetchContent)
  FetchContent_Declare(libgns
    GIT_REPOSITORY https://github.com/fallahn/libgns
    GIT_PROGRESS true
    GIT_TAG ${GNS_TAG}) # Could specify a tag/commit hash here?
  FetchContent_MakeAvailable(libgns)

  SET(GNS_DIR ${libgns_SOURCE_DIR})
  SET(SOCIAL_DIR ${GNS_DIR}/golf_ach/src)
  SET(SOCIAL_INCLUDE_DIR ${GNS_DIR}/golf_ach/include)

if(APPLE)
#apple sucks
else()

  SET(DISCORD_SDK_ROOT "${GNS_DIR}/extlibs/discord")
  SET(DISCORD_SDK_LIB_DIR "${DISCORD_SDK_ROOT}/lib/release")
  SET(DISCORD_SDK_BIN_DIR "${DISCORD_SDK_ROOT}/bin/release")
  SET(DISCORD_SDK_INCLUDE_DIR "${DISCORD_SDK_ROOT}/include")

  add_compile_definitions(USE_DCD)
  if(WIN32)
      set(DISCORD_LIB_PATH "${DISCORD_SDK_LIB_DIR}/discord_partner_sdk.lib")
      set(DISCORD_SHARED_LIB "${DISCORD_SDK_BIN_DIR}/discord_partner_sdk.dll")
  else() # Linux
      set(DISCORD_LIB_PATH "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.so")
      set(DISCORD_SHARED_LIB "${DISCORD_SDK_LIB_DIR}/libdiscord_partner_sdk.so")
  endif()
endif()

  SET(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${GNS_DIR}/cmake/modules/")
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

if(USE_STEAM)
  include_directories(${STEAMWORKS_INCLUDE_DIR})
  include_directories(${GNS_DIR}/golf_workshop/include)
  include_directories(${GNS_DIR}/libgns/include)
  include_directories(${DISCORD_SDK_INCLUDE_DIR})
endif()

SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/golf/CMakeLists.txt)
include(${PROJECT_DIR}/editor/CMakeLists.txt)
include(${PROJECT_DIR}/runner/CMakeLists.txt)
include(${PROJECT_DIR}/scrub/CMakeLists.txt)
include(${PROJECT_DIR}/sockets/CMakeLists.txt)
include(${PROJECT_DIR}/sportsball/CMakeLists.txt)
include(${PROJECT_DIR}/sqlite/CMakeLists.txt)
include(${SOCIAL_DIR}/CMakeLists.txt)

SET(GOLF_SRC ${GOLF_SRC} ${SOCIAL_SRC} ${EDITOR_SRC} ${ENDLESS_SRC} ${SCRUB_SRC} ${SOCKETS_SRC} ${SBALL_SRC} ${SQLITE_SRC})
#Steam version actually uses this too now...
#if(USE_NEWSFEED)
  include(${PROJECT_DIR}/rss/CMakeLists.txt)
  SET(GOLF_SRC ${GOLF_SRC} ${RSS_SRC})
#endif()

if(USE_STEAM)
  #include source of workshop, libgns
  include(${GNS_DIR}/golf_workshop/src/CMakeLists.txt)
  include(${GNS_DIR}/libgns/src/CMakeLists.txt)

  SET(GOLF_SRC ${GOLF_SRC} ${GNS_SRC} ${WORKSHOP_SRC})
endif()

add_executable(${PROJECT_NAME}
                ${PROJECT_SRC}
                ${GOLF_SRC})

# If on apple, create a nice bundle
if(APPLE)

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

  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)

  # Add the assets if the folder exists
  set(ASSETS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets)
  if (EXISTS ${ASSETS_PATH})
    target_sources(${PROJECT_NAME} PRIVATE ${ASSETS_PATH})
    set_source_files_properties(${ASSETS_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  endif()

  # Add the icon
  set(ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/macOS/icon.icns)
  target_sources(${PROJECT_NAME} PRIVATE ${ICON_PATH})
  set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)

endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:CRO_DEBUG_>)

target_link_libraries(${PROJECT_NAME}
  ${CROGINE_LIBRARIES}
  ${SDL2_LIBRARY}
  ${BULLET_LIBRARIES}
  ${SQLite3_LIBRARIES}  
  ${OPENGL_LIBRARIES})

if(TARGET crogine)
 target_link_libraries(${PROJECT_NAME} crogine)
endif()



#websockets needs this
if (WIN32)
  target_link_libraries(${PROJECT_NAME} wsock32 ws2_32 shlwapi)
endif()

if (UNIX AND NOT APPLE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads)
  target_link_libraries(${PROJECT_NAME} Threads::Threads)
endif()
#end websockets




if(USE_STEAM)
  target_link_libraries(${PROJECT_NAME} ${STEAMWORKS_LIBRARIES})

  if (NOT APPLE)
    target_link_libraries(${PROJECT_NAME} ${DISCORD_LIB_PATH} ${CMAKE_DL_LIBS})
  endif()
else()
  target_link_libraries(${PROJECT_NAME} CURL::libcurl)
endif()




if(APPLE)
  # Install the app bundle to the root install folder
  install(TARGETS ${PROJECT_NAME} DESTINATION .)

  # Use fixup_bundle to ensure all libs are copied into the bundle
  # Then sign with ad-hoc signing
  install(CODE " include(BundleUtilities)
    fixup_bundle(${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app \"\" 
    \"/Library/Frameworks;$<TARGET_FILE_DIR:crogine>;${CMAKE_INSTALL_PREFIX}/lib;${GNS_DIR}/extlibs/steam/bin\")
    execute_process(COMMAND codesign -f -s \"-\" --deep ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app COMMAND_ECHO STDOUT)
    execute_process(COMMAND codesign -vvvv ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app COMMAND_ECHO STDOUT)")
  set(CPACK_GENERATOR "DragNDrop")

  include(CPack)

endif()
