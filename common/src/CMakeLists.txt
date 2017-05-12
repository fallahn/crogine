#source files used by crogine library
set(PROJECT_SRC
  ${PROJECT_DIR}/core/App.cpp
  ${PROJECT_DIR}/core/Clock.cpp
  ${PROJECT_DIR}/core/DefaultLoadingScreen.cpp
  ${PROJECT_DIR}/core/MessageBus.cpp
  ${PROJECT_DIR}/core/State.cpp
  ${PROJECT_DIR}/core/StateStack.cpp
  ${PROJECT_DIR}/core/Wavetable.cpp
  ${PROJECT_DIR}/core/Window.cpp

  ${PROJECT_DIR}/detail/DistanceField.cpp
  ${PROJECT_DIR}/detail/glad.c
  ${PROJECT_DIR}/detail/SDLResource.cpp 

  ${PROJECT_DIR}/ecs/Component.cpp
  ${PROJECT_DIR}/ecs/Entity.cpp
  ${PROJECT_DIR}/ecs/EntityManager.cpp
  ${PROJECT_DIR}/ecs/Scene.cpp
  ${PROJECT_DIR}/ecs/System.cpp
  ${PROJECT_DIR}/ecs/SystemManager.cpp

  ${PROJECT_DIR}/ecs/components/Model.cpp
  ${PROJECT_DIR}/ecs/components/ParticleEmitter.cpp
  ${PROJECT_DIR}/ecs/components/Sprite.cpp
  ${PROJECT_DIR}/ecs/components/Text.cpp
  ${PROJECT_DIR}/ecs/components/Transform.cpp

  ${PROJECT_DIR}/ecs/systems/CommandSystem.cpp
  ${PROJECT_DIR}/ecs/systems/DebugInfo.cpp
  ${PROJECT_DIR}/ecs/systems/MeshSorter.cpp
  ${PROJECT_DIR}/ecs/systems/ParticleSystem.cpp
  ${PROJECT_DIR}/ecs/systems/SceneGraph.cpp
  ${PROJECT_DIR}/ecs/systems/SceneRenderer.cpp
  ${PROJECT_DIR}/ecs/systems/SpriteRenderer.cpp
  ${PROJECT_DIR}/ecs/systems/TextRenderer.cpp
  ${PROJECT_DIR}/ecs/systems/UISystem.cpp

  ${PROJECT_DIR}/graphics/Colour.cpp
  ${PROJECT_DIR}/graphics/Font.cpp
  ${PROJECT_DIR}/graphics/FontResource.cpp
  ${PROJECT_DIR}/graphics/Image.cpp
  ${PROJECT_DIR}/graphics/MaterialData.cpp
  ${PROJECT_DIR}/graphics/MaterialResource.cpp
  ${PROJECT_DIR}/graphics/MeshBuilder.cpp
  ${PROJECT_DIR}/graphics/MeshResource.cpp
  ${PROJECT_DIR}/graphics/PostProcess.cpp
  ${PROJECT_DIR}/graphics/PrimitiveBuilders.cpp
  ${PROJECT_DIR}/graphics/RenderTexture.cpp
  ${PROJECT_DIR}/graphics/Shader.cpp
  ${PROJECT_DIR}/graphics/ShaderResource.cpp
  ${PROJECT_DIR}/graphics/Spatial.cpp
  ${PROJECT_DIR}/graphics/StaticMeshBuilder.cpp
  ${PROJECT_DIR}/graphics/Texture.cpp
  ${PROJECT_DIR}/graphics/TextureResource.cpp

  ${PROJECT_DIR}/imgui/imgui.cpp
  ${PROJECT_DIR}/imgui/imgui_demo.cpp
  ${PROJECT_DIR}/imgui/imgui_draw.cpp
  ${PROJECT_DIR}/imgui/imgui_render.cpp

  ${PROJECT_DIR}/util/Random.cpp)
