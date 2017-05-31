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

#include <crogine/ecs/systems/ProjectionMapSystem.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/ProjectionMap.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/graphics/Spatial.hpp>
#include <crogine/detail/GlobalConsts.hpp>

using namespace cro;

ProjectionMapSystem::ProjectionMapSystem(MessageBus& mb)
    : System(mb, typeid(ProjectionMapSystem))
{
    requireComponent<Transform>();
    requireComponent<ProjectionMap>();
}

//public
void ProjectionMapSystem::process(Time)
{
    Scene* scene = getScene();
    scene->m_projectionMapCount = 0;
    const auto& frustum = scene->getActiveCamera().getComponent<Camera>().getFrustum();
    
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        const auto& tx = entity.getComponent<Transform>();
        auto pos = tx.getWorldPosition();
        bool visible = true;
        for (const auto& p : frustum)
        {
            visible = (Spatial::distance(p, pos) > 0);
            if (!visible) break;
        }
      
        if (visible && scene->m_projectionMapCount < MAX_PROJECTION_MAPS)
        {
            const auto& projectionComponent = entity.getComponent<ProjectionMap>();
            scene->m_projectionMaps[scene->m_projectionMapCount++] = projectionComponent.projection * glm::inverse(tx.getWorldTransform());
        }

        if (scene->m_projectionMapCount == MAX_PROJECTION_MAPS)
        {
            break;
        }
    }
}