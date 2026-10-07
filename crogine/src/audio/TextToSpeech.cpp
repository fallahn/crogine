/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2026
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/audio/TextToSpeech.hpp>
#include <crogine/core/FileSystem.hpp>

using namespace cro;

//speaker class for linux
#ifdef _WIN32
TTSSpeaker::TTSSpeaker()
    : m_voiceIndex  (0),
    m_initOK        (false),
    m_voice         (nullptr)
{
    //init com interface - must only do this once!!
    if (SUCCEEDED(CoInitialize(NULL)))
    {
        m_initOK = true;

        if (FAILED(CoCreateInstance(CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void**)&m_voice)))
        {
            m_voice = nullptr;
        }
    }
}

TTSSpeaker::~TTSSpeaker()
{
    if (m_voice)
    {
        m_voice->Release();
    }

    //we may still have the com interface init even
    //if the voice fails.
    if (m_initOK)
    {
        CoUninitialize();
    }
}

//public
bool TTSSpeaker::speak(const cro::String& str, float vol) const
{
    if (m_voice != nullptr)
    {
        vol = std::clamp(vol, 0.f, 1.f);
        m_voice->SetVolume(static_cast<std::uint16_t>(vol * 100.f));
        m_voice->Speak((LPCWSTR)(str.toUtf16().c_str()), SPF_ASYNC, nullptr);
        return true;
    }
    return false;
}

#elif defined __linux__
TTSSpeaker::TTSSpeaker()
    : m_voiceIndex  (0),
    m_threadRunning (true),
    m_busy          (false),
    m_thread        (&TTSSpeaker::threadFunc, this)
{
    m_threadRunning = cro::FileSystem::fileExists("flite");
    if (!m_threadRunning)
    {
        LogW << "flite not found, TTS is unavailable" << std::endl;
        m_thread.detach();
    }
    //else
    //{
    //    LogI << "Created TTS" << std::endl;
    //}
}

TTSSpeaker::~TTSSpeaker()
{
    if (m_threadRunning)
    {
        m_threadRunning = false;
        m_thread.join();
    }
}

//public
bool TTSSpeaker::speak(const cro::String& line, float vol) const
{
    //flite doesn't have a vol control :/
    if (m_threadRunning
        && vol > 0.2f)
    {
        std::scoped_lock l(m_mutex);
        m_queue.push(std::make_pair(line, m_voiceIndex));
        return true;
    }
    return false;
}

//private
void TTSSpeaker::threadFunc()
{
    while (m_threadRunning)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        if (!m_queue.empty())
        {
            if (!m_busy)
            {
                cro::String msg;
                std::int32_t type = 0;
                {
                    std::scoped_lock l(m_mutex);
                    msg = m_queue.front().first;
                    type = m_queue.front().second;
                    m_queue.pop();
                }

                //remove mid-line quotes as they break the string
                msg.replace("\"", " ");

                //then propertly terminate
                msg += "\"";

                {
                    std::string say = "./flite -voice ";
                    switch (type)
                    {
                    default:
                    case 0:
                        say += "awb -t \"";
                        break;
                    case 1:
                        say += "rms -t \"";
                        break;
                    case 2:
                        say += "slt -t \"";
                        break;
                    }

                    //attempt to preserve any utf encoding
                    auto utf = msg.toUtf8();
                    //these aren't null terminated by default
                    utf.push_back(0);

                    std::vector<char> finalMessage(say.length() + utf.size());

                    std::copy(say.begin(), say.end(), finalMessage.data());
                    std::copy(utf.begin(), utf.end(), finalMessage.data() + say.length());

                    //TODO is there a way to set the volume?
                    FILE* pipe = popen(finalMessage.data(), "r");
                    if (pipe)
                    {
                        //LogI << "Said: " << finalMessage.data() << std::endl;
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