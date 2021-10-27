/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "CollisionState.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

CollisionState::CollisionState(cro::StateStack& ss, cro::State::Context ctx)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus()),
    m_shape(0.5f, 1.5f)
{
    buildScene();
}

//public
bool CollisionState::handleEvent(const cro::Event& evt)
{
    m_scene.forwardEvent(evt);

    return false;
}

void CollisionState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool CollisionState::simulate(float dt)
{
    m_scene.simulate(dt);


    //TODO we'd update the collision world with out ent positions here
    m_collisionWorld->performDiscreteCollisionDetection();
    m_collisionWorld->debugDrawWorld();

    return false;
}

void CollisionState::render()
{
    auto& rw = cro::App::getWindow();
    m_scene.render(rw);

    //render the debug stuff
    auto viewProj = m_scene.getActiveCamera().getComponent<cro::Camera>().getActivePass().viewProjectionMatrix;
    m_debugDrawer.render(viewProj);
}

//private
void CollisionState::buildScene()
{
    setupCollisionWorld();

    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);


    cro::ModelDefinition md(m_resources);
    md.loadFromFile("assets/models/sphere.cmt");

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);


    auto callback = [](cro::Camera& cam)
    {
        glm::vec2 winSize(cro::App::getWindow().getSize());
        cam.setPerspective(60.f * cro::Util::Const::degToRad, winSize.x / winSize.y, 0.1f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    auto& camera = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camera.resizeCallback = callback;
    callback(camera);

    m_scene.getActiveCamera().getComponent<cro::Transform>().move({ 0.f, 0.f, 5.f });
}

void CollisionState::setupCollisionWorld()
{
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_collisionDispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());

    m_broadphaseInterface = std::make_unique<btDbvtBroadphase>();
    m_collisionWorld = std::make_unique<btCollisionWorld>(m_collisionDispatcher.get(), m_broadphaseInterface.get(), m_collisionConfiguration.get());

    m_collisionWorld->setDebugDrawer(&m_debugDrawer);

    m_ghostObject.setCollisionShape(&m_shape);
    m_collisionWorld->addCollisionObject(&m_ghostObject);
}