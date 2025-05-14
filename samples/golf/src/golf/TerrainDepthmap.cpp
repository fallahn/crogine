/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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

#include "TerrainDepthmap.hpp"
#include "GameConsts.hpp"
#include "HoleData.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

#include "../ErrorCheck.hpp"

namespace
{
    //40 metres per tile, 32 pixels per metre

    constexpr std::uint32_t MetresPerTile = 40u;
    static_assert((MapSize.x % MetresPerTile) == 0 && (MapSize.y % MetresPerTile) == 0, "Map size must be multiple of 40");

    //TODO this could be much optimised by only creating as many layers as actually
    //intersect the terrain. No idea how to calculate that though
    constexpr std::uint32_t TextureSize = 1280u;
    constexpr std::uint32_t ColCount = MapSize.x / MetresPerTile;
    constexpr std::uint32_t RowCount = MapSize.y / MetresPerTile;
    constexpr std::uint32_t TextureCount = ColCount * RowCount;
    constexpr float TileSize = MapSizeFloat.x / ColCount;

    constexpr float CameraHeight = 10.f;
    constexpr float MaxDepth = TerrainLevel - WaterLevel;

    const std::string TerrainVertex = 
        R"(
        ATTRIBUTE vec4 a_position;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        uniform sampler2D u_heightMap;
        const float MaxHeight = MAX_HEIGHT;
        
#include MAP_SIZE


        void main()
        {
            vec4 position = a_position;
            vec2 coord = vec2(a_position.x + 0.5, -a_position.z + 0.5) / MapSize;
            position.y = TEXTURE(u_heightMap, coord).g * MaxHeight;

            gl_Position = u_viewProjectionMatrix * u_worldMatrix * position;
        })";

    const std::string TerrainFragment =
        R"(
        OUTPUT
        void main(){FRAG_OUT = vec4(1.0);}
        )";

}

TerrainDepthmap::TerrainDepthmap()
    : m_gridIndex   (0),
    m_srcTexture    (0),
    m_dstTexture    (1),
    m_scene         (cro::App::getInstance().getMessageBus())
{
    for (auto& texture : m_textures)
    {
        if (!texture.create(TextureSize, TextureSize, TextureCount))
        {
            LogE << "Unable to create requested depth texture" << std::endl;
        }
    }

    buildScene();
}

//public
void TerrainDepthmap::setModel(const HoleData& holeData)
{
    //handle cases where images don't match map size
    cro::Image img(true);
    img.loadFromFile(holeData.mapPath);

    m_heightmap.create(MapSize.x, MapSize.y, img.getFormat());
    m_heightmap.update(img.getPixelData(), false, { 0,0,std::min(img.getSize().x, MapSize.x), std::min(img.getSize().y, MapSize.y) });
    m_terrainEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_heightMap", cro::TextureID(m_heightmap));

    //destroy old model and create new from meshdata / custom material
    //TODO for completeness we want to render all prop models because some,
    //such as boats or jetties, intersect the water. Not sure its worth the
    //extra effort though (and may block for too long)
    m_scene.destroyEntity(m_holeEnt);

    const auto& meshData = holeData.modelEntity.getComponent<cro::Model>().getMeshData();
    m_holeEnt = m_scene.createEntity();
    m_holeEnt.addComponent<cro::Transform>();
    m_holeEnt.addComponent<cro::Model>(meshData, m_holeMaterial);

    m_gridIndex = 0;
    //std::swap(m_srcTexture, m_dstTexture);
}

void TerrainDepthmap::update(std::int32_t count)
{
    if (count == -1)
    {
        count = TextureCount;
    }

    for (auto i = 0; i < count && m_gridIndex < TextureCount; ++i, ++m_gridIndex)
    {
        auto x = m_gridIndex % ColCount;
        auto y = m_gridIndex / ColCount;
        m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition({ static_cast<float>(x * TileSize), CameraHeight, -static_cast<float>(y * TileSize) });

        //we need to update the scene to make sure camera settings are updated
        m_scene.simulate(0.1f);

        m_textures[m_srcTexture].clear(m_gridIndex);
       // m_textures[m_dstTexture].clear(m_gridIndex);
        m_scene.render();
        m_textures[m_srcTexture].display();
        //m_textures[m_dstTexture].display();
    }
}

