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

#include "EmpSystem.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/core/Clock.hpp>

EmpSystem::EmpSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(EmpSystem))
{
    requireComponent<Emp>();
    requireComponent<cro::Transform>();
}


void EmpSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto& entity : entities)
    {
        auto& emp = entity.getComponent<Emp>();
        emp.currentTime = std::min(emp.currentTime + dt, 1.f);

        if (emp.currentTime == 1)
        {
            //finished updating, move out of view
            //entity.getComponent<cro::Transform>().setPosition(glm::vec3(-80.f));
        }
        else
        {
            //update shader
            entity.getComponent<cro::Model>().setMaterialProperty(0, "u_time", emp.currentTime);
        }
    }
}