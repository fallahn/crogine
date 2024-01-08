/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "ChunkVisSystem.hpp"
#include "TerrainBuilder.hpp"

#include <crogine/graphics/Spatial.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/util/Matrix.hpp>

namespace
{
    constexpr float MaxDist = 280.f;
    constexpr float CullDist = MaxDist * MaxDist;
}

ChunkVisSystem::ChunkVisSystem(cro::MessageBus& mb, glm::vec2 MapSize, TerrainBuilder* tb)
    : cro::System(mb, typeid(ChunkVisSystem)),
    m_terrainBuilder(tb),
    m_chunkSize     (MapSize.x / ColCount, MapSize.y / RowCount),
    m_currentIndex  (0),
    m_indexList     (RowCount* ColCount)
{
    static constexpr float DefaultHeight = 60.f;
    static const float Width = MapSize.x / ColCount;
    static const float Depth = -MapSize.y / RowCount; //Y size is mapped to -z in world coords

    //init the bounding boxes. Point 0 is lower, 1 is upper
    for (auto y = 0; y < RowCount; ++y)
    {
        for (auto x = 0; x < ColCount; ++x)
        {
            auto idx = y * ColCount + x;
            m_boundingBoxes[idx][0] = { x * Width, 0.f, y * Depth };
            m_boundingBoxes[idx][1] = { (x + 1) * Width, DefaultHeight, (y + 1) * Depth };
        }
    }

    CRO_ASSERT(tb, "mustn't be nullptr!");
}

//public
void ChunkVisSystem::process(float)
{
    auto lastIndex = m_currentIndex;
    m_currentIndex = 0;
    m_indexList.clear();

    const auto& cam = getScene()->getActiveCamera().getComponent<cro::Camera>();
    const auto frustum = cam.getPass(cro::Camera::Pass::Final).getFrustum();

    static constexpr auto ChunkCount = RowCount * ColCount;

    const auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
    //const auto camForward = cro::Util::Matrix::getForwardVector(getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldTransform());

#ifdef CRO_DEBUG_
    narrowphaseCount = 0;
    m_narrowphaseTimer.begin();
#endif
    for (auto i = 0; i < ChunkCount; ++i)
    {
        auto dir = camPos - m_boundingBoxes[i].getCentre();

        //cull any chunks behind the camera
        //TODO hm... doesn't work :S
        /*if (glm::dot(dir, camForward) < 0)
        {
            continue;
        }*/

        //cull most distance chunks
        auto l2 = glm::length2(dir);
        if (l2 > CullDist)
        {
            continue;
        }

        //all planes face inwards
        bool intersects = true;
        auto j = 0;
        do
        {
            intersects = cro::Spatial::intersects(frustum[j++], m_boundingBoxes[i]) != cro::Planar::Back;
        } while (intersects && j < 6);

        if (intersects)
        {
            m_currentIndex |= (1 << i);
            m_indexList.push_back(i);
#ifdef CRO_DEBUG_
            narrowphaseCount++;
        }
    }
    m_narrowphaseTimer.end();
#else
        }
    }
#endif

    //check if index changed and send index list to TerrainBuilder
    if (m_currentIndex != lastIndex)
    {
        m_terrainBuilder->onChunkUpdate(m_indexList);
    }
}

void ChunkVisSystem::setWorldHeight(float h)
{
    for (auto& b : m_boundingBoxes)
    {
        b[1].y = h;
    }
}