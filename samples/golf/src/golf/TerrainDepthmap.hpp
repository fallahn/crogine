/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include <crogine/graphics/DepthTexture.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MaterialResource.hpp>
#include <crogine/graphics/ShaderResource.hpp>
#include <crogine/graphics/MeshResource.hpp>
#include <crogine/ecs/Scene.hpp>

#include <array>

struct HoleData;
class TerrainDepthmap final
{
public:
    TerrainDepthmap();

    void setModel(const HoleData&);

    //updates the given number of tiles
    void update(std::int32_t count);

    cro::TextureID getTexture() const;

    cro::TextureID getTextureAt(std::uint32_t idx) const;

    //if there's only one hole loaded then we need to force swap indices
    void forceSwap() { std::swap(m_srcTexture, m_dstTexture); }

    glm::ivec2 getGridCount() const;

private:
    std::uint32_t m_gridIndex;
    std::size_t m_srcTexture;
    std::size_t m_dstTexture;
    std::array<cro::DepthTexture, 2u> m_textures;

    cro::Scene m_scene;
    cro::MeshResource m_meshes;
    cro::ShaderResource m_shaders;
    cro::MaterialResource m_materials;
    cro::Material::Data m_holeMaterial;

    cro::Entity m_holeEnt;
    cro::Entity m_terrainEnt;
    cro::Texture m_heightmap;

    void buildScene();
};