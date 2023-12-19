/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

#include "ServerState.hpp"

#include <array>

namespace sv
{
    class LobbyState final : public State
    {
    public:
        explicit LobbyState(SharedData&);

        void handleMessage(const cro::Message&) override;
        void netEvent(const net::NetEvent&) override;
        std::int32_t process(float) override;

        std::int32_t stateID() const override { return StateID::Lobby; }

    private:
        std::int32_t m_returnValue;
        SharedData& m_sharedData;

        std::array<bool, ConstVal::MaxClients> m_readyState = {};

        void insertPlayerInfo(const net::NetEvent&);
        void broadcastRules();
        void doServerCommand(const net::NetEvent&);
    };
}
