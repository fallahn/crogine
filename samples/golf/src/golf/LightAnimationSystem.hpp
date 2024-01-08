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
#include <crogine/util/Random.hpp>

#include <vector>

struct LightAnimation final
{
    static constexpr float FrameTime = 1.f / 10.f;

    std::vector<float> pattern;
    std::size_t currentIndex = 0;
    glm::vec4 baseColour = glm::vec4(1.f);

    LightAnimation() = default;

    LightAnimation(float lowest, float highest)
    {
        for (auto i = 0; i < 20; ++i)
        {
            pattern.push_back(cro::Util::Random::value(lowest, highest));
        }
    }

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

    static const std::string FlickerA;
    static const std::string FlickerB;
    static const std::string FlickerC;
};

class LightAnimationSystem final : public cro::System
{
public:
    explicit LightAnimationSystem(cro::MessageBus&);

    void process(float) override;

private:
    float m_accumulator;

    void onEntityAdded(cro::Entity) override;
};