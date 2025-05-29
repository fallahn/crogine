/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "SeaSystem.hpp"
#include "ResourceIDs.hpp"
#include "GameConsts.hpp"

#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>

#include <crogine/graphics/ModelDefinition.hpp>

namespace
{
#include "SeaShader.inl"
}

SeaSystem::SeaSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(SeaSystem)),
    m_waterIndex    (0)
{
    requireComponent<SeaComponent>();
    requireComponent<cro::Transform>();
    requireComponent<cro::Model>();
}

//public
void SeaSystem::handleMessage(const cro::Message&)
{

}

void SeaSystem::process(float dt)
{
    static float elapsed = dt;
    elapsed += dt;
    
    glUseProgram(m_shaderProperties.shaderID);
    glUniform1f(m_shaderProperties.timeUniform, elapsed);
    glUseProgram(0);

    auto oldIndex = m_waterIndex;
    if (m_waterTimer.elapsed().asSeconds() > 1.f / 24.f)
    {
        m_waterTimer.restart();

        m_waterIndex = (m_waterIndex + 1) % m_waterTextures.size();
    }

    auto entities = getEntities();
    for (auto entity : entities)
    {
        auto pos = entity.getComponent<cro::Transform>().getWorldPosition();
        pos /= (SeaRadius * 2.f);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_textureOffset", glm::vec2(pos.x, -pos.z));

        if (m_waterIndex != oldIndex)
        {
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_normalMap", cro::TextureID(m_waterTextures[m_waterIndex].getGLHandle()));
        }
    }
}

std::int32_t SeaSystem::loadResources(cro::ResourceCollection& rc)
{
    rc.shaders.loadFromString(ShaderID::Sea, SeaVertex, SeaFragment);
    auto matID = rc.materials.add(rc.shaders.get(ShaderID::Sea));

    const std::string basePath = "assets/images/water/water (";
    for (auto i = 0u; i < m_waterTextures.size(); ++i)
    {
        auto path = basePath + std::to_string((i * 3) + 1) + ").png";
        if (!m_waterTextures[i].loadFromFile(path, true))
        {
            LogW << "Failed to load " << path << std::endl;
        }
        else
        {
            m_waterTextures[i].setSmooth(true);
            m_waterTextures[i].setRepeated(true);

        }
    }
    rc.materials.get(matID).setProperty("u_normalMap", m_waterTextures[m_waterIndex]);



    m_shaderProperties.shaderID = rc.shaders.get(ShaderID::Sea).getGLHandle();

    //if (uniforms.count("u_time"))
    {
        m_shaderProperties.timeUniform = rc.shaders.get(ShaderID::Sea).getUniformID("u_time");
    }

    return matID;
}