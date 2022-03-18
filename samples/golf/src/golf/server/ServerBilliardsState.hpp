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

#pragma once

#include "../BilliardsSystem.hpp"

#include "ServerState.hpp"
#include "ServerPacketData.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/Scene.hpp>

namespace sv
{
    class BilliardsState final : public State
    {
    public:
        explicit BilliardsState(SharedData&);

        void handleMessage(const cro::Message&) override;
        void netEvent(const cro::NetEvent&) override;
        void netBroadcast() override;
        std::int32_t process(float) override;

        std::int32_t stateID() const override { return StateID::Billiards; }

    private:
        std::int32_t m_returnValue;
        SharedData& m_sharedData;
        cro::Scene m_scene;

        bool m_tableDataValid;
        bool m_gameStarted;
        bool m_allMapsLoaded;
        cro::Clock m_turnTimer;

        cro::Clock m_serverTime;

        TableData m_tableData;
        bool validateData();

        void initScene();
        void buildWorld();
        void sendInitialGameState(std::uint8_t);
        void doServerCommand(const cro::NetEvent&);

        void setNextPlayer();
        cro::Entity addBall(glm::vec3, std::uint8_t);
        void spawnBall(cro::Entity);
    };
}