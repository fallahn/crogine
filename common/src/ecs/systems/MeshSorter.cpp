/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include <crogine/ecs/systems/MeshSorter.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/graphics/Spatial.hpp>

#include <crogine/core/Clock.hpp>
#include <crogine/core/App.hpp>

#include "../../detail/GLCheck.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

using namespace cro;

MeshSorter::MeshSorter(MessageBus& mb, SceneRenderer& renderer)
    : System                (mb, typeid(MeshSorter)),
    m_renderer              (renderer)
{
    requireComponent<Transform>();
    requireComponent<Model>();
}

//public
void MeshSorter::process(cro::Time)
{
    auto& entities = getEntities();
    auto activeCamera = m_renderer.getActiveCamera();
    
    //build the frustum from the view-projection matrix
    auto viewProj = activeCamera.getComponent<Camera>().projection
        * glm::inverse(activeCamera.getComponent<Transform>().getWorldTransform());
    
    std::array<Plane, 6u> frustum = 
    {
        {Plane //left
        (
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        ),
        Plane //right
        (
            viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0],
            viewProj[3][3] - viewProj[3][0]
        ),
        Plane //bottom
        (
            viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1],
            viewProj[3][3] + viewProj[3][1]
        ),
        Plane //top
        (
            viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1],
            viewProj[3][3] - viewProj[3][1]
        ),
        Plane //near
        (
            viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2],
            viewProj[3][3] + viewProj[3][2]
        ),
        Plane //far
        (
            viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2],
            viewProj[3][3] - viewProj[3][2]
        )}
    };

    //normalise the planes
    for (auto& p : frustum)
    {
        const float factor = 1.f / std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        p.x *= factor;
        p.y *= factor;
        p.z *= factor;
        p.w *= factor;
    }
    //DPRINT("Near Plane", std::to_string(frustum[1].w));
    
    //cull entities by viewable into draw lists by pass
    m_visibleEntities.reserve(entities.size() * 2);
    for (auto& entity : entities)
    {
        auto model = entity.getComponent<Model>();
        auto sphere = model.m_meshData.boundingSphere;
        auto tx = entity.getComponent<Transform>();
        sphere.centre = glm::vec3(tx.getWorldTransform() * glm::vec4(sphere.centre.x, sphere.centre.y, sphere.centre.z, 1.f));
        auto scale = tx.getScale();
        sphere.radius *= (scale.x + scale.y + scale.z) / 3.f;

        //DPRINT("Found entity", std::to_string(entity.getIndex()));

        bool visible = true;
        std::size_t i = 0;
        while(visible && i < frustum.size())
        {
            visible = (Spatial::intersects(frustum[i++], sphere) != Planar::Back);
        }

        if (visible)
        {
            auto opaque = std::make_pair(entity, SortData());
            auto transparent = std::make_pair(entity, SortData());
            
            auto worldPos = tx.getWorldTransform() * glm::vec4(tx.getPosition(), 1.f);

            //foreach material
            //add ent/index pair to alpha or opaque list
            for (auto i = 0u; i < model.m_meshData.submeshCount; ++i)
            {
                if (model.m_materials[i].blendMode != Material::BlendMode::None)
                {
                    transparent.second.matIDs.push_back(i);
                    transparent.second.flags = static_cast<int64>(worldPos.z * 1000000.f); //suitably large number to shift decimal point
                    transparent.second.flags += 0x0FFF000000000000; //gaurentees embiggenment so that sorting places transparent last
                }
                else
                {
                    opaque.second.matIDs.push_back(i);
                    opaque.second.flags = static_cast<int64>(-worldPos.z * 1000000.f);
                }
            }

            //if (!opaque.second.matIDs.empty())
            {
                m_visibleEntities.push_back(opaque);
            }

            //if (!transparent.second.matIDs.empty())
            {
                m_visibleEntities.push_back(transparent);
            }
        }
    }
    //DPRINT("Visible ents", std::to_string(m_visibleEntities.size()));
    //DPRINT("Total ents", std::to_string(entities.size()));

    //sort lists by depth
    //sort opaque materials front to back
    std::sort(std::begin(m_visibleEntities), std::end(m_visibleEntities),
        [](MaterialPair& a, MaterialPair& b)
    {
        return a.second.flags < b.second.flags;
    });

    m_renderer.setDrawableList(m_visibleEntities);
}
