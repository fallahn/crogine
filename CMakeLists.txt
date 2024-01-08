cmake_minimum_required(VERSION 3.16)
project(cro)

option(BUILD_SAMPLES "Build the crogine samples" OFF)

add_subdirectory(crogine)
#add_subdirectory(editor)

if(BUILD_SAMPLES)
  #add_subdirectory(samples/multiplayer_game)
  #add_subdirectory(samples/project_template)
  #add_subdirectory(samples/scratchpad)
  #add_subdirectory(samples/threat_level)
  add_subdirectory(samples/golf)
endif()