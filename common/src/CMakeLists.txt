#source files used by crogine library
set(PROJECT_SRC
  ${PROJECT_DIR}/App.cpp
  ${PROJECT_DIR}/glad.c
  ${PROJECT_DIR}/SDLresource.cpp
  ${PROJECT_DIR}/State.cpp
  ${PROJECT_DIR}/StateStack.cpp
  ${PROJECT_DIR}/Window.cpp

  ${PROJECT_DIR}/ecs/Entity.cpp
  ${PROJECT_DIR}/ecs/EntityManager.cpp
  ${PROJECT_DIR}/ecs/Scene.cpp
  ${PROJECT_DIR}/ecs/System.cpp
  ${PROJECT_DIR}/ecs/SystemManager.cpp

  ${PROJECT_DIR}/system/Clock.cpp
  ${PROJECT_DIR}/system/Colour.cpp)
