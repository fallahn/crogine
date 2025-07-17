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

#include "../PacketIDs.hpp"
#include "../CommonConsts.hpp"
#include "../CoinSystem.hpp"
#include "../Networking.hpp"
#include "ServerLobbyState.hpp"
#include "ServerPacketData.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Network.hpp>

using namespace sv;

namespace
{
    constexpr float GameStartTime = 60.f * 5.f;
    constexpr float MinCanPos = 320.f;
    constexpr float MaxCanPos = 640.f;
}

//public
void LobbyState::netBroadcast()
{
    auto bucketEnt = m_gameScene.getSystem<CoinSystem>()->getBucketEnt();

    if (!bucketEnt.isValid()) return;

    const auto timestamp = m_serverTime.elapsed().asMilliseconds();
    
    const auto sendData = [&](cro::Entity e)
        {
            CanInfo info;
            info.serverID = static_cast<std::uint32_t>(e.getIndex());
            info.position = e.getComponent<cro::Transform>().getPosition();
            info.timestamp = timestamp;
            
            if (e.hasComponent<Coin>())
            {
                auto& coin = e.getComponent<Coin>();
                info.velocity = cro::Util::Net::compressVec3(glm::vec3(coin.velocity, 0.f), ConstVal::VelocityCompressionRange);
                info.collisionTerrain = coin.collisionStart ? 1 : ConstVal::NullValue;
                coin.collisionStart = false;
            }
            else
            {
                const auto vel = e.getComponent<cro::Callback>().getUserData<CanData>().velocity;
                info.velocity = cro::Util::Net::compressVec3(vel, ConstVal::VelocityCompressionRange);
            }
            m_sharedData.host.broadcastPacket(PacketID::CanUpdate, info, net::NetFlag::Unreliable);
        };
    sendData(bucketEnt);

    //broadcast coins
    const auto& entities = m_gameScene.getSystem<CoinSystem>()->getEntities();
    for (auto entity : entities)
    {
        sendData(entity);
    }
}

//private
void LobbyState::buildScene()
{
    auto& mb = m_sharedData.messageBus;

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<CoinSystem>(mb, m_sharedData);
    
    //counts the connected clients and spawns bucket after
    //5 minutes if more than one client
    if (!m_gameStarted) //hack just to stop this happening during debug
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(GameStartTime);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                std::int32_t count = 0;
                for (const auto& c : m_sharedData.clients)
                {
                    if (c.connected)
                    {
                        count++;
                    }
                }

                auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
                ct -= dt;
                if (count > 1)
                {
                    if (ct < 0)
                    {
                        //spawn can/start game
                        spawnCan();

                        e.getComponent<cro::Callback>().active = false;
                        m_gameScene.destroyEntity(e);
                    }
                }
                else
                {
                    ct = GameStartTime;
                }
            };
    }
}

void LobbyState::spawnCan()
{
    m_gameStarted = true;

    if (m_gameScene.getSystem<CoinSystem>()->getBucketEnt().isValid())
    {
        //already created
        return;
    }

    const auto getRandomPosition =
        []() 
        {
            const std::int32_t Offset = cro::Util::Random::value(0, 9);
            return MinCanPos + (((MaxCanPos - MinCanPos) / 10.f)* Offset);
        };

    const auto randPos = getRandomPosition();

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(randPos, 0.f, 0.f));
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<CanData>(randPos);
    entity.getComponent<cro::Callback>().function =
        [getRandomPosition](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<CanData>();
            switch (data.state)
            {
            default: break;
            case CanData::Pause:
                data.pauseTime -= dt;
                data.velocity = glm::vec3(0.f);
                if (data.pauseTime < 0)
                {
                    data.pauseTime += cro::Util::Random::value(0, 4);

                    data.target = getRandomPosition();
                    data.state = CanData::Move;
                }
                break;
            case CanData::Move:
            {
                auto pos = e.getComponent<cro::Transform>().getPosition().x;
                const auto movement = data.target - pos;
                data.velocity = { movement, 0.f, 0.f };

                e.getComponent<cro::Transform>().move({ movement * dt, 0.f, 0.f });

                if (std::abs(movement) < 3.f)
                {
                    data.state = CanData::Pause;
                }
            }
                break;
            }
        };


    m_gameScene.getSystem<CoinSystem>()->setBucketEnt(entity);

    ActorInfo spawnInfo;
    spawnInfo.playerID = 0; //we'll assume as we're in the lobby 0 is can, 1 is penny.
    spawnInfo.serverID = entity.getIndex();
    spawnInfo.position = entity.getComponent<cro::Transform>().getPosition();
    spawnInfo.lie = ConstVal::NullValue; //use this to ignore packets in game mode
    m_sharedData.host.broadcastPacket(PacketID::ActorSpawn, spawnInfo, net::NetFlag::Reliable);
}

void LobbyState::spawnCoin(float power, std::uint64_t client)
{
    if (m_gameStarted
        && m_gameScene.getSystem<CoinSystem>()->canSpawn(client))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<Coin>().velocity = glm::normalize(Coin::InitialVelocity) * power;
        entity.getComponent<Coin>().owner = client;

        //send actor spawn packet
        ActorInfo spawnInfo;
        spawnInfo.playerID = 1; //we'll assume as we're in the lobby 0 is can, 1 is penny.
        spawnInfo.serverID = entity.getIndex();
        spawnInfo.position = entity.getComponent<cro::Transform>().getPosition();
        spawnInfo.lie = ConstVal::NullValue; //use this to ignore packets in game mode
        m_sharedData.host.broadcastPacket(PacketID::ActorSpawn, spawnInfo, net::NetFlag::Reliable);
    }
}