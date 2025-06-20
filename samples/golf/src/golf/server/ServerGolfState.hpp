/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "../HoleData.hpp"
#include "ServerState.hpp"
#include "ServerPacketData.hpp"

#include <crogine/ecs/Scene.hpp>
#include <crogine/core/Clock.hpp>
#include <crogine/core/HiResTimer.hpp>

namespace sv
{
    class GolfState final : public State
    {
    public:
        explicit GolfState(SharedData&);

        void handleMessage(const cro::Message&) override;
        void netEvent(const net::NetEvent&) override;
        void netBroadcast() override;
        std::int32_t process(float) override;

        std::int32_t stateID() const override { return StateID::Golf; }

    private:
        std::int32_t m_returnValue;
        SharedData& m_sharedData;
        bool m_ntpPro; //hack so we can use existing rules
        bool m_mapDataValid;

        std::vector<HoleData> m_holeData;
        cro::Clock m_serverTime; //used in timestamping
        cro::Scene m_scene;

        float m_scoreboardTime; //how long to wait before setting next player active
        std::uint16_t m_scoreboardReadyFlags;

        //game rule stuff. TODO encapsulate somewhere
        bool m_gameStarted;
        //bool m_eliminationStarted; //< allows playing X holes before elimination starts
        bool m_allMapsLoaded;
        bool m_skinsFinals;
        std::uint8_t m_currentHole;

        std::uint8_t m_skinsPot;
        std::uint8_t m_currentBest; //current best score for hole, non-stroke games end if no-one can beat it
        std::uint8_t m_randomTargetCount;

        std::array<std::uint8_t, 2u> m_honour = { 0, 0 };

        struct Team final
        {
            std::int32_t currentPlayer = 0; //which of the two players is next
            std::array<std::array<std::uint8_t, 2u>, 2u> players = {};
        };
        std::vector<Team> m_teams;

        struct PlayerGroup final
        {
            cro::Clock turnTimer;
            bool warned = false;
            std::vector<PlayerStatus> playerInfo;
            std::vector<std::uint8_t> clientIDs; //list of clients which should be notified of this info
            std::uint8_t id = 0; //this group's ID, save keep measuring it
            std::uint8_t playerCount = 0; //total number of players in the group for all clients

            bool waitingForHole = false; //waiting for other groups to complete the hole
        };
        std::vector<PlayerGroup> m_playerInfo;


        //this is the group IDs indexed by client ID so we can look up a group for a given client
        std::array<std::int32_t, ConstVal::MaxClients> m_groupAssignments = {};

        void sendInitialGameState(std::uint8_t);
        void handlePlayerInput(const net::NetEvent::Packet&, bool predict);
        void checkReadyQuit(std::uint8_t);

        void setNextPlayer(std::int32_t groupID, bool newHole = false);
        void setNextHole();
        void skipCurrentTurn(std::uint8_t);
        void applyMulligan();

        bool validateMap();
        void initScene();
        void buildWorld();

        void handleRules(std::int32_t groupID, const struct GolfBallEvent&);
        bool summariseRules();

        void doServerCommand(const net::NetEvent&);
    };
}