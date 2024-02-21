
add_executable(golf WIN32 MACOSX_BUNDLE)

option(USE_NEWSFEED "Set to true to attempt to parse RSS newsfeed from itchio. Requires libcurl" TRUE)
option(USE_GNS "Use game networking services library. Cannot be used with newsfeed" FALSE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/modules/")

# Todo: Set this as a public property of crogine, or just use the standard NDEBUG
target_compile_definitions(golf PRIVATE $<$<CONFIG:Debug>:CRO_DEBUG_>)

# We're using c++17
target_compile_features(golf PRIVATE cxx_std_17)

set(OpenGL_GL_PREFERENCE "GLVND")

# Todo: Specify versions
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Bullet REQUIRED)
find_package(SQLite3 REQUIRED)

if(USE_GNS)
  if(USE_NEWSFEED)
    message(FATAL_ERROR "Can't use newsfeed with GNS builds. Set USE_NEWSFEED to false")
  endif()

  target_compile_definitions(golf PRIVATE USE_GNS)


  set(GNS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../libgns)
  set(SOCIAL_DIR ${GNS_DIR}/golf_ach/src)
  set(SOCIAL_INCLUDE_DIR ${GNS_DIR}/golf_ach/include)
  set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${GNS_DIR}/cmake/modules/")
  find_package(STEAMWORKS REQUIRED)

  target_include_directories(golf PRIVATE 
    ${STEAMWORKS_INCLUDE_DIR} 
    ${GNS_DIR}/golf_workshop/include
    ${GNS_DIR}/libgns/include)

else()
  set(SOCIAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libsocial/src)
  set(SOCIAL_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../libsocial/include)
endif()


target_include_directories(golf PRIVATE
  ${SDL2_INCLUDE_DIR}
  ${OPENGL_INCLUDE_DIR}
  ${BULLET_INCLUDE_DIRS}
  ${SQLite3_INCLUDE_DIRS}
  ${SOCIAL_INCLUDE_DIR}
  src)

if(USE_NEWSFEED)
  target_compile_definitions(golf PRIVATE USE_RSS)
  find_package(CURL REQUIRED)
  target_include_directories(golf PRIVATE ${CURL_INCLUDE_DIR})
endif()

set(PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
include(${PROJECT_DIR}/CMakeLists.txt)
include(${PROJECT_DIR}/golf/CMakeLists.txt)
include(${PROJECT_DIR}/editor/CMakeLists.txt)
include(${PROJECT_DIR}/runner/CMakeLists.txt)
include(${PROJECT_DIR}/sqlite/CMakeLists.txt)
include(${SOCIAL_DIR}/CMakeLists.txt)

set(GOLF_SRC ${GOLF_SRC} ${SOCIAL_SRC} ${EDITOR_SRC} ${ENDLESS_SRC} ${SQLITE_SRC})
include(${PROJECT_DIR}/rss/CMakeLists.txt)
set(GOLF_SRC ${GOLF_SRC} ${RSS_SRC})

if(USE_GNS)
  #include source of workshop, libgns
  include(${GNS_DIR}/golf_workshop/src/CMakeLists.txt)
  include(${GNS_DIR}/libgns/src/CMakeLists.txt)

  set(GOLF_SRC ${GOLF_SRC} ${GNS_SRC} ${WORKSHOP_SRC})
endif()

target_sources(golf PRIVATE ${PROJECT_SRC} ${GOLF_SRC})

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