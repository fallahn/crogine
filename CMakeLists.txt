cmake_minimum_required(VERSION 3.2.2)
project(cro)

set(BUILD_BATCAT false CACHE BOOL "Build the batcat sample application")
set(BUILD_TL false CACHE BOOL "Build the Threat Level sample application")

add_subdirectory(crogine)

if(BUILD_BATCAT)
add_subdirectory(samples/batcat)
endif()

if(BUILD_TL)
add_subdirectory(samples/threat_level)
endif()
