
set(PROJECT_SRC
  ${PROJECT_DIR}/Chunk.cpp
  ${PROJECT_DIR}/ChunkManager.cpp
  ${PROJECT_DIR}/ChunkMeshBuilder.cpp
  ${PROJECT_DIR}/ChunkSystem.cpp
  ${PROJECT_DIR}/Coordinate.cpp
  ${PROJECT_DIR}/ErrorState.cpp
  ${PROJECT_DIR}/GameState.cpp
  ${PROJECT_DIR}/InputParser.cpp
  ${PROJECT_DIR}/InterpolationComponent.cpp
  ${PROJECT_DIR}/InterpolationSystem.cpp
  ${PROJECT_DIR}/LoadingScreen.cpp
  ${PROJECT_DIR}/main.cpp
  ${PROJECT_DIR}/MenuCreation.cpp
  ${PROJECT_DIR}/MenuState.cpp
  ${PROJECT_DIR}/MyApp.cpp
  ${PROJECT_DIR}/PauseState.cpp
  ${PROJECT_DIR}/PlayerSystem.cpp
  ${PROJECT_DIR}/Server.cpp
  ${PROJECT_DIR}/ServerGameState.cpp
  ${PROJECT_DIR}/ServerLobbyState.cpp
  ${PROJECT_DIR}/SliderSystem.cpp
  ${PROJECT_DIR}/TerrainGen.cpp
  ${PROJECT_DIR}/Voxel.cpp


  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD_avx2.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD_avx512.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD_internal.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD_neon.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD_sse2.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/fastnoise/FastNoiseSIMD_sse41.cpp)
