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
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "../GolfGame.hpp"

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Easings.hpp>

#include <cstring>

namespace
{
    const cro::Time ResendTime = cro::seconds(1.5f);

    const std::array<std::string, 12u> ApplaudStrings =
    {
        "/me applauds",
        "/me nods in appreciation",
        "/me claps",
        "/me congratulates",
        "/me collapses in awe",
        "/me politely golf-claps",
        "/me does a double-take",
        "/me pats your back",
        "/me feels a bit star-struck",
        "/me ovates (which means applauding.)",
        "/me raises a glass of Fizz",
        "/me now knows what a true hero looks like"
    };

    const std::array<std::string, 17u> HappyStrings =
    {
        "/me is ecstatic",
        "/me grins from ear to ear",
        "/me couldn't be happier",
        "/me is incredibly pleased",
        "/me skips away in delight",
        "/me moonwalks",
        "/me twerks awkwardly",
        "/me dabs",
        "/me sploots with joy",
        "/me is over the moon",
        "/me cartwheels to the clubhouse and back",
        "/me 's rubbing intensifies",
        "/me high fives their caddy",
        "/me is on top of the world",
        "/me pirouettes jubilantly",
        "/me feels like a dog with two tails",
        "/me lets out a squeak"
    };

    const std::array<std::string, 11u> LaughStrings =
    {
        "/me laughs like a clogged drain",
        "/me giggles into their sleeve",
        "/me laughs out loud",
        "/me rolls on the floor with laughter",
        "/me can't contain their glee",
        "/me belly laughs",
        "/me 's sides have split from laughing",
        "/me can't contain themselves",
        "/me howls with laughter",
        "/me goes into hysterics",
        "/me cachinnates (which means laughing loudly)"
    };

    const std::array<std::string, 17u> AngerStrings =
    {
        "/me cow-fudged that one up",
        "/me rages",
        "/me throws their club into the lake",
        "/me stamps their foot",
        "/me throws a tantrum",
        "/me accidentally swears on a live public feed",
        "/me throws a hissy-fit",
        "/me is appalled",
        "/me clenches their teeth",
        "/me wails despondently",
        "/me shakes a fist",
        "/me cries like a baby",
        "/me 's blood boils",
        "/me blows a fuse",
        "/me blames the dev",
        "/me explodes in a shower of fleshy chunks",
        "/me should've known better"
    };
}

TextChat::TextChat(cro::Scene& s, SharedStateData& sd)
    : m_scene               (s),
    m_sharedData            (sd),
    m_visible               (false),
    m_scrollToEnd           (false),
    m_focusInput            (false),
    m_screenChatIndex       (0),
    m_screenChatActiveCount (0)
{
    registerWindow([&]() 
        {
            if (m_visible)
            {
                //used to detect if we had any input
                auto buffSize = m_inputBuffer.size();

                ImGui::SetNextWindowSize({ 600.f, 280.f });
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

                    if (ImGui::InputText("##ip", &m_inputBuffer, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        sendTextChat();
                        m_focusInput = true;
                    }
                    
                    ImGui::SetItemDefaultFocus();
                    if (m_focusInput
                        || ImGui::IsItemHovered()
                        || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                            && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                    {
                        ImGui::SetKeyboardFocusHere(-1);
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Send"))
                    {
                        sendTextChat();
                        //m_focusInput = true;
                    }

                    m_focusInput = false;
                }
                ImGui::End();

                //notification packets to tell clients if someone is typing
                if (m_inputBuffer.size() != buffSize)
                {
                    m_inputBuffer.empty() ? endChat() : beginChat();
                }
                else if (m_chatTimer.elapsed() > cro::seconds(2.f))
                {
                    endChat();
                }
            }
        });
}

TextChat::~TextChat()
{
#ifdef USE_GNS
    Social::hideChatInput();
#endif
}

//public
void TextChat::handleMessage(const cro::Message& msg)
{
    if (msg.id == cl::MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::SubmitOSK)
        {
            sendBufferedString(); //also ends the chat notification
        }
        else if (data.type == SystemEvent::CancelOSK)
        {
            endChat();
        }
    }
}

void TextChat::handlePacket(const net::NetEvent::Packet& pkt)
{
    const auto msg = pkt.as<TextMessage>();
    //only one person can type on a connected computer anyway
    //so we'll always assume it's player 0
    auto outStr = m_sharedData.connectionData[msg.client].playerData[0].name;

    //we have to manually truncate this else we get a lot of white space
    auto msgText = cro::String::fromUtf8(msg.messageData.begin(), std::find(msg.messageData.begin(), msg.messageData.end(), 0));

#ifdef USE_GNS
    Social::filterString(msgText);
#endif

    cro::Colour chatColour = TextNormalColour;

    //process any emotes such as /me and choose colour
    if (auto p = msgText.find("/me"); p == 0
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

    auto uiSize = glm::vec2(GolfGame::getActiveTarget()->getSize());
    uiSize /= getViewScale(uiSize);

    cro::Util::String::wordWrap(outStr, 70);

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Label);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, std::floor(uiSize.y - 18.f), 2.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(outStr);
    entity.getComponent<cro::Text>().setFillColour(chatColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(12.f);
    
    auto bounds = cro::Text::getLocalBounds(entity);
    bounds.width = std::round(bounds.width + 5.f);
    bounds.height = std::round(bounds.height + 4.f);

    static constexpr float BgAlpha = 0.4f;
    const cro::Colour c(0.f, 0.f, 0.f, BgAlpha);

    auto bgEnt = m_scene.createEntity();
    bgEnt.addComponent<cro::Transform>().setPosition({ -2.f, -2.f, -0.15f });
    bgEnt.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f, 0.1f });
    bgEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(bounds.left, bounds.bottom + bounds.height), c),
            cro::Vertex2D(glm::vec2(bounds.left, bounds.bottom), c),
            cro::Vertex2D(glm::vec2(bounds.left + bounds.width, bounds.bottom + bounds.height), c),
            cro::Vertex2D(glm::vec2(bounds.left + bounds.width, bounds.bottom), c)
        }
    );
    bgEnt.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    entity.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());
    
    auto currIdx = m_screenChatIndex;
    entity.getComponent<cro::Callback>().function =
        [&, bgEnt, bounds, currIdx](cro::Entity e, float dt) mutable
    {
        float& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        float alpha = cro::Util::Easing::easeInQuint(std::clamp(currTime * 4.f, 0.f, 1.f));
        auto c = e.getComponent<cro::Text>().getFillColour();
        c.setAlpha(alpha);
        e.getComponent<cro::Text>().setFillColour(c);

        c = e.getComponent<cro::Text>().getShadowColour();
        c.setAlpha(alpha);
        e.getComponent<cro::Text>().setShadowColour(c);

        c = cro::Colour(0.f, 0.f, 0.f, BgAlpha * alpha);
        auto& verts = bgEnt.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour = c;
        }

        const auto offset = -bounds.height * (1.f - alpha);
        e.getComponent<cro::Transform>().setOrigin({ 0.f, offset, 0.f });

        //move the next ent down (so this happens recursivelyand we get a nice stack)
        auto nextEnt = (currIdx + 1) % m_screenChatBuffer.size();
        if (m_screenChatBuffer[nextEnt].isValid())
        {
            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.y -= bounds.height;
            pos.y -= offset;
            m_screenChatBuffer[nextEnt].getComponent<cro::Transform>().setPosition(pos);
        }

        //have to keep this active for depth sorting
        m_scene.getActiveCamera().getComponent<cro::Camera>().isStatic = false;
        m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;

        if (currTime < 0)
        {
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
            m_screenChatActiveCount--;

            //hmmmmm this only needs to be done on the menu
            //m_scene.getActiveCamera().getComponent<cro::Camera>().isStatic = true;
        }
    };
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_screenChatActiveCount++;

    //if we do have an influx of messages force the oldest off screen
    if (m_screenChatActiveCount > m_screenChatBuffer.size() / 2)
    {
        for (auto e : m_screenChatBuffer)
        {
            if (e.isValid())
            {
                e.getComponent<cro::Transform>().move({ 0.f, 16.f });
                if (e.getComponent<cro::Transform>().getPosition().y > uiSize.y)
                {
                    m_scene.destroyEntity(e);
                    m_screenChatActiveCount--;
                }
            }
        }
    }
    m_screenChatBuffer[m_screenChatIndex] = entity;
    m_screenChatIndex = (m_screenChatIndex + 1) % m_screenChatBuffer.size();

    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
}

