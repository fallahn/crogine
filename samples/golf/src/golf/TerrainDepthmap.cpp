/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>

namespace
{
    //8x5 texture grid @ 800x800px
    //gives us 40 metres per tile, 20 pixels per metre
    constexpr std::uint32_t TextureSize = 800u;
    constexpr std::uint32_t ColCount = 8u;
    constexpr std::uint32_t RowCount = 5u;
    constexpr std::uint32_t TextureCount = ColCount * RowCount;
    constexpr float TileSize = 320.f / ColCount;

    constexpr float CameraHeight = 10.f;
    constexpr float WaterLevel = -0.02f;
    constexpr float MaxDepth = -1.f;
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
void TerrainDepthmap::setModel(const cro::Mesh::Data& meshData)
{
    //destroy old model and create new from meshdata / custom material
    m_scene.destroyEntity(m_modelEnt);

    m_modelEnt = m_scene.createEntity();
    m_modelEnt.addComponent<cro::Transform>();
    m_modelEnt.addComponent<cro::Model>(meshData, m_material);

    m_gridIndex = 0;
    std::swap(m_srcTexture, m_dstTexture);
}

void TerrainDepthmap::update(std::uint32_t count)
{
    for (auto i = 0u; i < count && m_gridIndex < TextureCount; ++i, ++m_gridIndex)
    {
        auto x = m_gridIndex % ColCount;
        auto y = m_gridIndex / ColCount;
        m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition({ static_cast<float>(x * TileSize), CameraHeight, -static_cast<float>(y * TileSize) });

        //we need to update the scene to make sure camera settings are updated
        m_scene.simulate(0.1f);

        m_textures[m_dstTexture].clear(m_gridIndex);
        m_scene.render();
        m_textures[m_dstTexture].display();
    }
}

cro::TextureID TerrainDepthmap::getTexture() const
{
    return m_textures[m_srcTexture].getTexture();
}

cro::TextureID TerrainDepthmap::getTextureAt(std::uint32_t idx) const
{
#ifdef GL41
    return 0; //not available on macOS, but only used for debugging anyway
#else
    return m_textures[m_srcTexture].getTexture(idx);
#endif
}

//private
void TerrainDepthmap::buildScene()
{
    //create material
    auto shaderID = m_shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::DepthMap);
    auto materialID = m_materials.add(m_shaders.get(shaderID));
    m_material = m_materials.get(materialID);


    //set up scene for rendering
    auto& mb = cro::App::getInstance().getMessageBus();
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
    m_modelEnt = m_scene.createEntity();

    auto resizeCallback = [](cro::Camera& cam)
    {
        cam.setOrthographic(0.f, static_cast<float>(TileSize), 0.f, static_cast<float>(TileSize), CameraHeight - WaterLevel, CameraHeight - MaxDepth);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };
    resizeCallback(m_scene.getActiveCamera().getComponent<cro::Camera>());
    m_scene.getActiveCamera().getComponent<cro::Camera>().resizeCallback = resizeCallback;
    m_scene.getActiveCamera().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -90.f * cro::Util::Const::degToRad);
    //position is set when updating
}