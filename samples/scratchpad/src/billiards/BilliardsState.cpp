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

#include "BilliardsState.hpp"
#include "BilliardsSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    bool showDebug = false;
}

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_ballDef       (m_resources)
{
    addSystems();
    buildScene();
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_i:
            m_scene.getSystem<BilliardsSystem>()->applyImpulse();
            break;
        case SDLK_o:
            addBall();
            break;
        case SDLK_p:
            showDebug = !showDebug;
            m_debugDrawer.setDebugMode(showDebug ? /*std::numeric_limits<std::int32_t>::max()*/3 : 0);
            break;
        case SDLK_BACKSPACE:
            requestStackPop();
            requestStackPush(States::ScratchPad::MainMenu);
            break;
        case SDLK_KP_1:
            m_scene.setActiveCamera(m_cameras[0]);
            break;
        case SDLK_KP_2:
            m_scene.setActiveCamera(m_cameras[1]);
            break;
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool BilliardsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return false;
}

void BilliardsState::render()
{
    m_scene.render();

    auto viewProj = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    m_debugDrawer.render(viewProj);
}

//private
void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<BilliardsSystem>(mb, m_debugDrawer);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void BilliardsState::buildScene()
{
    cro::ModelDefinition md(m_resources);
    if (md.loadFromFile("assets/billiards/pool_collision.cmt"))
    {
        //table
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        m_scene.getSystem<BilliardsSystem>()->initTable(entity.getComponent<cro::Model>().getMeshData());
    }

    //ball
    if (m_ballDef.loadFromFile("assets/billiards/ball.cmt"))
    {
        addBall();
    }


    //camera / sunlight
    auto callback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 80.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto& camera = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camera.resizeCallback = callback;
    //camera.shadowMapBuffer.create(2048, 2048);
    callback(camera);

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 2.f, 0.f });
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::Z_AXIS, -90.f * cro::Util::Const::degToRad);
    m_cameras[0] = m_scene.getActiveCamera();

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -1.2f, 1.f, 1.2f });
    entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -45.f * cro::Util::Const::degToRad);
    entity.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    callback(entity.addComponent<cro::Camera>());
    entity.getComponent<cro::Camera>().resizeCallback = callback;
    m_cameras[1] = entity;

    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -25.f * cro::Util::Const::degToRad);
}

void BilliardsState::addBall()
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cro::Util::Random::value(-0.1f, 0.1f), 0.5f, cro::Util::Random::value(-0.1f, 0.1f) });
    m_ballDef.createModel(entity);
    entity.addComponent<BilliardBall>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (e.getComponent<cro::Transform>().getPosition().y < -1.f)
        {
            m_scene.destroyEntity(e);
        }
    };
}