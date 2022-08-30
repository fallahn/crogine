/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

struct PocketBall final
{
    std::int32_t id = -1;
    bool awake = true;
    float velocity = 0.f;

    //tracks how many exist so we can
    //pop the bottom ball to prevent overflow
    std::uint32_t totalCount = 0;
};

class PocketBallSystem final : public cro::System
{
public:
    explicit PocketBallSystem(cro::MessageBus& mb);

    void process(float) override;

    void removeBall(std::int8_t);

private:
    void onEntityAdded(cro::Entity) override;
};
