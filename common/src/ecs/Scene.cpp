/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/Renderable.hpp>

#include <crogine/core/Clock.hpp>

using namespace cro;

Scene::Scene(MessageBus& mb)
    : m_messageBus  (mb),
    m_entityManager (mb),
    m_systemManager (*this)
{
    auto defaultCamera = createEntity();
    defaultCamera.addComponent<Transform>();
    defaultCamera.addComponent<Camera>();

    m_defaultCamera = defaultCamera.getIndex();

    currentRenderPath = [this]()
    {
        for (auto r : m_renderables) r->render();
    };
}

//public
void Scene::simulate(Time dt)
{
    for (const auto& entity : m_pendingEntities)
    {
        m_systemManager.addToSystems(entity);
    }
    m_pendingEntities.clear();

    for (const auto& entity : m_destroyedEntities)
    {
        m_systemManager.removeFromSystems(entity);
        m_entityManager.destroyEntity(entity);
    }
    m_destroyedEntities.clear();


    m_systemManager.process(dt);
    for (auto& p : m_postEffects) p->process(dt);
}

Entity Scene::createEntity()
{
    m_pendingEntities.push_back(m_entityManager.createEntity());
    return m_pendingEntities.back();
}

void Scene::destroyEntity(Entity entity)
{
    m_destroyedEntities.push_back(entity);
}

Entity Scene::getEntity(Entity::ID id) const
{
    return m_entityManager.getEntity(id);
}

Entity Scene::getDefaultCamera() const
{
    return m_entityManager.getEntity(m_defaultCamera);
}

void Scene::forwardMessage(const Message& msg)
{
    m_systemManager.forwardMessage(msg);

    if (msg.id == Message::WindowMessage)
    {
        const auto& data = msg.getData<Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            //resizes the post effect buffer if it is in use
            if (m_sceneBuffer.available())
            {
                m_sceneBuffer.create(data.data0, data.data1, true);
            }
        }
    }
}

void Scene::render()
{
    currentRenderPath();
}

//private
void Scene::postRenderPath()
{
    m_sceneBuffer.clear(Colour::Blue());
    for (auto r : m_renderables) r->render();
    m_sceneBuffer.display();

    m_postEffects[0]->apply(m_sceneBuffer);
}