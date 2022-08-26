/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <cstdint>
#include <limits>
#include <vector>

namespace cro
{
    class MessageBus;
}

class MatchMaking final
{
public:
    explicit MatchMaking(cro::MessageBus&);
    ~MatchMaking() = default;

    MatchMaking(const MatchMaking&) = delete;
    MatchMaking(MatchMaking&&) = delete;
    MatchMaking& operator = (const MatchMaking&) = delete;
    MatchMaking& operator = (MatchMaking&&) = delete;


    void createGame(std::int32_t gameType = 0);

    void joinGame(std::uint64_t lobbyID);

    void refreshLobbyList(std::int32_t gameType = 0) {}


    struct LobbyData final
    {
        std::uint64_t ID = 0;
        std::int32_t playerCount = 0;
        std::int32_t clientCount = 0;
    };
    const std::vector<LobbyData>& getLobbies() const { static std::vector<LobbyData> d;  return d; }

    void leaveGame() {}

    //use the message bus to relay callback events
    static constexpr std::int32_t MessageID = std::numeric_limits<std::int32_t>::max() / 2;

    struct Message final
    {
        enum
        {
            GameCreated,
            GameCreateFailed,
            LobbyJoined,
            LobbyJoinFailed,
            LobbyListUpdated,
            Error
        }type = Error;
        std::uint64_t hostID = 0;
    };

private:
    cro::MessageBus& m_messageBus;
};