cro::TextureID TerrainDepthmap::getTexture() const
{
    return m_textures[m_srcTexture].getTexture();
}

cro::TextureID TerrainDepthmap::getTextureAt(std::uint32_t idx) const
{
#ifdef GL41
    return cro::TextureID(0); //not available on macOS, but only used for debugging anyway
#else
    return m_textures[m_srcTexture].getTexture(idx);
#endif
}

glm::ivec2 TerrainDepthmap::getGridCount() const
{
    return { ColCount, RowCount };
}

std::int32_t TerrainDepthmap::getTileCount() const
{
    return TextureCount;
}

std::int32_t TerrainDepthmap::getMetresPerTile() const
{
    return MetresPerTile;
}

//private
void TerrainDepthmap::buildScene()
{
    static const std::string MapSizeString = "const vec2 MapSize = vec2(" + std::to_string(MapSize.x) + ".0, " + std::to_string(MapSize.y) + ".0); ";
    m_shaders.addInclude("MAP_SIZE", MapSizeString.c_str());


    //create material
    auto shaderID = m_shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::DepthMap);
    auto materialID = m_materials.add(m_shaders.get(shaderID));
    m_holeMaterial = m_materials.get(materialID);


    //set up scene for rendering
    auto& mb = cro::App::getInstance().getMessageBus();
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_holeEnt = m_scene.createEntity();

    auto resizeCallback = [](cro::Camera& cam)
    {
        //note the near/far plane values must match up in the water shader for proper linearisation
        cam.setOrthographic(0.f, static_cast<float>(TileSize), 0.f, static_cast<float>(TileSize), CameraHeight - WaterLevel, CameraHeight - MaxDepth);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    resizeCallback(m_scene.getActiveCamera().getComponent<cro::Camera>());
    m_scene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = resizeCallback;
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    //camera position is set when updating


    //create a terrain mesh - this is slightly different from the main terrain
    //in that the shader reads the heightmap directly. The buffer only has positions
    std::vector<glm::vec3> terrainBuffer(MapSize.x * MapSize.y);

    for (auto i = 0u; i < terrainBuffer.size(); ++i)
    {
        std::size_t x = i % (MapSize.x);
        std::size_t y = i / (MapSize.x);

        terrainBuffer[i] = { static_cast<float>(x), 0.f, -static_cast<float>(y) };
    }

    static constexpr auto xCount = MapSize.x;
    static constexpr auto yCount = MapSize.y;
    std::vector<std::uint32_t> indices;

    for (auto y = 0u; y < yCount - 1; ++y)
    {
        if (y > 0)
        {
            indices.push_back((y + 1) * xCount);
        }

        for (auto x = 0u; x < xCount; x++)
        {
            indices.push_back(((y + 1) * xCount) + x);
            indices.push_back((y * xCount) + x);
        }

        if (y < yCount - 2)
        {
            indices.push_back((y * xCount) + (xCount - 1));
        }
    }

    auto meshID = m_meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position, 1, GL_TRIANGLE_STRIP));
    shaderID = 0;
    m_shaders.loadFromString(shaderID, TerrainVertex, TerrainFragment, "#define MAX_HEIGHT " + std::to_string(MaxTerrainHeight) + "\n");
    materialID = m_materials.add(m_shaders.get(shaderID));
    auto material = m_materials.get(materialID);

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, TerrainLevel, 0.f });
    entity.addComponent<cro::Model>(m_meshes.getMesh(meshID), material);

    constexpr auto MaxBounds = glm::vec2(MapSize);
    auto* meshData = &entity.getComponent<cro::Model>().getMeshData();
    meshData->vertexCount = static_cast<std::uint32_t>(terrainBuffer.size());
    meshData->boundingBox[0] = glm::vec3(0.f, -10.f, 0.f);
    meshData->boundingBox[1] = glm::vec3(MaxBounds[0], 10.f, -MaxBounds[1]);
    meshData->boundingSphere.centre = { MaxBounds[0] / 2.f, 0.f, -MaxBounds[1] / 2.f };
    meshData->boundingSphere.radius = glm::length(meshData->boundingSphere.centre);

    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData->vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * terrainBuffer.size(), terrainBuffer.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto* submesh = &meshData->indexData[0];
    submesh->indexCount = static_cast<std::uint32_t>(indices.size());
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh->ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh->indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
    m_terrainEnt = entity;
}