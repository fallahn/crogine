/*-----------------------------------------------------------------------

Matt Marchant - 2022
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

#include "../PacketIDs.hpp"
#include "ServerBilliardsState.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>

#include <crogine/util/Random.hpp>
#include <crogine/util/Network.hpp>

using namespace sv;

namespace
{
    const cro::Time TurnTime = cro::seconds(20.f);
}

BilliardsState::BilliardsState(SharedData& sd)
    : m_returnValue     (StateID::Billiards),
    m_sharedData        (sd),
    m_scene             (sd.messageBus, 512),
    m_tableDataValid    (false),
    m_gameStarted       (false),
    m_allMapsLoaded     (false)
{
    if (m_tableDataValid = validateData(); m_tableDataValid)
    {
        initScene();
        buildWorld();
    }
    LOG("Entered Billiards state", cro::Logger::Type::Info);
}

//public
void BilliardsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

void BilliardsState::netEvent(const cro::NetEvent& evt)
{
    if (evt.type == cro::NetEvent::PacketReceived)
    {
        switch (evt.packet.getID())
        {
        default: break;
        case PacketID::ClientReady:
            if (!m_sharedData.clients[evt.packet.as<std::uint8_t>()].ready)
            {
                sendInitialGameState(evt.packet.as<std::uint8_t>());
            }
            break;
        /*case PacketID::InputUpdate:
            handlePlayerInput(evt.packet);
            break;*/
        case PacketID::ServerCommand:
            if (evt.peer.getID() == m_sharedData.hostID)
            {
                doServerCommand(evt);
            }
            break;
        case PacketID::TransitionComplete:
        {
            auto clientID = evt.packet.as<std::uint8_t>();
            m_sharedData.clients[clientID].mapLoaded = true;
        }
        }
    }
}

void BilliardsState::netBroadcast()
{
    auto timestamp = m_serverTime.elapsed().asMilliseconds();

    auto& balls = m_scene.getSystem<BilliardsSystem>()->getEntities();
    for (auto entity : balls)
    {
        auto& ball = entity.getComponent<BilliardBall>();
        if (ball.hadUpdate)
        {
            //TODO we can probably send a smaller struct here
            //but it probably doesn't matter
            ActorInfo info;
            info.position = entity.getComponent<cro::Transform>().getPosition();
            info.rotation = cro::Util::Net::compressQuat(entity.getComponent<cro::Transform>().getRotation());
            info.serverID = entity.getIndex();
            info.timestamp = timestamp;

            m_sharedData.host.broadcastPacket(PacketID::ActorUpdate, info, cro::NetFlag::Unreliable);

            ball.hadUpdate = false;
        }
    }
}

std::int32_t BilliardsState::process(float dt)
{
    if (m_gameStarted)
    {
        if (m_turnTimer.elapsed() < TurnTime)
        {
            //AFK'd
        }

        //we have to keep checking this as a client might
        //disconnect mid-transition and the final 'complete'
        //packet will never arrive.
        if (!m_allMapsLoaded)
        {
            bool allReady = true;
            for (const auto& client : m_sharedData.clients)
            {
                if (client.connected && !client.mapLoaded)
                {
                    allReady = false;
                }
            }
            m_allMapsLoaded = allReady;
        }
    }

    m_scene.simulate(dt);
    return m_returnValue;
}

//private
bool BilliardsState::validateData()
{
    auto path = "assets/golf/tables/" + m_sharedData.mapDir + ".table";
    if (m_tableData.loadFromFile(path))
    {
        if (!cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + m_tableData.collisionModel))
        {
            return false;
        }
        //TODO validate ruleset property
        return true;
    }
    return false;
}

void BilliardsState::initScene()
{
    auto& mb = m_sharedData.messageBus;
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<BilliardsSystem>(mb);

    //TODO add director based on rule set
}

void BilliardsState::buildWorld()
{
    m_scene.getSystem<BilliardsSystem>()->initTable(m_tableData);
}

void BilliardsState::sendInitialGameState(std::uint8_t clientID)
{
    //only send data the first time
    if (!m_sharedData.clients[clientID].ready)
    {
        if (!m_tableDataValid)
        {
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ServerError, static_cast<std::uint8_t>(MessageType::MapNotFound), cro::NetFlag::Reliable);
            return;
        }

        //send all the ball positions
        /*auto timestamp = m_serverTime.elapsed().asMilliseconds();
        for (const auto& player : m_playerInfo)
        {
            auto ball = player.ballEntity;

            ActorInfo info;
            info.serverID = static_cast<std::uint32_t>(ball.getIndex());
            info.position = ball.getComponent<cro::Transform>().getPosition();
            info.clientID = player.client;
            info.playerID = player.player;
            info.timestamp = timestamp;
            m_sharedData.host.sendPacket(m_sharedData.clients[clientID].peer, PacketID::ActorSpawn, info, cro::NetFlag::Reliable);
        }*/
    }

    m_sharedData.clients[clientID].ready = true;

    bool allReady = true;
    for (const auto& client : m_sharedData.clients)
    {
        if (client.connected && !client.ready)
        {
            allReady = false;
        }
    }

    if (allReady && !m_gameStarted)
    {
        //a guard to make sure this is sent just once
        m_gameStarted = true;

        //send command to set clients to first player
        //this also tells the client to stop requesting state

        //create an ent which waits for the clients to
        //finish loading
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            if (m_allMapsLoaded)
            {
                setNextPlayer();
                e.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(e);
            }

            //make sure to keep resetting this to prevent unfairly
            //truncating the next player's turn
            m_turnTimer.restart();
        };
    }
}

void BilliardsState::doServerCommand(const cro::NetEvent& evt)
{
#ifdef CRO_DEBUG_
    switch (evt.packet.as<std::uint8_t>())
    {
    default: break;
    case ServerCommand::SpawnBall:
        addBall({ cro::Util::Random::value(-0.1f, 0.1f), 0.5f, cro::Util::Random::value(-0.1f, 0.1f) }, cro::Util::Random::value(0, 15));
        break;
    case ServerCommand::StrikeBall:
        m_scene.getSystem<BilliardsSystem>()->applyImpulse();
        break;
    }
#endif
}

void BilliardsState::setNextPlayer()
{
    //TODO update scores

    //broadcast PacketID::SetPlayer
    //TODO include relevant player ID
    m_sharedData.host.broadcastPacket(PacketID::SetPlayer, std::uint8_t(0), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);

    m_turnTimer.restart();
}

void BilliardsState::addBall(glm::vec3 position, std::uint8_t id)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<BilliardBall>().id = id;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, position, id](cro::Entity e, float)
    {
        if (e.getComponent<cro::Transform>().getPosition().y < -1.f)
        {
            m_sharedData.host.broadcastPacket(PacketID::EntityRemoved, e.getIndex(), cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
            m_scene.destroyEntity(e);

            addBall(position, id);
        }
    };

    //notify client
    ActorInfo info;
    info.serverID = entity.getIndex();
    info.position = entity.getComponent<cro::Transform>().getPosition();
    info.rotation = cro::Util::Net::compressQuat(entity.getComponent<cro::Transform>().getRotation());
    info.state = entity.getComponent<BilliardBall>().id;
    info.timestamp = m_serverTime.elapsed().asMilliseconds();

    m_sharedData.host.broadcastPacket(PacketID::ActorSpawn, info, cro::NetFlag::Reliable, ConstVal::NetChannelReliable);
}