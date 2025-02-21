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
        auto& entities = getEntities();
        for (auto entity : entities)
        {
            auto& coin = entity.getComponent<Coin>();
            auto& tx = entity.getComponent<cro::Transform>();

            tx.move(coin.velocity * dt);
            coin.velocity += Coin::Gravity * dt;

            //TODO collision with can

            if (tx.getPosition().y < 0)
            {
                const auto id = static_cast<std::uint32_t>(entity.getIndex());
                getScene()->destroyEntity(entity);

                m_sharedData.host.broadcastPacket(PacketID::CoinRemove, id, net::NetFlag::Reliable);
            }
        }
    }
}