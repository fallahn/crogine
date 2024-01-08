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

#include "WeatherAnimationSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/OpenGL.hpp>

WeatherAnimationSystem::WeatherAnimationSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(WeatherAnimationSystem)),
    m_hidden        (true),
    m_opacity       (0.f),
    m_targetOpacity (1.f),
    m_shaderID      (0),
    m_uniformID     (-1)
{
    requireComponent<WeatherAnimation>();
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
}

//public
void WeatherAnimationSystem::process(float dt)
{
    if (!m_hidden)
    {
        auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getPosition();
        camPos.x = std::floor(camPos.x / AreaEnd[0]);
        camPos.z = std::floor(camPos.z / AreaEnd[2]);

        camPos.x *= AreaEnd[0];
        camPos.y = 0.f;
        camPos.z *= AreaEnd[2];

        for (auto entity : getEntities())
        {
            auto& anim = entity.getComponent<WeatherAnimation>();
            entity.getComponent<cro::Transform>().setPosition(anim.basePosition + camPos);
        }

        if (m_targetOpacity < m_opacity)
        {
            m_opacity = std::max(m_targetOpacity, m_opacity - (dt * 0.5f));
            updateShader();

            if (m_opacity == 0)
            {
                m_hidden = true;

                for (auto e : getEntities())
                {
                    e.getComponent<cro::Model>().setHidden(true);
                }
            }
        }
        else if (m_targetOpacity > m_opacity)
        {
            m_opacity = std::min(m_targetOpacity, m_opacity + (dt * 0.5f));
            updateShader();
        }
    }
}

void WeatherAnimationSystem::setHidden(bool hidden)
{
    if (hidden)
    {
        m_targetOpacity = 0.f;
    }
    else
    {
        m_hidden = false;
        m_targetOpacity = 1.f;
        for (auto e : getEntities())
        {
            e.getComponent<cro::Model>().setHidden(false);
        }
    }
}

void WeatherAnimationSystem::setMaterialData(const cro::Shader& shader, cro::Colour colour)
{
    m_shaderID = shader.getGLHandle();
    m_uniformID = shader.getUniformID("u_colour");
    m_colour = colour;
}

//private
void WeatherAnimationSystem::updateShader()
{
    glUseProgram(m_shaderID);
    glUniform4f(m_uniformID, m_colour.getRed(), m_colour.getGreen(), m_colour.getBlue(), m_opacity);
}