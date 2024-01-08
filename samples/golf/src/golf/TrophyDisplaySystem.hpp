/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

struct TrophyDisplay final
{
    std::size_t tableIndex = 0;
    float startHeight = 0.f;

    float delay = 0.f;
    float scale = 0.f;

    std::int32_t particleCount = 3;
    float particleTime = 0.f;
    static constexpr float ParticleInterval = 0.6f;

    enum
    {
        Idle, In, Active
    }state = Idle;
    cro::Entity label;
};

class TrophyDisplaySystem final : public cro::System
{
public:
    explicit TrophyDisplaySystem(cro::MessageBus&);

    void process(float) override;

private:
    std::vector<float> m_wavetable;

    void onEntityAdded(cro::Entity) override;
};