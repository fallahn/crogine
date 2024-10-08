/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "DriftSystem.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Wavetable.hpp>

DriftSystem::DriftSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(DriftSystem))
{
    //create wave table
    m_waveTable = cro::Util::Wavetable::sine(0.07f, 0.2f);

    requireComponent<Drifter>();
    requireComponent<cro::Transform>();
}

//public
void DriftSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& cp = entity.getComponent<Drifter>();
        float position = m_waveTable[cp.currentIndex] * cp.amplitude;
        cp.currentIndex = (cp.currentIndex + 1) % m_waveTable.size();

        //auto& tx = entity.getComponent<cro::Transform>();
        //auto currPos = tx.getPosition();
        //currPos.y = position;
        //tx.setPosition(currPos);

        entity.getComponent<cro::Transform>().move({ 0.f, position * dt, 0.f });
    }
}