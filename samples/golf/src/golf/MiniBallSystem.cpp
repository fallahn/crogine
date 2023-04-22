/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "MiniBallSystem.hpp"
#include "MinimapZoom.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

MiniBallSystem::MiniBallSystem(cro::MessageBus& mb, const MinimapZoom& mz)
    : cro::System(mb, typeid(MiniBallSystem)),
    m_minimapZoom(mz)
{
    requireComponent<cro::Transform>();
    requireComponent<cro::Drawable2D>();
    requireComponent<MiniBall>();
}

//public
void MiniBallSystem::process(float dt)
{
    const auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& ball = entity.getComponent<MiniBall>();

        if (!ball.parent.isValid())
        {
            //player left the game and parent was destroyed
            getScene()->destroyEntity(entity);
            continue;
        }

        if (ball.state == MiniBall::Animating)
        {
            ball.currentTime = std::max(0.f, ball.currentTime - (dt * 3.f));

            static constexpr float MaxScale = 6.f - 1.f;
            float scale = 1.f + (MaxScale * ball.currentTime);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(scale) * m_minimapZoom.mapScale);

            float alpha = 1.f - ball.currentTime;
            auto& verts = entity.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour.setAlpha(alpha);
            }

            if (ball.currentTime == 0)
            {
                ball.currentTime = 1.f;
                ball.state = MiniBall::Idle;
            }

            if (ball.parent.isValid())
            {
                auto position = ball.parent.getComponent<cro::Transform>().getPosition();
                entity.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(position), 0.1f));
            }
        }
        else
        {
            if (ball.parent.isValid())
            {
                auto position = ball.parent.getComponent<cro::Transform>().getPosition();
                entity.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(position), 0.1f));

                //set scale based on height
                static constexpr float MaxHeight = 40.f;
                float scale = 1.f + (position.y / MaxHeight);
                entity.getComponent<cro::Transform>().setScale(glm::vec2(scale) * m_minimapZoom.mapScale);
            }
        }
    }
}