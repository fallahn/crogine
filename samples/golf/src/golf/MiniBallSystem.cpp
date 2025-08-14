/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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
#include "CommonConsts.hpp"
#include "ClientCollisionSystem.hpp"
#include "BallSystem.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    float vertexTimer = 0.f;
}

MiniBallSystem::MiniBallSystem(cro::MessageBus& mb, const MinimapZoom& mz)
    : cro::System(mb, typeid(MiniBallSystem)),
    m_minimapZoom(mz),
    m_activePlayer(0)
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
                entity.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(position), 0.01f * static_cast<float>(ball.playerID)));
            }
        }
        else
        {
            if (ball.parent.isValid())
            {
                auto position = ball.parent.getComponent<cro::Transform>().getPosition();
                entity.getComponent<cro::Transform>().setPosition(glm::vec3(m_minimapZoom.toMapCoords(position), 0.05f * static_cast<float>(ball.playerID)));

                //set scale based on height
                static constexpr float MaxHeight = 40.f;
                float scale = 1.f + (position.y / MaxHeight);
                entity.getComponent<cro::Transform>().setScale(glm::vec2(scale) * m_minimapZoom.mapScale * 2.f);


                //or if in bounds of the mini map
                auto miniBounds = ball.minimap.getComponent<cro::Transform>().getWorldTransform() * ball.minimap.getComponent<cro::Drawable2D>().getLocalBounds();
                //auto renderBounds = glm::inverse(entity.getComponent<cro::Transform>().getWorldTransform()) * miniBounds;
                entity.getComponent<cro::Drawable2D>().setCroppingArea(miniBounds, true);


                //fade out the ball if not the active player to make current player more prominent
                auto& verts = entity.getComponent<cro::Drawable2D>().getVertexData();
                float alpha = verts[0].colour.getAlpha();
                if (ball.playerID == m_activePlayer)
                {
                    alpha = std::min(1.f, alpha + dt);
                }
                else
                {
                    alpha = std::max(0.4f, alpha - dt);
                }
                for (auto& v : verts)
                {
                    v.colour.setAlpha(alpha);
                }

                //update the trail if in flight
                const auto state = ball.parent.getComponent<ClientCollider>().state;
                if (state == static_cast<std::uint8_t>(Ball::State::Flight)
                    || state == static_cast<std::uint8_t>(Ball::State::Roll)
                    || state == static_cast<std::uint8_t>(Ball::State::Putt))
                {
                    static constexpr float PointFreq = 0.25f;
                    static constexpr cro::Colour c(1.f, 1.f, 1.f, 0.5f);
                    
                    vertexTimer += dt;
                    if (vertexTimer > PointFreq)
                    {
                        const auto p = glm::vec2(entity.getComponent<cro::Transform>().getPosition());
                        vertexTimer -= PointFreq;

                        ball.minitrail.getComponent<cro::Drawable2D>().getVertexData().emplace_back(p, c);
                        ball.minitrail.getComponent<cro::Drawable2D>().updateLocalBounds();
                    }
                }
            }
        }
    }
}

void MiniBallSystem::setActivePlayer(std::uint8_t client, std::uint8_t player)
{
    //this MUST match how the ball playerID is calculated (and actually should be a single func somewhere)
    m_activePlayer = ((client * ConstVal::MaxPlayers) + player) + 1;
}