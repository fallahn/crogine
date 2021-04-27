/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include <vector>

struct SpawnerAnimation final
{
    std::size_t tableIndex = 0;
    std::size_t activeIndex = 0; //if the animation is retriggered before this is finished this waits for the index to catch up
    bool active = true;

    cro::Entity spinnerEnt;

    void start();
};

class SpawnerAnimationSystem final : public cro::System 
{
public:
    explicit SpawnerAnimationSystem(cro::MessageBus&);

    void process(float) override;

private:
    std::vector<float> m_wavetable;

    void onEntityAdded(cro::Entity) override;
};
