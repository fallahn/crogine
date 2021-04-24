/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#pragma once

#include "InputParser.hpp"
#include "PlayerState.hpp"

#include <crogine/ecs/System.hpp>
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>

#include <array>
#include <memory>

struct PlayerUpdate;
struct Player final
{
    cro::Entity avatar;

    //if we assume (and we should) 60 updates per second
    //this will buffer 2 seconds worth of input
    static constexpr std::size_t HistorySize = 120;
    std::array<Input, HistorySize> inputStack = {};
    std::size_t nextFreeInput = 0; //POST incremented after adding new input to history
    std::size_t lastUpdatedInput = HistorySize - 1; //index of the last parsed input

    std::uint16_t previousInputFlags = 0;

    std::uint8_t collisionLayer = 0; //index into the collision data depending on which layer we're on
    struct State final
    {
        enum
        {
            Walking, Falling,
            Teleport, Dead,
            Reset,

            Count
        };
    };
    std::uint8_t state = State::Falling; //this is used to index into the update functions in the player system
    
    //these are a bit confusing: if the foot flag is set then
    //the player's foot has contact with someting. HOWEVER any
    //of the other flags indicated that the player body is touching this type.
    std::uint8_t collisionFlags = 0;

    glm::vec3 velocity = glm::vec3(0.f);
    
    enum Direction
    {
        Left = -1, Right = 1
    }direction = Right;

    std::uint8_t id = 4; //this should be the same as the ActorID for this entity
    std::uint8_t connectionID = 4;
    glm::vec3 spawnPosition = glm::vec3(0.f);

    bool carrying = false;
    bool waitResync = false; //waiting to resync player with server
    bool local = false; //server instance or client

    static constexpr float PuntCoolDown = 3.f; //seconds
    float puntLevel = PuntCoolDown;
    float puntLevelLinear = PuntCoolDown;

    float resetTime = 1.f;
};

struct PlayerAvatar final
{
    float rotation = 0.f;
    cro::Entity crateEnt;
    cro::Entity holoEnt;
};

class PlayerSystem final : public cro::System
{
public:
    explicit PlayerSystem(cro::MessageBus&);

    void handleMessage(const cro::Message&) override;

    void process(float) override;

    void reconcile(cro::Entity, const PlayerUpdate&);

private:
    std::array<std::unique_ptr<PlayerState>, Player::State::Count> m_playerStates;
    void processInput(cro::Entity);

    void processCollision(cro::Entity, std::uint32_t);
};
