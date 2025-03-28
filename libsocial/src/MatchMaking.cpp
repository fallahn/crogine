/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2024
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

#include "MatchMaking.hpp"

#include <crogine/core/MessageBus.hpp>

MatchMaking::MatchMaking(cro::MessageBus& mb, bool)
    : m_messageBus(mb)
{

}

//public
void MatchMaking::createGame(std::int32_t, std::int32_t)
{
    auto* msg = m_messageBus.post<Message>(MessageID);
    msg->type = Message::GameCreated;
}

void MatchMaking::joinGame(std::uint64_t,  std::int32_t gameType)
{
    auto* msg = m_messageBus.post<Message>(MessageID);
    msg->type = Message::LobbyJoined;
    msg->gameType = gameType;
}