/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "GameConsts.hpp"

#include <crogine/ecs/Scene.hpp>

class BeaconCallback final
{
public:
    explicit BeaconCallback(const cro::Scene& scene)
        : m_scene(scene) {}

    void operator() (cro::Entity e, float dt)
    {
        auto pos = e.getComponent<cro::Transform>().getWorldPosition();
        glm::vec2 beaconPos(pos.x, -pos.z);

        pos = m_scene.getActiveCamera().getComponent<cro::Transform>().getWorldPosition();
        glm::vec2 camPos(pos.x, -pos.z);

        float amount = smoothstep(MinDist / MaxDist, 1.f, std::min(1.f, glm::length2(beaconPos - camPos) / (MaxDist * MaxDist)));

        if (amount == 0)
        {
            e.getComponent<cro::Model>().setHidden(true);
        }
        else
        {
            e.getComponent<cro::Model>().setHidden(false);
            if (amount != 1)
            {
                //model is blended additively so black == transparent
                cro::Colour c(amount, amount, amount);
                e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", c);
            }

            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, dt);
        }

        e.getComponent<cro::Callback>().getUserData<float>() = amount; //used in debugging
    }

private:

    const cro::Scene& m_scene;

    static constexpr float MaxDist = 100.f;
    static constexpr float MinDist = 16.f;
};
