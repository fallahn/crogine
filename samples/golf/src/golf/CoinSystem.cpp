/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "CoinSystem.hpp"
#include "PacketIDs.hpp"
#include "server/ServerState.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/detail/glm/gtx/norm.hpp>

namespace
{
    constexpr glm::vec2 BucketCentre = glm::vec2(20.f, 23.f);
    constexpr float BucketRadius = 32.f; //crude broad phase
    constexpr float BucketRadSqr = BucketRadius * BucketRadius;
    constexpr cro::FloatRect BucketLeft = {1.f, 0.f, 6.f, 46.f};
    constexpr cro::FloatRect BucketRight = {35.f, 0.f, 6.f, 46.f};
    constexpr cro::FloatRect CoinRect = {-2.f, -2.f, 4.f, 4.f};

    constexpr float Dampening = 0.05f;

    glm::vec3 getManifold(const cro::FloatRect& overlap, const glm::vec2 collisionNormal)
    {
        //the collision normal is stored in x and y, with the penetration in z
        glm::vec3 manifold = { 0.f, 1.f, 0.f };

        if (overlap.width < overlap.height)
        {
            manifold.x = (collisionNormal.x < 0) ? 1.f : -1.f;
            manifold.z = overlap.width;
        }
        else
        {
            manifold.y = (collisionNormal.y < 0) ? 1.f : -1.f;
            manifold.z = overlap.height;
        }
        //LogI << manifold << std::endl;
        return manifold;
    }
}

CoinSystem::CoinSystem(cro::MessageBus& mb, sv::SharedData& sd)
    : cro::System(mb, typeid(CoinSystem)),
    m_sharedData(sd)
{
    requireComponent<cro::Transform>();
    requireComponent<Coin>();
}

//public
void CoinSystem::handleMessage(const cro::Message&) {}

void CoinSystem::process(float dt)
{
    if (m_bucketEnt.isValid())
    {
        const auto bucketLeft = BucketLeft.transform(m_bucketEnt.getComponent<cro::Transform>().getLocalTransform());
        const auto bucketRight = BucketRight.transform(m_bucketEnt.getComponent<cro::Transform>().getLocalTransform());

        const auto leftCentre = glm::vec2(bucketLeft.left + (bucketLeft.width / 2.f), bucketLeft.bottom + (bucketLeft.height / 2.f));
        const auto rightCentre = glm::vec2(bucketRight.left + (bucketRight.width / 2.f), bucketRight.bottom + (bucketRight.height / 2.f));

        const auto canCentre = glm::vec2(m_bucketEnt.getComponent<cro::Transform>().getPosition()) + BucketCentre;

        auto& entities = getEntities();
        for (auto entity : entities)
        {
            auto& coin = entity.getComponent<Coin>();
            auto& tx = entity.getComponent<cro::Transform>();

            glm::vec2 startPos = tx.getPosition();
            if (glm::length2(startPos - canCentre) < BucketRadSqr)
            {
                static constexpr std::int32_t StepCount = 4; //TODO we could try adjusting this based on velocity magnitude
                const auto step = (coin.velocity * dt) / static_cast<float>(StepCount);

                //collision with can
                for (auto i = 0; i < StepCount; ++i)
                {
                    const auto pos = glm::vec2(tx.getPosition()) + (step * static_cast<float>(i));

                    auto coinRect = CoinRect;
                    coinRect.left += pos.x;
                    coinRect.bottom += pos.y;

                    cro::FloatRect overlap;

                    auto testCollision = [&](const cro::FloatRect& target)
                        {
                            if (coinRect.intersects(target, overlap))
                            {
                                const auto normal = leftCentre - pos;
                                const auto manifold = getManifold(overlap, normal);

                                tx.setPosition(pos + (glm::vec2(manifold) * (manifold.z * 2.f)));
                                coin.velocity = glm::reflect(coin.velocity, glm::vec2(manifold));

                                coin.velocity *= Dampening;
                                return true;
                            }
                            return false;
                        };

                    if (testCollision(bucketLeft) || testCollision(bucketRight))
                    {
                        break;
                    }
                    else
                    {
                        tx.move(step);
                    }
                }
            }
            else
            {
                //we're far enough to just move
                tx.move(coin.velocity * dt);
            }

            coin.velocity += Coin::Gravity * dt;

            if (tx.getPosition().y < 0)
            {
                const auto id = static_cast<std::uint32_t>(entity.getIndex());
                getScene()->destroyEntity(entity);

                m_sharedData.host.broadcastPacket(PacketID::CoinRemove, id, net::NetFlag::Reliable);
            }
        }
    }
}

bool CoinSystem::canSpawn(std::uint64_t client)
{
    static constexpr std::int32_t MaxCoins = 5;

    if (m_coinCount.count(client) == 0)
    {
        return true;
    }

    return m_coinCount.at(client) < MaxCoins;
}

//private
void CoinSystem::onEntityAdded(cro::Entity e)
{
    auto client = e.getComponent<Coin>().owner;
    if (m_coinCount.count(client) == 0)
    {
        m_coinCount.insert(std::make_pair(client, 1));
    }
    else
    {
        m_coinCount.at(client)++;
    }
}

void CoinSystem::onEntityRemoved(cro::Entity e)
{
    //this should NEVER be missing from the counter...
    m_coinCount.at(e.getComponent<Coin>().owner)--;
}