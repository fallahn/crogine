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

#include "VatAnimationSystem.hpp"
#include "VatFile.hpp"

#include <crogine/ecs/components/Model.hpp>
#include <crogine/util/Random.hpp>

void VatAnimation::setVatData(const VatFile& file)
{
    CRO_ASSERT(file.m_frameCount != 0, "");

    totalTime = static_cast<float>(file.m_frameCount) / file.m_frameRate;
    loopTime = static_cast<float>(file.m_frameLoop) / file.m_frameRate;

    targetTime = loopTime;
    //LogI << "Target Time: " << targetTime << std::endl;
}

void VatAnimation::applaud()
{
    targetTime = totalTime;
    currentTime = loopTime - cro::Util::Random::value(0.2f, 0.7f);

    offsetMultiplier = 0.f;
}

VatAnimationSystem::VatAnimationSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(VatAnimationSystem))
{
    requireComponent<VatAnimation>();
    requireComponent<cro::Model>();
}

//public
void VatAnimationSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& anim = entity.getComponent<VatAnimation>();
        anim.currentTime += dt;

        if (anim.currentTime > anim.targetTime)
        {
            anim.currentTime -= anim.targetTime;

            //always default back to idle looping.
            anim.targetTime = anim.loopTime;
        }

        if (anim.targetTime < anim.totalTime)
        {
            //we're in the idle loop so slow desync instances
            anim.offsetMultiplier = std::min(1.f, anim.offsetMultiplier + dt);
        }

        //would prefer not to do the string lookup, but perf hit is negligable...
        float normTime = anim.currentTime / anim.totalTime;
        float targTime = anim.targetTime / anim.totalTime;
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_time", normTime);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_maxTime", targTime);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_offsetMultiplier", anim.offsetMultiplier);

        entity.getComponent<cro::Model>().setShadowMaterialProperty(0, "u_time", normTime);
        entity.getComponent<cro::Model>().setShadowMaterialProperty(0, "u_maxTime", targTime);
        entity.getComponent<cro::Model>().setShadowMaterialProperty(0, "u_offsetMultiplier", anim.offsetMultiplier);
    }
}