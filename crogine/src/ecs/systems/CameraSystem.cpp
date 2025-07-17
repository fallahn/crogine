/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2025
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

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/GBuffer.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/graphics/Spatial.hpp>

using namespace cro;

CameraSystem::CameraSystem(cro::MessageBus& mb)
    : System            (mb, typeid(CameraSystem)),
    m_nextDrawlistIndex (0)
{
    requireComponent<Camera>();
    requireComponent<Transform>();
}

//public
void CameraSystem::handleMessage(const Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            auto& entities = getEntities();

            for (auto entity : entities)
            {
                auto& camera = entity.getComponent<Camera>();
                if (camera.resizeCallback)
                {
                    camera.resizeCallback(camera);
                    camera.m_dirtyTx = true;
                    //camera.active = true; //don't do this, if there are a lot of cameras active at once it utterly kills perf
                }
                resizeGBuffer(entity);
            }
        }
    }
}

void CameraSystem::process(float)
{
    auto& entities = getEntities();

    for (auto entity : entities)
    {
        auto& camera = entity.getComponent<Camera>();

        if (camera.active)
        {
            const auto& tx = entity.getComponent<Transform>();
            camera.updateMatrices(tx, -getScene()->getWaterLevel() * 2.f);

            //don't clear these then systems updating them can just do swaps
            //finalPass.drawList.clear();
            //reflectionPass.drawList.clear();
            getScene()->updateDrawLists(entity);
        }

        camera.m_lastActive = camera.active;
        
        if (camera.isStatic)
        {
            camera.active = false;
        }
    }
}

const std::vector<Entity>& CameraSystem::getCameras() const
{
    return getEntities();
}

//private
void CameraSystem::resizeGBuffer(Entity entity)
{
    if (entity.hasComponent<GBuffer>())
    {
        glm::vec2 size(App::getWindow().getSize());
        const auto& cam = entity.getComponent<Camera>();
        size.x *= cam.viewport.width;
        size.y *= cam.viewport.height;

        entity.getComponent<GBuffer>().buffer.create(static_cast<std::uint32_t>(size.x), static_cast<std::uint32_t>(size.y), GBuffer::TargetCount);
    }
}

void CameraSystem::onEntityAdded(Entity entity)
{
    resizeGBuffer(entity);

    if (!m_freeDrawlistIndices.empty())
    {
        entity.getComponent<cro::Camera>().m_drawListIndex = m_freeDrawlistIndices.back();
        m_freeDrawlistIndices.pop_back();
    }
    else
    {
        entity.getComponent<cro::Camera>().m_drawListIndex = m_nextDrawlistIndex++;
    }

    entity.getComponent<cro::Transform>().addCallback(
        [entity]() mutable
        {
            entity.getComponent<cro::Camera>().m_dirtyTx = true;
        });
}

void CameraSystem::onEntityRemoved(Entity entity)
{
    m_freeDrawlistIndices.push_back(entity.getComponent<cro::Camera>().m_drawListIndex);
}