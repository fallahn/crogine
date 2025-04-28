/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2025
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
#include "../Colordome-32.hpp"

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
    //this is static so the preference is remember between
    //instances
    bool closeOnSend = false;

    static void showToolTip(const char* desc)
    {
        if (ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    const cro::Time ResendTime = cro::seconds(1.5f);

    const std::array<std::string, 14u> ApplaudStrings =
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
        "/me now knows what a true hero looks like",
        "/me whoops and hollers from the crowd",
        "/me asks for a selfie"
    };

    const std::array<std::string, 18u> HappyStrings =
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
        "/me lets out a squeak",
        "/me oscillates with glee"
    };

    const std::array<std::string, 14u> LaughStrings =
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
        "/me cachinnates (which means laughing loudly)",
        "/me buries a snort",
        "/me brays like a mule",
        "/me cackles like a crone"
    };

    const std::array<std::string, 18u> AngerStrings =
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
        "/me should've known better",
        "/me meant to do that"
    };

    struct Emote final
    {
        explicit Emote(const std::vector<std::uint32_t>& cp)
            : codepoint(cp)
        {
            cro::String str;
            for (auto cpt : codepoint)
            {
                str += cpt;
            }
            auto utf = str.toUtf8();
            icon.resize(utf.size());
            std::memcpy(icon.data(), utf.data(), utf.size());
            icon.push_back(0);
        }
        std::vector<std::uint32_t> codepoint;
        std::vector<char> icon;
    };

    const std::array<Emote, 25u> Emotes =
    {
        Emote({0x1F600}), Emote({0x1F601}), Emote({0x1F602}), Emote({0x1F929}), Emote({0x1F61B}),
        Emote({0x1F914}), Emote({0x1F92D}), Emote({0x1F973}), Emote({0x1F60E}), Emote({0x1F644}),
        Emote({0x1F62C}), Emote({0x1F632}), Emote({0x1F633}), Emote({0x1F629}), Emote({0x1F624}),
        Emote({0x1F3C6}), Emote({0x1F947}), Emote({0x1F948}), Emote({0x1F949}), Emote({0x26F3}),
        Emote({0x2764}),  Emote({0x1F573}), Emote({0x1F4A5}), Emote({0x1F4A8}), Emote({0x1F4A4}),
    };
}

std::deque<TextChat::BufferLine> TextChat::m_displayBuffer;

