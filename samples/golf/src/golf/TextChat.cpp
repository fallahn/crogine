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

#include <cstring>

namespace
{
    const cro::Time ResendTime = cro::seconds(0.5f);
}

TextChat::TextChat(cro::Scene& s, SharedStateData& sd)
    : m_scene   (s),
    m_sharedData(sd),
    m_visible   (false)
{
    registerWindow([&]() 
        {
            if (m_visible)
            {
                ImGui::SetNextWindowSize({ 400.f, 200.f });
                if (ImGui::Begin("Chat Window", &m_visible))
                {
                    const float reserveHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
                    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -reserveHeight), false);

                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
                    for (const auto& [str, colour] : m_displayBuffer)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, colour);
                        ImGui::TextWrapped("%s", str.toUtf8().c_str());
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    {
                        ImGui::SetScrollHereY(1.0f);
                    }
                    ImGui::PopStyleVar();
                    ImGui::EndChild();
                    ImGui::Separator();

                    bool focusInput = false;
                    if (ImGui::InputText("##ip", &m_inputBuffer, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        sendTextChat();
                        focusInput = true;
                    }
                    ImGui::SetItemDefaultFocus();
                    
                    ImGui::SameLine();
                    if (ImGui::Button("Send"))
                    {
                        sendTextChat();
                        focusInput = true;
                    }
                    if (focusInput)
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                    }
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
    auto outStr = m_sharedData.connectionData[msg.client].playerData[0].name;

    auto end = std::find(msg.messageData.begin(), msg.messageData.end(), char(0));
    auto msgText = cro::String::fromUtf8(msg.messageData.begin(), end);

    //process any emotes such as /me and choose colour
    if (auto p = msgText.find("/me"); p != cro::String::InvalidPos
        && msgText.size() > 4)
    {
        outStr += " " + msgText.substr(p + 4);
        m_displayBuffer.emplace_back(outStr, ImVec4(1.f, 1.f, 0.f, 1.f));
    }
    else
    {
        outStr += ": " + msgText;
        m_displayBuffer.emplace_back(outStr, ImVec4(1.f, 1.f, 1.f, 1.f));
    }

    if (m_displayBuffer.size() > MaxLines)
    {
        m_displayBuffer.pop_front();
    }

    
    //TODO create an entity to temporarily show the message on screen
}


//private
void TextChat::sendTextChat()
{
    if (!m_inputBuffer.empty()
        && m_limitClock.elapsed() > ResendTime)
    {
        TextMessage msg;
        msg.client = m_sharedData.clientConnection.connectionID;

        auto len = std::min(m_inputBuffer.size(), TextMessage::MaxBytes);
        std::memcpy(msg.messageData.data(), m_inputBuffer.data(), len);

        m_inputBuffer.clear();

        m_sharedData.clientConnection.netClient.sendPacket(PacketID::ChatMessage, msg, net::NetFlag::Reliable, ConstVal::NetChannelStrings);

        m_limitClock.restart();
    }
}