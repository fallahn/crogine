/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2025
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
#include "GameConsts.hpp"

#include <crogine/graphics/Spatial.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/util/Matrix.hpp>

#include <crogine/gui/Gui.hpp>

//#define DRAW_DEBUG

namespace
{
#ifdef DRAW_DEBUG
    float debugDot = 0.f;
    std::int32_t queryIndex = 14;
#endif

    constexpr float MaxDist = CameraFarPlane * 0.875f;// 280.f;
    constexpr float CullDist = MaxDist * MaxDist;
}

ChunkVisSystem::ChunkVisSystem(cro::MessageBus& mb, glm::vec2 mapSize, TerrainBuilder* tb)
    : cro::System(mb, typeid(ChunkVisSystem)),
    m_terrainBuilder    (tb),
    m_chunkSize         (mapSize.x / ColCount, mapSize.y / RowCount),
    m_currentIndex      (0),
    m_indexList         (RowCount * ColCount),
    m_chunkCullRadius   (0.f)
{
    static constexpr float DefaultHeight = 60.f;
    static const float Width = mapSize.x / ColCount;
    static const float Depth = -mapSize.y / RowCount; //Y size is mapped to -z in world coords

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

    //used to early-cull chunks by detecting if they overlap
    //the camera's world position
    const auto radVec = m_boundingBoxes[0].getSize() / 2.f;
    m_chunkCullRadius = glm::length2(glm::vec2(radVec.x, radVec.z));

    CRO_ASSERT(tb, "mustn't be nullptr!");

#ifdef DRAW_DEBUG
    m_debugTexture.create(MapSize.x/2, MapSize.y/2, false);
    m_debugVerts.setPrimitiveType(GL_TRIANGLES);
    m_debugVerts.setScale(glm::vec2(0.5f));
    m_debugFrustum.setPrimitiveType(GL_LINE_STRIP);
    m_debugFrustum.setScale(glm::vec2(0.5f));
    registerWindow([&]() 
        {
            ImGui::Begin("Chunks");
            ImGui::Image(m_debugTexture.getTexture(), {MapSize.x/2, MapSize.y/2}, {0.f, 1.f}, {1.f, 0.f});
            
            const auto camForward = getScene()->getActiveCamera().getComponent<cro::Transform>().getForwardVector();
            const auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
            const auto dir = (m_boundingBoxes[queryIndex].getCentre()/* + m_boundingBoxes[queryIndex][0]*/) - camPos;
            ImGui::Text("Dot: %3.2f", glm::dot(glm::vec2(camForward.x, camForward.z), glm::vec2(dir.x, dir.z)));
            ImGui::Text("Debug Dot: %3.2f", debugDot);
            if (ImGui::InputInt("Index", &queryIndex))
            {
                queryIndex %= (ColCount * RowCount);
            }
            ImGui::Text("Box Radius: %3.2f", m_chunkCullRadius);
            ImGui::End();
        });
#endif
}