TextChat::TextChat(cro::Scene& s, SharedStateData& sd)
    : m_scene               (s),
    m_sharedData            (sd),
    m_visible               (false),
    m_scrollToEnd           (false),
    m_focusInput            (false),
    m_drawWindow            (false),
    m_animDir               (0),
    m_animationProgress     (0.f),
    m_screenChatIndex       (0),
    m_screenChatActiveCount (0),
    m_showShortcuts         (false)
{
    if (sd.logChat)
    {
        initLog();
    }

    registerCommand("cl_use_tts", [&](const std::string& str)
        {
            if (str == "1" || str == "true")
            {
                m_sharedData.useTTS = true;
                cro::Console::print("use_tts set to " + str);
            }
            else if (str == "0" || str == "false")
            {
                m_sharedData.useTTS = false;
                cro::Console::print("use_tts set to " + str);
            }
            else
            {
                cro::Console::print("Usage: use_tts <0|1>");
            }
        });

    //use cro string to construct the utf8 strings
    cro::String str(std::uint32_t(0x1F44F));

    auto utf = str.toUtf8();

    m_buttonStrings.applaud.resize(utf.size());
    std::memcpy(m_buttonStrings.applaud.data(), utf.data(), utf.size());
    m_buttonStrings.applaud.push_back('#'); //these duplicate some button labels in the flyout *sigh*
    m_buttonStrings.applaud.push_back('#');
    m_buttonStrings.applaud.push_back('1');
    m_buttonStrings.applaud.push_back(0);

    str.clear();
    str = std::uint32_t(0x1F600);
    utf = str.toUtf8();
    m_buttonStrings.happy.resize(utf.size());
    std::memcpy(m_buttonStrings.happy.data(), utf.data(), utf.size());
    m_buttonStrings.popout = m_buttonStrings.happy;
    m_buttonStrings.popout.push_back('#');
    m_buttonStrings.popout.push_back('#');
    m_buttonStrings.popout.push_back('2');
    m_buttonStrings.popout.push_back(0);
    
    m_buttonStrings.happy.push_back('#');
    m_buttonStrings.happy.push_back('#');
    m_buttonStrings.happy.push_back('1');
    m_buttonStrings.happy.push_back(0);

    str.clear();
    str = std::uint32_t(0x1F923);
    utf = str.toUtf8();
    m_buttonStrings.laughing.resize(utf.size());
    std::memcpy(m_buttonStrings.laughing.data(), utf.data(), utf.size());
    m_buttonStrings.laughing.push_back('#');
    m_buttonStrings.laughing.push_back('#');
    m_buttonStrings.laughing.push_back('1');
    m_buttonStrings.laughing.push_back(0);

    str.clear();
    str = std::uint32_t(0x1F624);
    utf = str.toUtf8();
    m_buttonStrings.angry.resize(utf.size());
    std::memcpy(m_buttonStrings.angry.data(), utf.data(), utf.size());
    m_buttonStrings.angry.push_back('#');
    m_buttonStrings.angry.push_back('#');
    m_buttonStrings.angry.push_back('1');
    m_buttonStrings.angry.push_back(0);


    registerWindow([&]()
        {
            //ImGui::ShowDemoWindow();
            //if (m_visible)
            if (m_drawWindow)
            {
                //used to detect if we had any input
                auto buffSize = m_inputBuffer.size();

                const auto viewScale = getViewScale();
                const glm::vec2 OutputSize = glm::vec2(cro::App::getWindow().getSize());
                //const glm::vec2 WindowSize = glm::vec2(std::max(320.f, std::round((OutputSize.x / 3.f))), OutputSize.y);
                const glm::vec2 WindowSize = glm::vec2(OutputSize.x, std::max(240.f, std::round(OutputSize.y / 3.f)));

                ImGui::SetNextWindowSize({ WindowSize.x, WindowSize.y });
                //ImGui::SetNextWindowPos({ std::round(-WindowSize.x * cro::Util::Easing::easeInCubic(m_animationProgress)), 0.f });
                ImGui::SetNextWindowPos({ 0.f, std::round((OutputSize.y - WindowSize.y) + (WindowSize.y * cro::Util::Easing::easeInCubic(m_animationProgress))) });

                if (m_animationProgress == 1)
                {
                    m_drawWindow = false;
                    m_visible = false;
                }

                ImGui::GetFont()->Scale *= viewScale;
                ImGui::PushFont(ImGui::GetFont());

                m_sharedData.chatFonts.buttonLarge->Scale = 0.5f * viewScale;

                if (ImGui::Begin("Chat Window", nullptr, 
                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
                {
                    if (m_showShortcuts
                        && !Social::isSteamdeck())
                    {
                        ImGui::Text("Quick Emotes: ");
                        ImGui::SameLine();
                        ImGui::PushFont(m_sharedData.chatFonts.buttonLarge);
                        if (ImGui::Button(m_buttonStrings.applaud.data(), ImVec2(0.f, m_sharedData.chatFonts.buttonHeight * viewScale)))
                        {
                            quickEmote(TextChat::Applaud);
                            //m_visible = false;
                        }
                        ImGui::PopFont();
                        showToolTip("Applaud - Shortcut: Number 7");
                        ImGui::SameLine();
                        ImGui::PushFont(m_sharedData.chatFonts.buttonLarge);
                        if (ImGui::Button(m_buttonStrings.happy.data(), ImVec2(0.f, m_sharedData.chatFonts.buttonHeight * viewScale)))
                        {
                            quickEmote(TextChat::Happy);
                            //m_visible = false;
                        }
                        ImGui::PopFont();
                        showToolTip("Happy - Shortcut: Number 8");
                        ImGui::SameLine();
                        ImGui::PushFont(m_sharedData.chatFonts.buttonLarge);
                        if (ImGui::Button(m_buttonStrings.laughing.data(), ImVec2(0.f, m_sharedData.chatFonts.buttonHeight * viewScale)))
                        {
                            quickEmote(TextChat::Laughing);
                            //m_visible = false;
                        }
                        ImGui::PopFont();
                        showToolTip("Laughing - Shortcut: Number 9");
                        ImGui::SameLine();
                        ImGui::PushFont(m_sharedData.chatFonts.buttonLarge);
                        if (ImGui::Button(m_buttonStrings.angry.data(), ImVec2(0.f, m_sharedData.chatFonts.buttonHeight * viewScale)))
                        {
                            quickEmote(TextChat::Angry);
                            //m_visible = false;
                        }
                        ImGui::PopFont();
                        showToolTip("Grumpy - Shortcut: Number 0");
                        ImGui::Separator();
                    }
                    
                    std::int32_t flags = ImGuiWindowFlags_AlwaysUseWindowPadding;
                    if (Social::isSteamdeck())
                    {
                        flags |= ImGuiWindowFlags_NoScrollbar;
                    }

                    const float reserveHeight = (ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing());// *2.f;
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f,0.f,0.f,0.4f));
                    ImGui::BeginChild("ScrollingRegion", ImVec2(0, -reserveHeight), false, flags);
                    ImGui::PopStyleColor();

                    
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

                    if (!Social::isSteamdeck())
                    {
                        if (ImGui::InputText("##ip", &m_inputBuffer, ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            sendTextChat();
                            m_focusInput = true;
                        }

                        ImGui::SetItemDefaultFocus();
                        if (m_focusInput
                            || ImGui::IsItemHovered()
                            || (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow/*AndChildWindows*/)
                                && !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)))
                        {
                            ImGui::SetKeyboardFocusHere(-1);
                        }

                        ImGui::SameLine();
                        if (ImGui::Button(m_buttonStrings.popout.data()))
                        {
                            ImGui::OpenPopup("emote_popup");
                        }
                        m_focusInput = false;

                        ImGui::SameLine();
                        if (ImGui::Button("Send"))
                        {
                            sendTextChat();
                        }
                        ImGui::SameLine();
                        ImGui::Checkbox("Close On Send", &closeOnSend);

                        //emoji keypad popout
                        if (ImGui::BeginPopup("emote_popup"))
                        {
                            ImGui::PushFont(m_sharedData.chatFonts.buttonLarge);

                            for (auto i = 0; i < 25; ++i)
                            {
                                if (ImGui::Button(Emotes[i].icon.data(), ImVec2(0.f, m_sharedData.chatFonts.buttonHeight * viewScale)))
                                {
                                    for (auto cp : Emotes[i].icon)
                                    {
                                        m_inputBuffer.push_back(cp);
                                    }
                                    m_inputBuffer.pop_back(); //removes the terminator.. ugh
                                }

                                if ((i % 5) != 4) ImGui::SameLine();
                            }

                            ImGui::PopFont();
                            ImGui::EndPopup();
                        }
                    }
                    else
                    {
                        ImGui::Text("Chat History");
                    }
                }
                ImGui::End();

                m_sharedData.chatFonts.buttonLarge->Scale = 0.5f;

                ImGui::GetFont()->Scale = 1.f;
                ImGui::PopFont();

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
    Social::hideTextInput();
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

bool TextChat::handlePacket(const net::NetEvent::Packet& pkt)
{
    if (m_sharedData.blockChat)
    {
        return false;
    }

    const auto msg = pkt.as<TextMessage>();

    if (msg.client >= ConstVal::MaxClients)
    {
        LogE << "Recieved message from client " << (int)msg.client << ": Invalid client ID!!" << std::endl;
        return false;
    }

    if (m_sharedData.connectionData[msg.client].playerCount == 0)
    {
        LogW << "Recieved message from client " << (int)msg.client << ": This client is no connected!" << std::endl;
        return false;
    }

    //only one person can type on a connected computer anyway
    //so we'll always assume it's player 0
    auto outStr = m_sharedData.connectionData[msg.client].playerData[0].name;
    auto msgText = msg.getString();// cro::String::fromUtf8(msg.messageData.begin(), std::find(msg.messageData.begin(), msg.messageData.end(), 0));

#ifdef USE_GNS
    Social::filterString(msgText);
#endif

    cro::Colour chatColour = TextNormalColour;
    cro::Colour listColour = TextNormalColour;
    bool playSound = true;

    //process any emotes such as /me and choose colour
    if (auto p = msgText.find("/me"); p == 0
        && msgText.size() > 4)
    {
        chatColour = listColour = TextGoldColour;
        outStr += " " + msgText.substr(p + 4);
    }
    else
    {
        outStr += ": " + msgText;
        playSound = !speak(msgText);

        static std::int32_t idx = 0;
        idx = (idx + 1) % 2;
        listColour = idx == 0 ? TextNormalColour : CD32::Colours[CD32::GreyLight];
    }
    m_displayBuffer.emplace_back(outStr, ImVec4(listColour));

    if (m_displayBuffer.size() > MaxLines)
    {
        m_displayBuffer.pop_front();
    }
    m_scrollToEnd = true;

    printToScreen(outStr, chatColour);

    return playSound;
}

void TextChat::printToScreen(cro::String outStr, cro::Colour chatColour)
{
    //create an entity to temporarily show the message on screen

    auto uiSize = glm::vec2(GolfGame::getActiveTarget()->getSize());
    uiSize /= getViewScale(uiSize);

    cro::Util::String::wordWrap(outStr, 70);

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Label);
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, std::floor(uiSize.y - 18.f), 5.1f });
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

    static constexpr float BgAlpha = 0.45f;
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

        //move the next ent down (so this happens recursively and we get a nice stack)
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


    if (m_sharedData.logChat)
    {
        auto t = std::time(nullptr);
        auto* tm = std::localtime(&t);

        if (m_logFile.is_open() && m_logFile.good())
        {
            const auto utf = outStr.toUtf8();
            m_logFile << std::put_time(tm, "<%H:%M:%S> ") << utf.c_str() <<"\n";
        }
    }
}

