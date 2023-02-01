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

#pragma once

#include <crogine/ecs/Entity.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>


#include <array>

namespace cro
{
    class Scene;
    class Model;
}

struct TerrainChunk final
{
    std::int32_t itemCount = 0;

    struct LOD final
    {
        bool visible = false; //whether or not to include this LOD
        struct ModelNode final
        {
            std::vector<glm::mat4> transforms;
            std::vector<glm::mat3> normalMatrices;
        };
        std::array<ModelNode, 4u> models = {};
    };
    std::array<LOD, 2u> lodData = {};

    std::uint8_t lod0 = 0;
    std::uint8_t lod1 = 0;
};

class TerrainChunker final : public cro::GuiClient
{
public:
    explicit TerrainChunker(const cro::Scene&);

    void update();

    //swaps with existing
    void setChunks(std::vector<TerrainChunk>&);

    void addModel(cro::Model*, std::uint32_t lod, std::uint32_t idx);

    static constexpr int32_t ChunkCountX = 6;
    static constexpr int32_t ChunkCountY = 4;
    static constexpr int32_t ChunkCount = ChunkCountX * ChunkCountY;

private:
    const cro::Scene& m_scene;
    std::vector<TerrainChunk> m_chunks;

    //these are indices to the chunk array
    std::vector<std::int32_t> m_visible;
    std::vector<std::int32_t> m_previouslyVisible;

    std::array<std::array<cro::Model*, 4u>, 2u> m_models = {};

#ifdef CRO_DEBUG_
    cro::RenderTexture m_debugTexture;
    cro::Texture m_debugQuadTexture;
    cro::SimpleQuad m_debugQuad;
    cro::SimpleVertexArray m_debugCamera;
#endif
};