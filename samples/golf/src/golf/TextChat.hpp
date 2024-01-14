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

#pragma once

#include "SharedStateData.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <deque>

class TextChat final : public cro::GuiClient
{
public:
    TextChat(cro::Scene&, SharedStateData&);
    ~TextChat(); //there's no ownership here it just resets the OSK

    void handleMessage(const cro::Message&);

    void handlePacket(const net::NetEvent::Packet&);

    void toggleWindow(bool showOSK, bool showQuickEmote);

    bool isVisible() const { return m_visible; }

    void setRootNode(cro::Entity e) { m_rootNode = e; }

    enum
    {
        Angry, Happy, Laughing, Applaud
    };
    void quickEmote(std::int32_t);

    void sendBufferedString();

private:

    cro::Scene& m_scene;
    SharedStateData& m_sharedData;
    bool m_visible;
    bool m_scrollToEnd;
    bool m_focusInput;

    struct BufferLine final
    {
        const cro::String str;
        ImVec4 colour;
        
        BufferLine(const cro::String& s, ImVec4 c)
            : str(s), colour(c)
        { }
    };

    static constexpr std::size_t MaxLines = 24;
    std::deque<BufferLine> m_displayBuffer;
    std::string m_inputBuffer;

    cro::Clock m_limitClock;

    cro::Entity m_rootNode;
    std::array<cro::Entity, 10U> m_screenChatBuffer = {};
    std::size_t m_screenChatIndex;
    std::size_t m_screenChatActiveCount;

    bool m_showShortcuts;
    struct ButtonStrings final
    {
        std::vector<char> angry;
        std::vector<char> applaud;
        std::vector<char> happy;
        std::vector<char> laughing;
    }m_buttonStrings;


    //raises notifications for client icons
    cro::Clock m_chatTimer;
    void beginChat();
    void endChat();

    void sendTextChat();
};
