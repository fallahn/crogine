/*-----------------------------------------------------------------------

Matt Marchant 2024
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
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

#include <crogine/gui/GuiClient.hpp>
#include <crogine/util/Random.hpp>

//remember if we modify these we need to re-calc the matrix
//used for projection in the shader
static inline constexpr glm::uvec2 LightMapSize(1024u, 512u);
static inline constexpr cro::FloatRect LightMapWorldCoords(-20.f, -20.f, 40.f, 20.f);

namespace cro
{
    class RenderTexture;
}

struct LightmapProjector final
{
    cro::Colour colour;
    float size = 10.f;
    float brightness = 1.f;

    std::vector<float> pattern;
    std::size_t currentIndex = 0;
    float animationTime = 0.f;

    void setPattern(const std::string& s)
    {
        //use the classic quake engine light flicker pattern
        pattern.clear();
        currentIndex = 0;

        for (auto c : s)
        {
            pattern.push_back(static_cast<float>(c - 'a') / 13.f);
        }

        if (!pattern.empty())
        {
            currentIndex = cro::Util::Random::value(0u, pattern.size()) % pattern.size();
        }
    }
};

class LightmapProjectionSystem final : public cro::System, public cro::GuiClient
{
public:
    LightmapProjectionSystem(cro::MessageBus&, cro::RenderTexture*);

    void process(float) override;

private:
    cro::SimpleVertexArray m_vertexArray;
    cro::Texture m_texture;
    cro::RenderTexture* m_renderTexture;

    glm::vec2 toVertPosition(glm::vec3) const;
};