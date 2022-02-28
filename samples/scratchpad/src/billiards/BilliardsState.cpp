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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/util/Constants.hpp>

BilliardsState::BilliardsState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus())
{
    addSystems();
    buildScene();
}

//public
bool BilliardsState::handleEvent(const cro::Event& evt)
{
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
}

//private
void BilliardsState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void BilliardsState::buildScene()
{
    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/billiards/pool_collision.cmt");

    //table
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);


    //ball
    md.loadFromFile("assets/billiards/ball.cmt");
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 0.f });
    md.createModel(entity);



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

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 1.f, 2.f });
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -28.f * cro::Util::Const::degToRad);

    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -25.f * cro::Util::Const::degToRad);
    m_scene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -25.f * cro::Util::Const::degToRad);
}