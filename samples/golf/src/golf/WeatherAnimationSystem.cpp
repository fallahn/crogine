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

#include "WeatherAnimationSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Transform.hpp>

WeatherAnimationSystem::WeatherAnimationSystem(cro::MessageBus& mb)
    : cro::System   (mb, typeid(WeatherAnimationSystem)),
    m_hidden        (true)
{
    requireComponent<WeatherAnimation>();
    requireComponent<cro::Model>();
    requireComponent<cro::Transform>();
}

//public
void WeatherAnimationSystem::process(float)
{
    if (!m_hidden)
    {
        auto camPos = getScene()->getActiveCamera().getComponent<cro::Transform>().getPosition();
        camPos.x = std::floor(camPos.x / AreaEnd[0]);
        camPos.z = std::floor(camPos.z / AreaEnd[2]);

        camPos.x *= AreaEnd[0];
        camPos.y = 0.f;
        camPos.z *= AreaEnd[2];

        for (auto entity : getEntities())
        {
            entity.getComponent<cro::Transform>().setPosition(entity.getComponent<WeatherAnimation>().basePosition + camPos);
        }
    }
}

void WeatherAnimationSystem::setHidden(bool hidden)
{
    for (auto e : getEntities())
    {
        e.getComponent<cro::Model>().setHidden(hidden);
    }
    m_hidden = hidden;
}