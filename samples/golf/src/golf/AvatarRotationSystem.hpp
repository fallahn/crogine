/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/util/Maths.hpp>

struct AvatarRotation final
{
    float currentRotation = 0.f;
    float targetRotation = 0.f;
    bool active = false;
};

//smooths rotation naimation from remote clients
class AvatarRotationSystem final : public cro::System
{
public:
    explicit AvatarRotationSystem(cro::MessageBus& mb)
        : cro::System(mb, typeid(AvatarRotationSystem))
    {
        requireComponent<cro::Transform>();
        requireComponent<AvatarRotation>();
    }

    void process(float dt) override
    {
        auto& entities = getEntities();
        for (auto entity : entities)
        {
            auto& avRot = entity.getComponent<AvatarRotation>();
            if (avRot.active)
            {
                auto rot = cro::Util::Maths::shortestRotation(avRot.currentRotation, avRot.targetRotation);
                if (std::abs(rot) < dt)
                {
                    avRot.currentRotation = avRot.targetRotation;
                    avRot.active = false;
                }
                else
                {
                    avRot.currentRotation += cro::Util::Maths::sgn(rot) * dt;
                }
                entity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, avRot.currentRotation);
            }
        }
    }
};
