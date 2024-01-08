/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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
        bool m_mapDataValid;

        std::vector<HoleData> m_holeData;
        cro::Clock m_serverTime; //used in timestamping
        cro::Scene m_scene;

        float m_scoreboardTime; //how long to wait before setting next player active
        std::uint16_t m_scoreboardReadyFlags;

        //game rule stuff. TODO encapsulate somewhere
        bool m_gameStarted;
        bool m_eliminationStarted; //< allows playing X holes before elimination starts
        bool m_allMapsLoaded;
        bool m_skinsFinals;
        std::uint8_t m_currentHole;
        std::vector<PlayerStatus> m_playerInfo; //active players. Sorted by distance so the front position is active player
        std::uint8_t m_skinsPot;
        std::uint8_t m_currentBest; //current best score for hole, non-stroke games end if no-one can beat it
        std::uint8_t m_randomTargetCount;

        cro::Clock m_turnTimer;
        std::array<std::uint8_t, 2u> m_honour = { 0, 0 };

        void sendInitialGameState(std::uint8_t);
        void handlePlayerInput(const net::NetEvent::Packet&, bool predict);
        void checkReadyQuit(std::uint8_t);

        void setNextPlayer(bool newHole = false);
        void setNextHole();
        void skipCurrentTurn(std::uint8_t);

        bool validateMap();
        void initScene();
        void buildWorld();

        void handleRules(const struct GolfBallEvent&);
        bool summariseRules();

        void doServerCommand(const net::NetEvent&);
    };
}