/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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

#include "../../detail/GLCheck.hpp"

#include <crogine/ecs/systems/BillboardSystem.hpp>
#include <crogine/ecs/components/BillboardCollection.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/detail/OpenGL.hpp>

#include <crogine/graphics/BillboardMeshBuilder.hpp>

using namespace cro;

namespace
{

}

BillboardSystem::BillboardSystem(MessageBus& mb)
    : System(mb, typeid(BillboardSystem))
{
    requireComponent<BillboardCollection>();
}

//public
void BillboardSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& bbc = entity.getComponent<BillboardCollection>();
        if (bbc.m_dirty)
        {
            //update the model data
            std::vector<BillboardMeshBuilder::VertexLayout> vertexData;
            std::vector<std::uint16_t> indexData;
            auto& meshData = entity.getComponent<cro::Model>().getMeshData();

            //boundingbox
            meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
            meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());

            const auto& quads = bbc.m_billboards;
            for (const auto& quad : quads)
            {
                //the base position of the quad is stored in the vertex Normal data
                //rather than any actual normal data.

                auto& v1 = vertexData.emplace_back();
                v1.pos =
                {
                    -quad.origin.x,
                    -quad.origin.y,
                    0.f
                };
                v1.colour = quad.colour;                
                v1.rootPos = quad.position;
                v1.uvCoords =
                {
                    quad.textureRect.left,
                    quad.textureRect.bottom
                };
                v1.size = quad.size;


                //--------------------

                auto& v2 = vertexData.emplace_back();
                v2.pos =
                {
                    -quad.origin.x + quad.size.x,
                    -quad.origin.y,
                    0.f
                };
                v2.colour = quad.colour;
                v2.rootPos = quad.position;
                v2.uvCoords =
                {
                    quad.textureRect.left + quad.textureRect.width,
                    quad.textureRect.bottom
                };
                v2.size = quad.size;

                //--------------------

                auto& v3 = vertexData.emplace_back();
                v3.pos =
                {
                    -quad.origin.x + quad.size.x,
                    -quad.origin.y + quad.size.y,
                    0.f
                };
                v3.colour = quad.colour;
                v3.rootPos = quad.position;
                v3.uvCoords =
                {
                    quad.textureRect.left + quad.textureRect.width,
                    quad.textureRect.bottom + quad.textureRect.height
                };
                v3.size = quad.size;


                //-------------------

                auto& v4 = vertexData.emplace_back();
                v4.pos =
                {
                    -quad.origin.x,
                    -quad.origin.y + quad.size.y,
                    0.f
                };
                v4.colour = quad.colour;
                v4.rootPos = quad.position;
                v4.uvCoords =
                {
                    quad.textureRect.left,
                    quad.textureRect.bottom + quad.textureRect.height
                };
                v4.size = quad.size;



                //min point - not strictly accurate but enough to encompass the bounds
                if (meshData.boundingBox[0].x > quad.position.x - quad.size.x)
                {
                    meshData.boundingBox[0].x = quad.position.x - quad.size.x;
                }
                if (meshData.boundingBox[0].y > quad.position.y - quad.size.y)
                {
                    meshData.boundingBox[0].y = quad.position.y - quad.size.y;
                }
                if (meshData.boundingBox[0].z > quad.position.z)
                {
                    meshData.boundingBox[0].z = quad.position.z;
                }

                //maxpoint
                if (meshData.boundingBox[1].x < quad.position.x + quad.size.x)
                {
                    meshData.boundingBox[1].x = quad.position.x + quad.size.x;
                }
                if (meshData.boundingBox[1].y < quad.position.y + quad.size.y)
                {
                    meshData.boundingBox[1].y = quad.position.y + quad.size.y;
                }
                if (meshData.boundingBox[1].z < quad.position.z)
                {
                    meshData.boundingBox[1].z = quad.position.z;
                }


                const auto baseIndex = static_cast<std::uint32_t>(vertexData.size());

                //two tris
                indexData.push_back(baseIndex);
                indexData.push_back(baseIndex + 2);
                indexData.push_back(baseIndex + 3);

                indexData.push_back(baseIndex + 2);
                indexData.push_back(baseIndex);
                indexData.push_back(baseIndex + 1);
            }

            meshData.vertexCount = vertexData.size();
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.vboID));
            glCheck(glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(BillboardMeshBuilder::VertexLayout), vertexData.data(), GL_DYNAMIC_DRAW));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indexData.size());
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].ibo));
            glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexData.size() * sizeof(std::uint16_t), indexData.data(), GL_DYNAMIC_DRAW));

            //update bounding sphere
            const auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
            meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
            meshData.boundingSphere.radius = glm::length(rad);

            bbc.m_dirty = false;
        }
    }
}