//public
void ChunkVisSystem::process(float)
{
    auto lastIndex = m_currentIndex;
    m_currentIndex = 0;
    m_indexList.clear();

    auto camEnt = getScene()->getActiveCamera();
    const auto& cam = camEnt.getComponent<cro::Camera>();
    const auto frustum = cam.getPass(cro::Camera::Pass::Final).getFrustum();

    static constexpr auto ChunkCount = RowCount * ColCount;

    const auto camPos = camEnt.getComponent<cro::Transform>().getWorldPosition();
    const auto camForward = camEnt.getComponent<cro::Transform>().getForwardVector();
    const glm::vec2 fwd2d = { camForward.x, camForward.z };

//#ifdef CRO_DEBUG_
//    narrowphaseCount = 0;
//    m_narrowphaseTimer.begin();
//#endif
    for (auto i = 0; i < ChunkCount; ++i)
    {
        //const auto dir = m_boundingBoxes[i].getCentre() - camPos;
        const auto centre = (m_boundingBoxes[i].getCentre());
        const glm::vec2 worldCentre = glm::vec2(centre.x, centre.z);
        const glm::vec2 dir = worldCentre - glm::vec2(camPos.x, camPos.z);

        //cull any chunks behind the camera
        //unless they overlap the camera position
        const auto l2 = glm::length2(dir);
        if (l2 < m_chunkCullRadius
            || glm::dot(dir, fwd2d) > 0)
        {
            //cull most distance chunks
            if (l2 < CullDist)
            {
                //TODO we could early-cull against map
                // AABB as there'll be no trees in the water (hopefully)
                //all planes face inwards
                bool intersects = true;
                auto j = 0;
                do
                {
                    intersects = cro::Spatial::intersects(frustum[j++], m_boundingBoxes[i]) != cro::Planar::Back;
                } while (intersects && j < 6);

                if (intersects)
                {
                    m_currentIndex |= (std::size_t(1) << i);
                    m_indexList.push_back(i);
//#ifdef CRO_DEBUG_
//                    narrowphaseCount++;
//                }
//            }
//            m_narrowphaseTimer.end();
//#else
                }
            }
        }
    }
//#endif

    //check if index changed and send index list to TerrainBuilder
    if (m_currentIndex != lastIndex)
    {
        m_terrainBuilder->onChunkUpdate(m_indexList);
    }

#ifdef DRAW_DEBUG
    updateDebug(getScene()->getActiveCamera());
#endif
}

void ChunkVisSystem::setWorldHeight(float h)
{
    for (auto& b : m_boundingBoxes)
    {
        b[1].y = h;
    }
}

//private
void ChunkVisSystem::updateDebug(cro::Entity cam)
{
#ifdef DRAW_DEBUG
    std::vector<cro::Vertex2D> verts;
    const auto addQuad =
        [&](glm::vec2 pos, glm::vec2 size, cro::Colour c)
        {
            verts.emplace_back(glm::vec2(pos.x, pos.y + size.y), c);
            verts.emplace_back(glm::vec2(pos.x, pos.y), c);
            verts.emplace_back(glm::vec2(pos+size), c);

            verts.emplace_back(glm::vec2(pos+size), c);
            verts.emplace_back(glm::vec2(pos.x, pos.y), c);
            verts.emplace_back(glm::vec2(pos.x + size.x, pos.y), c);
        };


    for (auto i = 0u; i < m_boundingBoxes.size(); ++i)
    {
        const auto& box = m_boundingBoxes[i];
        const glm::vec2 pos = { box[0].x, -box[0].z };
        const glm::vec2 size = { box.getSize().x, -box.getSize().z };

        if (m_currentIndex & (std::size_t(1) << i))
        {
            addQuad(pos, size, cro::Colour::Green);
        }
        else
        {
            addQuad(pos, size, cro::Colour::Red);
        }
    }

    m_debugVerts.setVertexData(verts);
    const auto& corners = cam.getComponent<cro::Camera>().getFrustumCorners();
    const auto worldTx = cam.getComponent<cro::Transform>().getWorldTransform();
    const auto fwd = cam.getComponent<cro::Transform>().getForwardVector() * 20.f;
    const auto worldPos = cam.getComponent<cro::Transform>().getWorldPosition();
    static constexpr std::array indices = { 0,4,5,1,0 };
    verts.clear();
    for (auto i : indices)
    {
        const auto c = worldTx * corners[i];

        glm::vec2 pos(c.x, -c.z);
        verts.emplace_back(pos, cro::Colour::Blue);
    }
    glm::vec2 p(worldPos.x, -worldPos.z);
    verts.emplace_back(p, cro::Colour::LightGrey);
    verts.emplace_back(p + glm::vec2(fwd.x, -fwd.z), cro::Colour::DarkGrey);
    
    verts.emplace_back(p, cro::Colour::LightGrey);
    const auto boxPos = m_boundingBoxes[queryIndex].getCentre();
    verts.emplace_back(glm::vec2(boxPos.x, -boxPos.z), cro::Colour::DarkGrey);
    
    m_debugFrustum.setVertexData(verts);

    m_debugTexture.clear(cro::Colour::Blue);
    m_debugVerts.draw();
    m_debugFrustum.draw();
    m_debugTexture.display();
#endif
}