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
#include "MenuConsts.hpp"

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <cstring>

namespace
{
    const cro::Time ResendTime = cro::seconds(0.5f);
}

TextChat::TextChat(cro::Scene& s, SharedStateData& sd)
    : m_scene           (s),
    m_sharedData        (sd),
    m_visible           (false),
    m_scrollToEnd       (false),
    m_screenChatIndex   (0)
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

                    if (m_scrollToEnd ||
                        ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                    {
                        ImGui::SetScrollHereY(1.0f);
                        m_scrollToEnd = false;
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

    cro::Colour chatColour = TextNormalColour;

    //process any emotes such as /me and choose colour
    if (auto p = msgText.find("/me"); p != cro::String::InvalidPos
        && msgText.size() > 4)
    {
        chatColour = TextGoldColour;
        outStr += " " + msgText.substr(p + 4);
    }
    else
    {
        outStr += ": " + msgText;
    }
    m_displayBuffer.emplace_back(outStr, ImVec4(chatColour));

    if (m_displayBuffer.size() > MaxLines)
    {
        m_displayBuffer.pop_front();
    }

    
    //create an entity to temporarily show the message on screen
    if (m_screenChatBuffer[m_screenChatIndex].isValid())
    {
        m_scene.destroyEntity(m_screenChatBuffer[m_screenChatIndex]);
    }
    //TODO we need to attach this to some root node?
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Label);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 30.f, 1.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(outStr);
    entity.getComponent<cro::Text>().setFillColour(chatColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(5.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        float& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        float alpha = std::clamp(currTime, 0.f, 1.f);
        auto c = e.getComponent<cro::Text>().getFillColour();
        c.setAlpha(alpha);
        e.getComponent<cro::Text>().setFillColour(c);

        c = e.getComponent<cro::Text>().getShadowColour();
        c.setAlpha(alpha);
        e.getComponent<cro::Text>().setShadowColour(c);

        if (currTime < 0)
        {
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_screenChatBuffer[m_screenChatIndex] = entity;
    m_screenChatIndex = (m_screenChatIndex + 1) % 2;
    if (m_screenChatBuffer[m_screenChatIndex].isValid())
    {
        m_screenChatBuffer[m_screenChatIndex].getComponent<cro::Transform>().move({ 0.f, 12.f });
    }
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
        m_scrollToEnd = true;
    }
}