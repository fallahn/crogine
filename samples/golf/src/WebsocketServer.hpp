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

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>
#include <string>

void websocketTest();

struct SharedStateData;
class WebSock
{
public:

    static bool start(std::int32_t port, std::size_t maxConnections = 4);
    static void stop();

    //send to all connected clients
    static void broadcastPacket(const std::vector<std::byte>& data);
    
    template <typename T>
    static void broadcastPacket(std::uint8_t packetID, const T& data)
    {
        std::vector<std::byte> p(sizeof(data) + 1);
        p[0] = static_cast<std::byte>(packetID);
        std::memcpy(&p[1], &data, sizeof(data));
        broadcastPacket(p);
    }

    static void broadcastPlayers(const SharedStateData&);

    static std::string getStatus();

private:

};