void TextChat::toggleWindow(bool showOSK)
{
#ifdef USE_GNS
    if (Social::isSteamdeck())
    {
        beginChat();

        const auto cb =
            [&](bool submitted, const char* buffer)
            {
                if (submitted)
                {
                    m_inputBuffer = buffer;
                    sendTextChat();
                }
                endChat();
            };

        //this only shows the overlay as Steam takes care of dismissing it
        Social::showChatInput(cb);
    }
    else
#endif
    {
        if (showOSK)
        {
            auto* msg = cro::App::postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::RequestOSK;
            msg->data = 1; //use OSK buffer
        }
        else
        {
            m_visible = (!m_visible && !Social::isSteamdeck());
            m_focusInput = m_visible;
        }
    }
}

void TextChat::quickEmote(std::int32_t emote)
{
    //preserve any partially complete input
    auto oldStr = m_inputBuffer;

    switch (emote)
    {
    default: return;
    case Angry:
        m_inputBuffer = AngerStrings[cro::Util::Random::value(0u, AngerStrings.size() - 1)];
        break;
    case Happy:
        m_inputBuffer = HappyStrings[cro::Util::Random::value(0u, HappyStrings.size() - 1)];
        break;
    case Laughing:
        m_inputBuffer = LaughStrings[cro::Util::Random::value(0u, LaughStrings.size() - 1)];
        break;
    case Applaud:
        m_inputBuffer = ApplaudStrings[cro::Util::Random::value(0u, ApplaudStrings.size() - 1)];
        break;
    }

    sendTextChat();

    m_inputBuffer = oldStr;
}

void TextChat::sendBufferedString()
{
    if (!m_sharedData.OSKBuffer.empty())
    {
        auto oldString = m_inputBuffer;

        auto str = m_sharedData.OSKBuffer.toUtf8();
        m_inputBuffer = std::string(str.begin(), str.end());

        sendTextChat();
        m_inputBuffer = oldString;

        m_sharedData.useOSKBuffer = false;
        m_sharedData.OSKBuffer.clear();
    }
}


//private
void TextChat::beginChat()
{
    m_chatTimer.restart();

    Activity a;
    a.client = m_sharedData.clientConnection.connectionID;
    a.type = Activity::PlayerChatStart;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

void TextChat::endChat()
{
    Activity a;
    a.client = m_sharedData.clientConnection.connectionID;
    a.type = Activity::PlayerChatEnd;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::Activity, a, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
}

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

        m_visible = false;

        endChat();
    }
}