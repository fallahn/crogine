/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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

#ifdef _WIN32
#define NOMINMAX
#include <sapi.h>
#elif defined __linux__
#include <stdio.h>
#include <chrono>
#include <thread>
#include <queue>
#include <atomic>
#include <mutex>
#endif

#include "SharedStateData.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/gui/GuiClient.hpp>

#include <deque>
#include <fstream>

class TextChat final : public cro::GuiClient, public cro::ConsoleClient
{
public:
    TextChat(cro::Scene&, SharedStateData&);
    ~TextChat(); //there's no ownership here it just resets the OSK

    void handleMessage(const cro::Message&);

    bool handlePacket(const net::NetEvent::Packet&); //returns true if a notification sound should play

    void printToScreen(cro::String, cro::Colour);

    void toggleWindow(bool showOSK, bool showQuickEmote, bool enableDeckInput = true);

    bool isVisible() const { return m_visible; }

    void setRootNode(cro::Entity e) { m_rootNode = e; }

    void update(float);

    enum
    {
        Angry, Happy, Laughing, Applaud
    };
    void quickEmote(std::int32_t);

    void sendBufferedString();

    void initLog();

private:

    std::ofstream m_logFile;

    cro::Scene& m_scene;
    SharedStateData& m_sharedData;
    bool m_visible;
    bool m_scrollToEnd;
    bool m_focusInput;
    bool m_drawWindow;
    std::int32_t m_animDir; //1 in 0 out
    float m_animationProgress;

    struct BufferLine final
    {
        const cro::String str;
        ImVec4 colour;

        BufferLine(const cro::String& s, ImVec4 c)
            : str(s), colour(c)
        { }
    };

    static constexpr std::size_t MaxLines = 24;
    static std::deque<BufferLine> m_displayBuffer;
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
        std::vector<char> popout;
    }m_buttonStrings;


    //raises notifications for client icons
    cro::Clock m_chatTimer;
    void beginChat();
    void endChat();

    void sendTextChat();

#ifdef _WIN32
    struct TTSSpeaker final
    {
        ISpVoice* voice = nullptr;
        bool initOK = false;

        TTSSpeaker()
        {
            //init com interface - must only do this once!!
            if (SUCCEEDED(CoInitialize(NULL)))
            {
                initOK = true;

                if (FAILED(CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&voice)))
                {
                    voice = nullptr;
                }
            }
        }

        ~TTSSpeaker()
        {
            if (voice)
            {
                voice->Release();
            }

            //we may still have the com interface init even
            //if the voice fails.
            if (initOK)
            {
                CoUninitialize();
            }
        }
    }m_speaker;

#elif defined(__linux__)
    class TTSSpeaker final
    {
    public:
        enum class Voice
        {
            One, Two, Three
        };

        TTSSpeaker()
            : m_threadRunning   (true),
            m_busy              (false),
            m_thread            (&TTSSpeaker::threadFunc, this)
        {
            /*m_threadRunning = cro::FileSystem::fileExists("flite");
            if (!m_threadRunning)
            {
                LogW << "flite not found, TTS is unavailable" << std::endl;
            }*/
            LogI << "Created TTS" << std::endl;
        }

        ~TTSSpeaker()
        {
            m_threadRunning = false;
            m_thread.join();
        }

        void say(const std::string& line, Voice voice) const
        {
            LogI << "Saying: " << line << std::endl;
            //if (cro::FileSystem::fileExists("flite"))
            {
                std::scoped_lock l(m_mutex);
                m_queue.push(std::make_pair(line, voice));
            }
        }

    private:
        std::atomic_bool m_threadRunning;
        std::atomic_bool m_busy;
        mutable std::mutex m_mutex;
        mutable std::queue<std::pair<std::string, Voice>> m_queue;

        std::thread m_thread;
        void threadFunc()
        {
            while (m_threadRunning)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));

                if (!m_queue.empty())
                {
                    if (!m_busy)
                    {
                        std::string msg;
                        Voice type;
                        {
                            std::scoped_lock l(m_mutex);
                            msg = m_queue.front().first;
                            type = m_queue.front().second;
                            m_queue.pop();
                        }

                        {
                            std::string say = "./flite -voice ";
                            switch (type)
                            {
                            default:
                            case Voice::One:
                                say += "awb \"";
                                break;
                            case Voice::Two:
                                say += "rms \"";
                                break;
                            case Voice::Three:
                                say += "slt \"";
                                break;
                            }
                            say += msg + "\"";

                            FILE* pip = popen(say.c_str(), "r");
                            if (pip)
                            {
                                while (pclose(pip) == -1)
                                {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(30));
                                }
                            }
                            else
                            {
                                LogE << "Could not pip to flite" << std::endl;
                            }

                            m_busy = false;
                        }
                    }
                }
            }
        }
    }m_speaker;
#endif
    bool speak(const cro::String&) const; //returns true if speech was initiated
};
