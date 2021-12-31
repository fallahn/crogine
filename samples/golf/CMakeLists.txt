cmake_minimum_required(VERSION 3.2.2)


#Xcode 10 will default to Mojave (must be done BEFORE project())
if (APPLE)
 SET(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version")
 SET(MACOS_BUNDLE False CACHE BOOL "")

  if(MACOS_BUNDLE)
    SET(BUNDLE_NAME "VGA Golf")
  endif()

endif()


project(golf)
SET(PROJECT_NAME vga_golf)


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
#TODO specify version 2.83
find_package(Bullet REQUIRED)

include_directories(
  ${CROGINE_INCLUDE_DIR}
  ${SDL2_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${BULLET_INCLUDE_DIRS}
  src)

SET(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/golf/CMakeLists.txt)





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
  add_executable(${PROJECT_NAME} MACOSX_BUNDLE ${PROJECT_SRC} ${GOLF_SRC} ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/icon.icns)
  set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE_ICON_FILE icon.icns)



else()


  # regular make files
  add_executable(${PROJECT_NAME}
                 ${PROJECT_SRC}
                 ${GOLF_SRC})

endif()




target_link_libraries(${PROJECT_NAME}
  ${CROGINE_LIBRARIES}
  ${SDL2_LIBRARY}
  ${BULLET_LIBRARIES}  
  ${OPENGL_LIBRARIES})


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
