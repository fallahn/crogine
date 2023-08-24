/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "TextChat.hpp"
#include "PacketIDs.hpp"

TextChat::TextChat(cro::Scene& s, SharedStateData& sd)
    : m_scene   (s),
    m_sharedData(sd),
    m_visible   (false)
{
    registerWindow([&]() 
        {
            if (m_visible)
            {
                ImGui::SetNextWindowSize({ 400.f, 100.f });
                if (ImGui::Begin("Chat Window", &m_visible))
                {


                }
                ImGui::End();
            }
        });
}

//public
void TextChat::handlePacket(const net::NetEvent::Packet& pkt)
{
    const auto msg = pkt.as<TextMessage>();
    //only one person can type on a connected computer anyway
    //so we'll always assume it's player 0
    const auto& name = m_sharedData.connectionData[msg.client].playerData[0].name;

    std::string textStr = msg.messageData.data();

    //TODO process any emotes such as /me and choose colour

    m_displayBuffer.emplace_back(textStr, ImVec4(1.f, 0.f, 0.f, 0.f));
    if (m_displayBuffer.size() > MaxLines)
    {
        m_displayBuffer.pop_front();
    }

    cro::String screenMsg = cro::String::fromUtf8(textStr.begin(), textStr.end());
    //TODO create an entity to temporarily show the message on screen
}


//private
void TextChat::sendTextChat()
{

}