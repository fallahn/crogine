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

#include "LightAnimationSystem.hpp"

#include <crogine/ecs/components/LightVolume.hpp>

const std::string LightAnimation::FlickerA = "mmmmmaaaaammmmmaaaaaabcdefgabcdefg";
const std::string LightAnimation::FlickerB = "mmmaaaabcdefgmmmmaaaammmaamm";
const std::string LightAnimation::FlickerC = "mmmaaammmaaammmabcdefaaaammmmabcdefmmmaaaa";

LightAnimationSystem::LightAnimationSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(LightAnimationSystem)),
    m_accumulator(0.f)
{
    requireComponent<LightAnimation>();
    requireComponent<cro::LightVolume>();
}

//public
void LightAnimationSystem::process(float dt)
{
    m_accumulator += dt;

    while (m_accumulator > LightAnimation::FrameTime)
    {
        auto& entities = getEntities();
        for (auto entity : entities)
        {
            auto& anim = entity.getComponent<LightAnimation>();
            
            if (!anim.pattern.empty())
            {
                float multiplier = anim.pattern[anim.currentIndex];
                anim.currentIndex = (anim.currentIndex + 1) % anim.pattern.size();

                entity.getComponent<cro::LightVolume>().colour = anim.baseColour * multiplier;
            }
        }

        m_accumulator -= LightAnimation::FrameTime;
    }
}

//private
void LightAnimationSystem::onEntityAdded(cro::Entity e)
{
    e.getComponent<LightAnimation>().baseColour = e.getComponent<cro::LightVolume>().colour.getVec4();
}