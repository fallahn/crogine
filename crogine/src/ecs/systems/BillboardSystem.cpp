/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/detail/AllocationResource.hpp>

#include <crogine/graphics/BillboardMeshBuilder.hpp>

using namespace cro;

namespace
{

}

BillboardSystem::BillboardSystem(MessageBus& mb)
    : System(mb, typeid(BillboardSystem))
{
    requireComponent<BillboardCollection>();
    requireComponent<Model>();
}

//public
void BillboardSystem::process(float)
{
    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& bbc = entity.getComponent<BillboardCollection>();

        //check if we have any pending callbacks which need assignment
        if (!bbc.m_pendingCallbacks.empty())
        {
            for (const auto& [camera, callback] : bbc.m_pendingCallbacks)
            {
                const auto drawListIndex = camera.getComponent<Camera>().getDrawListIndex();
                if (bbc.m_cameraCallbacks.size() <= drawListIndex)
                {
                    bbc.m_cameraCallbacks.resize(drawListIndex + 1);
                }

                //this is probably only pertinent when index is
                //zero as it suggests that the camera system may not
                //yet have been updated to assign valid indices and
                //all callbacks are being assigned to index 0
                if (drawListIndex == 0
                    && bbc.m_cameraCallbacks[0])
                {
                    LogW << "[Billboards] Billboard camera callback at index 0 already assigned - this may indicate an out-of-date index" << std::endl;
                }

                bbc.m_cameraCallbacks[drawListIndex] = callback;

                if (bbc.m_chunkSize == 0)
                {
                    LogW << "[Billboards] Setting a drawlist callback on a billboard collection with 0 chunk size. THIS IS PROBABLY BAD" << std::endl;
                }
            }
            bbc.m_pendingCallbacks.clear();
        }


        if (bbc.m_dirty)
        {
            //update the model data
            auto& model = entity.getComponent<Model>();
            std::vector<BillboardMeshBuilder::VertexLayout> vertexData;
            auto& meshData = model.getMeshData();

            //boundingbox
            meshData.boundingBox[0] = glm::vec3(std::numeric_limits<float>::max());
            meshData.boundingBox[1] = glm::vec3(std::numeric_limits<float>::lowest());

            const auto& quads = bbc.m_billboards;
            for (const auto& quad : quads)
            {
                //do this first else we'll be off by one quad...
                const auto baseIndex = static_cast<std::uint32_t>(vertexData.size());

                if (baseIndex > std::numeric_limits<std::uint16_t>::max()-4)
                {
                    LogW << "[Billboards] Index count reached " << baseIndex << ": no more billboards can be added" << std::endl;
                    break;
                }

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
                v1.uvCoords = glm::packSnorm2x16(
                {
                    quad.textureRect.left,
                    quad.textureRect.bottom
                });
                v1.size = glm::packHalf2x16(quad.size);


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
                v2.uvCoords = glm::packSnorm2x16(
                {
                    quad.textureRect.left + quad.textureRect.width,
                    quad.textureRect.bottom
                });
                v2.size = glm::packHalf2x16(quad.size);

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
                v3.uvCoords = glm::packSnorm2x16(
                {
                    quad.textureRect.left + quad.textureRect.width,
                    quad.textureRect.bottom + quad.textureRect.height
                });
                v3.size = glm::packHalf2x16(quad.size);


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
                v4.uvCoords = glm::packSnorm2x16(
                {
                    quad.textureRect.left,
                    quad.textureRect.bottom + quad.textureRect.height
                });
                v4.size = glm::packHalf2x16(quad.size);



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
            }

            meshData.vertexCount = vertexData.size();

            if (meshData.vertexCount > std::numeric_limits<std::uint16_t>::max())
            {
                LogW << "[Billboards] More vertices were added to billboard mesh than can be indexed" << std::endl;
            }

            //make sure to update the multidraw indices on the model
            //here first so that refreshVBO() sets the correct draw function
            //but AFTER the total bounds has been calculated.
            //TODO *however* this means double iterating, so maybe send this
            //all via a std::future and update the VBO once we have a result.
            std::vector<std::uint16_t> indexData;
            std::vector<std::vector<std::uint16_t>> indexChunkData;
            std::vector<std::pair<float, std::int32_t>> meanHeight; //track the average height of a chunk so culling options can use a 3D position

            bbc.m_chunkCount = { 0u, 0u };

            if (bbc.m_chunkSize > 0)
            {
                bbc.m_chunkCoordStart.x = meshData.boundingBox[0].x;
                bbc.m_chunkCoordStart.y = meshData.boundingBox[0].z;

                bbc.m_chunkCount.x = static_cast<std::uint32_t>(std::ceil((meshData.boundingBox[1].x - meshData.boundingBox[0].x) / bbc.m_chunkSize));
                bbc.m_chunkCount.y = static_cast<std::uint32_t>(std::ceil((meshData.boundingBox[1].z - meshData.boundingBox[0].z) / bbc.m_chunkSize));
                indexChunkData.resize(bbc.m_chunkCount.x * bbc.m_chunkCount.y);
                meanHeight.resize(indexChunkData.size());

                for (auto& dl : bbc.m_drawLists)
                {
                    dl.resize(indexChunkData.size());
                }
            }

            const auto getGridIndex = 
                [this, &bbc, &meshData](glm::vec3 pos)
                {    
                    if (bbc.m_chunkSize == 0u) //don't do chunking
                    {
                        //we're assuming that we'll never have 4 billion
                        //chunks - but that's the kind of assumption which
                        //blew up the Ariane 5.
                        return std::numeric_limits<std::uint32_t>::max();
                    }
                    
                    const std::uint32_t gridX = static_cast<std::uint32_t>((pos.x - meshData.boundingBox[0].x) / bbc.m_chunkSize);
                    const std::uint32_t gridY = static_cast<std::uint32_t>((pos.z - meshData.boundingBox[0].z) / bbc.m_chunkSize);
                    return (gridY * bbc.m_chunkCount.x) + gridX;
                };

            const auto insertQuadIndices =
                [](const std::uint32_t baseIndex, std::vector<std::uint16_t>& dest)
                {
                    //two tris
                    dest.push_back(baseIndex);
                    dest.push_back(baseIndex + 2);
                    dest.push_back(baseIndex + 3);

                    dest.push_back(baseIndex + 2);
                    dest.push_back(baseIndex);
                    dest.push_back(baseIndex + 1);
                };


            for (auto i = 0u; i < vertexData.size(); i += 4)
            {
                const auto gridIndex = getGridIndex(vertexData[i].rootPos);
                if (gridIndex == std::numeric_limits<std::uint32_t>::max())
                {
                    insertQuadIndices(i, indexData);
                }
                else
                {
                    if (gridIndex < indexChunkData.size())
                    {
                        insertQuadIndices(i, indexChunkData[gridIndex]);

                        //this value should be the same across all 4 verts.
                        meanHeight[gridIndex].first += vertexData[i].rootPos.y;
                        meanHeight[gridIndex].second++;
                    }
                    else
                    {
                        LogW << "[Billboards] " << gridIndex << ": invalid grid index" << std::endl;
                    }
                }
            }
            
            //concatenate the index arrays ready to upload to the IBO
            bbc.m_chunkIndices.clear();
            std::int32_t i = 0;
            for (const auto& chunk : indexChunkData)
            {
                auto& offsets = bbc.m_chunkIndices.emplace_back();
                offsets.count = static_cast<std::uint32_t>(chunk.size());
                offsets.offset = static_cast<std::uint32_t>(indexData.size() * sizeof(std::uint16_t));
                offsets.meanHeight = meanHeight[i].first / meanHeight[i].second;

                indexData.insert(indexData.end(), chunk.cbegin(), chunk.cend());
                i++;
            }

            //this isn't valid, but it marks the model as needing to
            //update the draw function to Multi-draw. This is correctly
            //updated by the render() function, below. TODO - FIXME!!
            if (bbc.m_chunkSize)
            {
                static constexpr GLsizei primer = 0;
                model.m_indexSizeArray = &primer;
            }

            bool needsVAORefresh = false;

            //we need to resize if larger than previous
            if (meshData.vboAllocator->getBlockCount(vertexData.size()) >
                meshData.vboAllocation.blockCount)
            {
                meshData.vboAllocator->freeAllocation(meshData.vboAllocation);
                meshData.vboAllocation = meshData.vboAllocator->newAllocation(vertexData.size());

                needsVAORefresh = true;
            }

            //resize IBO if needed
            meshData.indexData[0].indexCount = static_cast<std::uint32_t>(indexData.size());
            if (meshData.iboAllocator->getBlockCount(indexData.size()) >
                meshData.indexData[0].iboAllocation.blockCount)
            {
                meshData.iboAllocator->freeAllocation(meshData.indexData[0].iboAllocation);
                meshData.indexData[0].iboAllocation = meshData.iboAllocator->newAllocation(indexData.size());
                //needsVAORefresh = true;
            }

            if (needsVAORefresh)
            {
                model.refreshVAO();
            }

            //don't upload until everything is fully resized and refreshed
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.bufferID));
            glCheck(glBufferSubData(GL_ARRAY_BUFFER, meshData.vboAllocation.offset, vertexData.size() * sizeof(BillboardMeshBuilder::VertexLayout), vertexData.data()));
            glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

            meshData.indexData[0].iboAllocation.baseVertex = meshData.indexData[0].iboAllocation.offset / sizeof(std::uint16_t);
            glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].iboAllocation.bufferID));
            glCheck(glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[0].iboAllocation.offset, indexData.size() * sizeof(std::uint16_t), indexData.data()));


            //update bounding sphere
            const auto rad = (meshData.boundingBox[1] - meshData.boundingBox[0]) / 2.f;
            meshData.boundingSphere.centre = meshData.boundingBox[0] + rad;
            meshData.boundingSphere.radius = glm::length(rad);

            bbc.m_dirty = false;
        }
    }
}

