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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/graphics/QuadBuilder.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

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
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }
    m_scene.getSystem<VoxelFpsCameraSystem>()->handleEvent(evt);

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
            break;
        }
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

    //m_environmentMap.loadFromFile("assets/images/hills.hdr");



    //camera
    auto updateView = [](cro::Camera& cam)
    {
        auto winSize = glm::vec2(cro::App::getWindow().getSize());
        cam.setPerspective(75.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, Voxel::MapSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_scene.createEntity();
    camEnt.addComponent<cro::Transform>().setPosition({ 0.f, 10.f, 10.f });
    camEnt.addComponent<cro::Camera>().resizeCallback = updateView;
    //camEnt.getComponent<cro::Camera>().reflectionBuffer.create(1024, 1024);
    //camEnt.addComponent<cro::AudioListener>();
    camEnt.addComponent<FpsCamera>();
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
    m_shaderIDs[Shader::Water] = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::DiffuseColour);
    m_materialIDs[Material::Water] = m_resources.materials.add(m_resources.shaders.get(m_shaderIDs[Shader::Water]));

    auto material = m_resources.materials.get(m_materialIDs[Material::Water]);
    material.setProperty("u_colour", cro::Colour::AliceBlue);
    material.doubleSided = true;

    auto meshID = m_resources.meshes.loadMesh(cro::QuadBuilder(Voxel::MapSize));

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 90.f * cro::Util::Const::degToRad);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
}