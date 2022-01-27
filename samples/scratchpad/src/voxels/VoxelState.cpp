/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#include "VoxelState.hpp"
#include "FpsCameraSystem.hpp"
#include "../StateIDs.hpp"

#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/GridMeshBuilder.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/gui/Gui.hpp>

namespace
{
    bool showDebug = false;
}

VoxelState::VoxelState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus()),
    m_showLayerWindow(false)
{
    std::fill(m_showLayer.begin(), m_showLayer.end(), true);
    loadSettings();    
    
    buildScene();

    registerWindow([&]()
        {
            drawMenuBar();
            drawLayerWindow();
        });
}

//public
bool VoxelState::handleEvent(const cro::Event& evt)
{
    m_scene.getSystem<VoxelFpsCameraSystem>()->handleEvent(evt);

    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_l:
        {
        }
        break;
        case SDLK_p:

            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            saveSettings();
            break;
        }
    }
    else if (evt.type == SDL_QUIT)
    {
        saveSettings();
    }

    m_scene.forwardEvent(evt);

    return false;
}

void VoxelState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool VoxelState::simulate(float dt)
{
    m_scene.simulate(dt);

    return false;
}

void VoxelState::render()
{
    m_scene.render();
}

//private
void VoxelState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CallbackSystem>(mb);
    //m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<VoxelFpsCameraSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);

    m_scene.setWaterLevel(Voxel::WateLevel);

    m_environmentMap.loadFromFile("assets/images/hills.hdr");
    //TODO set skybox if we move this project to golf


    //camera
    auto updateView = [](cro::Camera& cam)
    {
        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective(75.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, Voxel::MapSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_scene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ Voxel::MapSize.x / 2.f, 10.f, 10.f });
    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    //camEnt.getComponent<cro::Camera>().reflectionBuffer.create(1024, 1024);
    //camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<VoxelFpsCamera>().controllerIndex = 0;
    updateView(camEnt.getComponent<cro::Camera>());
    m_scene.setActiveCamera(camEnt);

    //sun direction
    auto sunEnt = m_scene.getSunlight();
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -65.f * cro::Util::Const::degToRad);
    sunEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -35.f * cro::Util::Const::degToRad);

    createLayers();
}

void VoxelState::createLayers()
{
    //water plane
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/voxels/models/ground_plane.cmt");

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    entity.getComponent<cro::Transform>().setPosition({ Voxel::MapSize.x / 2.f, Voxel::WateLevel, -Voxel::MapSize.y / 2.f });
    md.createModel(entity);
    entity.getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Water]);
    m_layers[Layer::Water] = entity;


    //terrain mesh

    //TODO create a double buffered texture we can draw to
    //or maybe manipulate an array of floats and upload those to the texture
    cro::Image img;
    img.create(16, 16, cro::Colour::Magenta);
    m_tempTexture.loadFromImage(img);

    m_shaderIDs[Shader::Terrain] = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseMap);
    m_materialIDs[Material::Terrain] = m_resources.materials.add(m_resources.shaders.get(m_shaderIDs[Shader::Terrain]));

    auto material = m_resources.materials.get(m_materialIDs[Material::Terrain]);
    material.setProperty("u_diffuseMap", m_tempTexture);

    //TODO replace this with dynamic mesh builder and create a proper grid
    cro::GridMeshBuilder gridBuilder(Voxel::MapSize, 4);
    auto meshID = m_resources.meshes.loadMesh(gridBuilder);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.getComponent<cro::Model>().setHidden(!m_showLayer[Layer::Terrain]);
    m_layers[Layer::Terrain] = entity;
}

void VoxelState::loadSettings()
{
    const auto cfgPath = cro::App::getPreferencePath() + "voxels.cfg";

    cro::ConfigFile cfg;
    if (cfg.loadFromFile(cfgPath))
    {
        const auto& props = cfg.getProperties();
        for (const auto& p : props)
        {
            const auto& name = p.getName();
            if (name == "show_water")
            {
                m_showLayer[Layer::Water] = p.getValue<bool>();
            }
            else if (name == "show_terrain")
            {
                m_showLayer[Layer::Terrain] = p.getValue<bool>();
            }
            else if (name == "show_voxel")
            {
                m_showLayer[Layer::Voxel] = p.getValue<bool>();
            }
        }
    }
}

void VoxelState::saveSettings()
{
    cro::ConfigFile cfg;
    cfg.addProperty("show_water").setValue(m_showLayer[Layer::Water]);
    cfg.addProperty("show_terrain").setValue(m_showLayer[Layer::Terrain]);
    cfg.addProperty("show_voxel").setValue(m_showLayer[Layer::Voxel]);

    const auto cfgPath = cro::App::getPreferencePath() + "voxels.cfg";
    cfg.save(cfgPath);
}