void BillboardSystem::updateDrawList(Entity camera)
{
    const auto index = camera.getComponent<Camera>().getDrawListIndex();

    for (auto entity : getEntities())
    {
        const auto& model = entity.getComponent<Model>();
        //if (!model.isHidden())
        {
            auto& bbc = entity.getComponent<BillboardCollection>();
            if (index >= bbc.m_drawLists.size())
            {
                bbc.m_drawLists.resize(index + 1);
                bbc.m_drawLists[index].resize(bbc.m_chunkCount.x * bbc.m_chunkCount.y);

                bbc.m_cameraCallbacks.resize(index + 1);
            }

            //do custom culling if found
            if (!model.isHidden()
                && bbc.m_cameraCallbacks[index]
                && !bbc.m_chunkIndices.empty()) //*sigh* this is probably only ever needed for one frame
            {
                //TODO do we know if this is a shadow pass here? Probably query camera for active pass
                bbc.m_cameraCallbacks[index](camera, entity);
            }
            else
            {
                //just draw everything
                const auto& meshData = model.getMeshData();

                auto& drawlist = bbc.m_drawLists[index];
                drawlist.clear();
                drawlist.resize(1); //hmm should we be resizing every frame?

                drawlist.m_indexOffsetArray[0] = (reinterpret_cast<void*>(meshData.indexData[0].iboAllocation.offset));
                drawlist.m_indexSizeArray[0] = (static_cast<GLsizei>(meshData.indexData[0].indexCount));
                drawlist.m_size = 1;
            }
        }
    }
}

void BillboardSystem::render(Entity camera, const RenderTarget&)
{
    //set the draw list pointers on the model for this camera.
    const auto index = camera.getComponent<Camera>().getDrawListIndex();
    for (auto entity : getEntities())
    {
        auto& model = entity.getComponent<Model>();
        if (!model.isHidden())
        {
            const auto& meshData = model.getMeshData();

            const auto& bbc = entity.getComponent<BillboardCollection>();

            //TODO find out why this happens and fix it instead of hacking around it
            //*probably* because the camera gets set active after the first call to
            //updateDrawList()
            if (index < bbc.m_drawLists.size())
            {
                const auto& drawlist = bbc.m_drawLists[index];

                model.m_indexDrawCount = static_cast<GLsizei>(drawlist.m_size);
                model.m_indexOffsetArray = drawlist.m_indexOffsetArray.data();
                model.m_indexSizeArray = drawlist.m_indexSizeArray.data();
                //assert(model.m_indexDrawCount == drawlist.indexSizeArray.size());
            }
        }
    }
}