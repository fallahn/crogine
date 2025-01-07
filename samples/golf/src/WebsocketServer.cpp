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

#include "WebsocketServer.hpp"

#include "sockets/IXNetSystem.h"
#include "sockets/IXWebSocketServer.h"

#include <iostream>

void websocketTest()
{
    ix::initNetSystem();

    ix::WebSocketServer server(8080, "0.0.0.0");

    server.setOnClientMessageCallback([]
    (std::shared_ptr<ix::ConnectionState> state, ix::WebSocket& socket, const ix::WebSocketMessagePtr& msg)
        {
            std::cout << "Message from " << state->getRemoteIp() << std::endl;

            switch (msg->type)
            {
            default: break;
            case ix::WebSocketMessageType::Open:
                std::cout << state->getId() << " connected" << std::endl;

                //TODO store socket pointers mapped to... stateID?

                socket.send("OK");
                break;
            case ix::WebSocketMessageType::Message:
            {
                if (!msg->binary)
                {
                    std::cout << msg->str << std::endl;
                }
                else
                {
                    std::cout << "RX binary message" << std::endl;
                }
                socket.send("hello");
            }
            break;
            case ix::WebSocketMessageType::Close:
                std::cout << state->getId() << " has disconnected" << std::endl;
                break;
            }
        });


    auto res = server.listen();
    if (!res.first)
    {
        std::cout << res.second << std::endl;
        ix::uninitNetSystem();
        return;
    }

    server.start();
    server.wait();


    ix::uninitNetSystem();
}