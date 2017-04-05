#source files used by crogine library
set(PROJECT_SRC
  ${PROJECT_DIR}/App.cpp
  ${PROJECT_DIR}/glad.c
  ${PROJECT_DIR}/SDLResource.cpp
  ${PROJECT_DIR}/State.cpp
  ${PROJECT_DIR}/StateStack.cpp
  ${PROJECT_DIR}/Window.cpp

  ${PROJECT_DIR}/ecs/Component.cpp
  ${PROJECT_DIR}/ecs/Entity.cpp
  ${PROJECT_DIR}/ecs/EntityManager.cpp
  ${PROJECT_DIR}/ecs/Scene.cpp
  ${PROJECT_DIR}/ecs/System.cpp
  ${PROJECT_DIR}/ecs/SystemManager.cpp

  ${PROJECT_DIR}/ecs/components/Model.cpp
  ${PROJECT_DIR}/ecs/components/Transform.cpp

  ${PROJECT_DIR}/ecs/systems/MeshRenderer.cpp

  ${PROJECT_DIR}/graphics/Image.cpp
  ${PROJECT_DIR}/graphics/MaterialData.cpp
  ${PROJECT_DIR}/graphics/MaterialResource.cpp
  ${PROJECT_DIR}/graphics/MeshBuilder.cpp
  ${PROJECT_DIR}/graphics/MeshResource.cpp
  ${PROJECT_DIR}/graphics/PrimitiveBuilders.cpp
  ${PROJECT_DIR}/graphics/Shader.cpp
  ${PROJECT_DIR}/graphics/ShaderResource.cpp
  ${PROJECT_DIR}/graphics/Spatial.cpp
  ${PROJECT_DIR}/graphics/Texture.cpp
  ${PROJECT_DIR}/graphics/TextureResource.cpp

  ${PROJECT_DIR}/imgui/imgui.cpp
  ${PROJECT_DIR}/imgui/imgui_demo.cpp
  ${PROJECT_DIR}/imgui/imgui_draw.cpp
  ${PROJECT_DIR}/imgui/imgui_render.cpp

  ${PROJECT_DIR}/system/Clock.cpp
  ${PROJECT_DIR}/system/Colour.cpp)