void TextChat::toggleWindow(bool showOSK, bool showQuickEmote, bool enableDeckInput)
{
    m_showShortcuts = showQuickEmote;

#ifdef USE_GNS
    if (Social::isSteamdeck()
        && enableDeckInput
        /*&& !m_visible*/)
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
        Social::showTextInput(cb, "Chat");
    }
    else
#endif
    {
        if (showOSK && !Social::isSteamdeck()) //deck uses its own kb
        {
            auto* msg = cro::App::postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::RequestOSK;
            msg->data = 1; //use OSK buffer
        }
        else
        {
            m_visible = (!m_visible /*&& !Social::isSteamdeck()*/);
            m_focusInput = m_visible;

            if (m_visible)
            {
                m_drawWindow = true;
                m_animDir = 1;
                m_scrollToEnd = true;
            }
            else
            {
                m_animDir = 0;
            }
        }
    }
}

void TextChat::update(float dt)
{
    //seems counter intuitive at first but 1 is full animated OUT of the screen
    const float Speed = dt * 6.f;
    if (m_animDir == 0)
    {
        m_animationProgress = std::min(1.f, m_animationProgress + Speed);
    }
    else
    {
        m_animationProgress = std::max(0.f, m_animationProgress - Speed);
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

void TextChat::initLog()
{
    if (!m_logFile.is_open())
    {
        auto t = std::time(nullptr);
        auto* tm = std::localtime(&t);

        std::string filename = "chat_log_" + std::to_string(tm->tm_year + 1900) + "-" + std::to_string(tm->tm_mon + 1) + "-" + std::to_string(tm->tm_mday) + ".txt";
        m_logFile.open(filename, std::ios::app);
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

        if (!m_sharedData.blockChat)
        {
            m_sharedData.clientConnection.netClient.sendPacket(PacketID::ChatMessage, msg, net::NetFlag::Reliable, ConstVal::NetChannelStrings);
        }
        else
        {
            printToScreen("Chat has been disabled from the Options menu", TextHighlightColour);
        }

        m_limitClock.restart();
        m_scrollToEnd = true;

        //m_visible = false;
        
        if (closeOnSend /*|| Social::isSteamdeck()*/)
        {
            m_animDir = 0;
        }

        endChat();
    }
}

bool TextChat::speak(const cro::String& str) const
{
#ifdef _WIN32
    if (!Social::isSteamdeck() &&
        m_sharedData.useTTS)
    {
        if (m_speaker.voice != nullptr)
        {
            //m_speaker.voice->SetVolume(); //TODO read mixer and scale 0 - 100
            m_speaker.voice->Speak((LPCWSTR)(str.toUtf16().c_str()), SPF_ASYNC, nullptr);
            return true;
        }
    }
#elif defined(__linux__)
    m_speaker.say(str, TTSSpeaker::Voice::Three);
    return true;
#endif
    return false;
}

//speaker class for linux
#ifdef __linux__
TextChat::TTSSpeaker::TTSSpeaker()
    : m_threadRunning   (true),
    m_busy              (false),
    m_thread            (&TTSSpeaker::threadFunc, this)
{
    m_threadRunning = cro::FileSystem::fileExists("flite");
    if (!m_threadRunning)
    {
        LogW << "flite not found, TTS is unavailable" << std::endl;
    }
    else
    {
        LogI << "Created TTS" << std::endl;
    }
}

TextChat::TTSSpeaker::~TTSSpeaker()
{
    if (m_threadRunning)
    {
        m_threadRunning = false;
        m_thread.join();
    }
}

//public
void TextChat::TTSSpeaker::say(const cro::String& line, Voice voice) const
{
    LogI << "Saying: " << line.toAnsiString() << std::endl;
    if (cro::FileSystem::fileExists("flite"))
    {
        std::scoped_lock l(m_mutex);
        m_queue.push(std::make_pair(line, voice));
    }
}

//private
void TextChat::TTSSpeaker::threadFunc()
{
    while (m_threadRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        if (!m_queue.empty())
        {
            if (!m_busy)
            {
                cro::String msg;
                Voice type;
                {
                    std::scoped_lock l(m_mutex);
                    msg = m_queue.front().first.to;
                    type = m_queue.front().second;
                    m_queue.pop();
                }

                //attempt to remove unpronouncable chars such
                //as emojis, and add the terminating "
                //std::remove_if(msg.begin(), msg.end(), [](std::uint32_t c)
                //    {
                //        //ugh we're never going to be able to cover everything
                //        //as flite doesn't support UTF in any way that I can tell
                //        //so let's just discard every char which would use more 
                //        //than one byte (so keep ASCII + everything > 127)
                //        return c > 255;
                //    });
                msg += "\"";
                //TODO test that the -t switch enforces playback of single words
                //else we have to hack in a space and a period to make it look like multiple words...

                {
                    std::string say = "./flite -voice ";
                    switch (type)
                    {
                    default:
                    case Voice::One:
                        say += "awb -t \"";
                        break;
                    case Voice::Two:
                        say += "rms -t \"";
                        break;
                    case Voice::Three:
                        say += "slt -t \"";
                        break;
                    }

                    //attempt to preserve any utf encoding
                    const auto utf = msg.toUtf8();
                    std::vector<char> finalMessage(say.length() + utf.size());
                    
                    std::copy(say.begin(), say.end(), finalMessage.data());
                    std::copy(utf.begin(), utf.end(), finalMessage.data() + say.length());
                    

                    //say += msg.toAnsiString();

                    FILE* pipe = popen(/*say.c_str()*/finalMessage.data(), "r");
                    if (pipe)
                    {
                        LogI << "Said: " << /*say*/finalMessage.data() << std::endl;
                        while (pclose(pipe) == -1)
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(30));
                        }
                    }
                    else
                    {
                        LogE << "Could not pipe to flite" << std::endl;
                    }

                    m_busy = false;
                }
            }
        }
    }
}
#endif