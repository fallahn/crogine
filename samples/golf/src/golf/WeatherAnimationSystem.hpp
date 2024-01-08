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

#include <crogine/ecs/System.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Shader.hpp>

#include <array>

struct WeatherAnimation final
{
    glm::vec3 basePosition = glm::vec3(0.f);
};

class WeatherAnimationSystem final : public cro::System 
{
public:
    static constexpr std::array<float, 3u> AreaStart = { 0.f, 0.f, 0.f };
    static constexpr std::array<float, 3u> AreaEnd = { 20.f, 80.f, 20.f };

    explicit WeatherAnimationSystem(cro::MessageBus&);

    void process(float) override;

    void setHidden(bool);

    void setMaterialData(const cro::Shader&, cro::Colour);

    float getOpacity() const { return m_opacity; }

private:
    bool m_hidden;
    float m_opacity;
    float m_targetOpacity;

    std::uint32_t m_shaderID;
    std::int32_t m_uniformID;
    cro::Colour m_colour;

    void updateShader();
};
