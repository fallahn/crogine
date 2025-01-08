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

//these must always be included first else we get
//a double include for winsock.h on windows...
#include "sockets/IXNetSystem.h"
#include "sockets/IXWebSocketServer.h"

#include "WebsocketServer.hpp"
#include "golf/MessageIDs.hpp"
#include "golf/PacketIDs.hpp"
#include "golf/GameConsts.hpp"
#include "golf/SharedStateData.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/Log.hpp>
#include <crogine/core/String.hpp>

#include <memory>

namespace
{
    //this should persist between instances
    bool initDone = false;
    struct Socket final
    {
        std::unique_ptr<ix::WebSocketServer> server;

        Socket(std::int32_t port, std::size_t maxConnections)
        {
            if (!initDone)
            {
                ix::initNetSystem();
                initDone = true;
            }

            server = std::make_unique<ix::WebSocketServer>(port, "0.0.0.0", ix::SocketServer::kDefaultTcpBacklog, maxConnections);
            server->setOnClientMessageCallback([]
            (std::shared_ptr<ix::ConnectionState> state, ix::WebSocket& socket, const ix::WebSocketMessagePtr& msg)
                {
                    switch (msg->type)
                    {
                    default: break;
                    case ix::WebSocketMessageType::Open:
                        LogI << "Websocket: " << state->getId() << " connected from " << state->getRemoteIp() << std::endl;
                        {
                            //raise a message so we can broadcast relevant game info
                            auto* msg = cro::App::getInstance().getMessageBus().post<WebSocketEvent>(cl::MessageID::WebSocketMessage);
                            msg->type = WebSocketEvent::Connected;
                        }
                        break;
                    case ix::WebSocketMessageType::Message:
                    {
                        /*if (!msg->binary)
                        {
                            LogI << msg->str << std::endl;
                        }
                        else
                        {
                            LogI << "RX binary message" << std::endl;
                        }
                        socket.send("hello");*/
                    }
                    break;
                    case ix::WebSocketMessageType::Close:
                        LogI << "Websocket: " << state->getId() << " has disconnected" << std::endl;
                        {
                            //raise a message so we can broadcast relevant game info
                            auto* msg = cro::App::getInstance().getMessageBus().post<WebSocketEvent>(cl::MessageID::WebSocketMessage);
                            msg->type = WebSocketEvent::Disconnected;
                        }
                        break;
                    }
                });

            const auto [result, reason] = server->listen();
            if (result)
            {
                server->start();
                LogI << "Websocket: listening on " << port << std::endl;
            }
            else
            {
                LogE << "Failed to start websocket server: " << reason << std::endl;
                server.reset();
            }
        }

        ~Socket()
        {
            //hmm, not sure this is necessary
        }
    };

    std::unique_ptr<Socket> server;
}

bool WebSock::start(std::int32_t port, std::size_t maxConnections)
{
    server = std::make_unique<Socket>(port, maxConnections);
    return server->server != nullptr;
}

void WebSock::stop()
{
    if (server->server)
    {
        server->server->stop();
        server.reset();
        LogI << "Websocket: Stopped listening" << std::endl;
    }
}

void WebSock::broadcastPacket(const std::vector<std::byte>& packet)
{
    if (server->server)
    {
        auto sockets = server->server->getClients();
        for (auto& socket : sockets)
        {
            socket->sendBinary(ix::IXWebSocketSendData(reinterpret_cast<const char*>(packet.data()), packet.size()));
        }
    }
}

void WebSock::broadcastPlayers(const SharedStateData& sd)
{
    if (sd.clientConnection.connected)
    {
        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
        {
            for (auto j = 0u; j < sd.connectionData[i].playerCount; ++j)
            {
                const auto utf = sd.connectionData[i].playerData[j].name.toUtf8();

                std::vector<std::byte> data(3 + utf.length());
                data[0] = std::byte(PacketID::PlayerInfo);
                data[1] = std::byte(i);
                data[2] = std::byte(j);
                std::memcpy(&data[3], utf.data(), utf.length());

                WebSock::broadcastPacket(data);
            }
        }
    }
}


//--------------------//
void websocketTest()
{
    ix::initNetSystem();

    ix::WebSocketServer server(8080, "0.0.0.0");

    server.setOnClientMessageCallback([]
    (std::shared_ptr<ix::ConnectionState> state, ix::WebSocket& socket, const ix::WebSocketMessagePtr& msg)
        {
            LogI << "Message from " << state->getRemoteIp() << std::endl;

            switch (msg->type)
            {
            default: break;
            case ix::WebSocketMessageType::Open:
                LogI << state->getId() << " connected" << std::endl;

                //TODO store socket pointers mapped to... stateID?

                socket.send("OK");
                break;
            case ix::WebSocketMessageType::Message:
            {
                if (!msg->binary)
                {
                    LogI << msg->str << std::endl;
                }
                else
                {
                    LogI << "RX binary message" << std::endl;
                }
                socket.send("hello");
            }
            break;
            case ix::WebSocketMessageType::Close:
                LogI << state->getId() << " has disconnected" << std::endl;
                break;
            }
        });


    auto res = server.listen();
    if (!res.first)
    {
        LogI << res.second << std::endl;
        ix::uninitNetSystem();
        return;
    }

    server.start();
    server.wait();


    ix::uninitNetSystem();
}