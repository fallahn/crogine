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

#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <sapi.h>
#elif defined __linux__
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#endif

#include <crogine/Config.hpp>
#include <crogine/core/String.hpp>

namespace cro
{
    class CRO_EXPORT_API TTSSpeaker final
    {
    public:
        TTSSpeaker();
        ~TTSSpeaker();

        TTSSpeaker(const TTSSpeaker&) = delete;
        TTSSpeaker(TTSSpeaker&&) = delete;

        const TTSSpeaker& operator = (const TTSSpeaker&) = delete;
        TTSSpeaker& operator = (TTSSpeaker&&) = delete;

        //setVoice(std::int32_t idx);

        /*!
        \brief Speak the given string
        \param str The string to read
        \param vol Normalised volume at which to start speaking
        \returns true on success else false
        */
        bool speak(const cro::String& str, float vol = 1.f) const;

    private:
        std::int32_t m_voiceIndex;

#ifdef _WIN32
        ISpVoice* m_voice = nullptr;
        bool m_initOK = false;


#elif defined(__linux__)
        std::atomic_bool m_threadRunning;
        std::atomic_bool m_busy;
        mutable std::mutex m_mutex;
        mutable std::queue<std::pair<cro::String, std::int32_t>> m_queue;

        std::thread m_thread;
        void threadFunc();
#endif
    };
}