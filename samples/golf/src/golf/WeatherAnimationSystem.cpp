/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2026
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
#include "../Colordome-32.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/detail/OpenGL.hpp>

#ifdef DEBUG_UI
#include <crogine/gui/Gui.hpp>
#endif

WeatherAnimationSystem::WeatherAnimationSystem(cro::MessageBus& mb, cro::Scene& skyboxScene)
    : cro::System   (mb, typeid(WeatherAnimationSystem)),
    m_starsAmount   (0.f),
    m_skyboxScene   (skyboxScene),
    m_skyColour     (1.f),
    m_overcastColour(CD32::Colours[CD32::GreyMid].getVec4()),
    m_adaptSkyColour(false),
    m_hidden        (true),
    m_opacity       (0.f),
    m_targetOpacity (1.f),
    m_shaderID      (0),
    m_uniformID     (-1)
{
    requireComponent<WeatherAnimation>();
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();

#ifdef DEBUG_UI
    registerWindow([this]() 
        {
            ImGui::Begin("Weather");
            ImGui::Text("Opacity %.2f", m_opacity);
            ImGui::Text("Target %.2f", m_targetOpacity);
            ImGui::End();
        });
#endif
}

//public
void WeatherAnimationSystem::process(float dt)
{
    if (!m_hidden)
    {
        const auto& camTx = getScene()->getActiveCamera().getComponent<cro::Transform>();
        auto camPos = camTx.getPosition() + camTx.getForwardVector() * 10.f;
        //camPos.x = std::floor(camPos.x / AreaEnd[0]);
        //camPos.z = std::floor(camPos.z / AreaEnd[2]);

        //camPos.x *= AreaEnd[0];
        camPos.y = 0.f;
        //camPos.z *= AreaEnd[2];

        for (auto entity : getEntities())
        {
            const auto& anim = entity.getComponent<WeatherAnimation>();
            entity.getComponent<cro::Transform>().setPosition(/*anim.basePosition +*/ camPos);
        }

        if (m_targetOpacity < m_opacity)
        {
            m_opacity = std::max(m_targetOpacity, m_opacity - (dt * 0.5f));
            updateShader();

            updateSkyColour();

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

            updateSkyColour();
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
    updateSkyColour();
}

void WeatherAnimationSystem::setMaterialData(const cro::Shader& shader, cro::Colour colour)
{
    m_shaderID = shader.getGLHandle();
    m_uniformID = shader.getUniformID("u_colour");
    m_colour = colour;
}

void WeatherAnimationSystem::setClearSkyColour(cro::Colour c)
{
    m_adaptSkyColour = true;
    m_skyColour = c.getVec4();
}

//private
void WeatherAnimationSystem::updateSkyColour()
{
    const auto opacity = std::clamp(m_opacity, 0.f, 1.f);
    if (m_adaptSkyColour)
    {
        //set the sunlight
        const auto colour = cro::Colour::White.getVec4() * ((std::max(0.f, (1.f - opacity)) * 0.45f) + 0.55f);

        getScene()->getSunlight().getComponent<cro::Sunlight>().setColour(colour);
        m_skyboxScene.getSunlight().getComponent<cro::Sunlight>().setColour(colour);

        //set sky colour
        const auto skyColour = glm::mix(m_skyColour, m_overcastColour, opacity);
        auto colours = m_skyboxScene.getSkyboxColours();
        colours.top = skyColour;
        m_skyboxScene.setSkyboxColours(colours);
        
        m_skyboxScene.setCloudmapAmount(0.3f + (0.55f * opacity));
    }

    m_skyboxScene.setStarsAmount(glm::mix(m_starsAmount, 0.f, opacity));
}

void WeatherAnimationSystem::updateShader()
{
    glUseProgram(m_shaderID);
    glUniform4f(m_uniformID, m_colour.getRed(), m_colour.getGreen(), m_colour.getBlue(), m_colour.getAlpha() * m_opacity);
}