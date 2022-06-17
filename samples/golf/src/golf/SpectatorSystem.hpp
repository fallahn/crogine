/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#pragma once

#include <crogine/ecs/System.hpp>

namespace cro::Util::Maths
{
    class Spline;
}

struct Spectator final
{
    float progress = 0.f;
    float stateTime = 0.f;
    float pauseTime = 1.f;

    float direction = 1.f;
    static constexpr float walkSpeed = 1.4f;
    const cro::Util::Maths::Spline* path = nullptr;

    float rotation = 0.f;
    float targetRotation = 0.f;

    enum class State
    {
        Walk, Pause
    }state = State::Pause;

    struct AnimID final
    {
        enum
        {
            Walk, Idle,
            Count
        };
    };
    std::array<std::size_t, AnimID::Count> anims = {};
};

class SpectatorSystem final : public cro::System
{
public:
    explicit SpectatorSystem(cro::MessageBus&);

    void process(float) override;

private:
    void onEntityAdded(cro::Entity) override;
};