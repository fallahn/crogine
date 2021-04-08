/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include "AudioRenderer.hpp"
#include "AudioFile.hpp"

#ifdef __APPLE__
#include <al.h>
#include <alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif


#include <SDL_thread.h>

#include <atomic>
#include <array>

namespace cro
{
    namespace Detail
    {
        struct OpenALStream final
        {
            std::array<ALuint, 3> buffers{};
            std::size_t currentBuffer = 0;
            std::unique_ptr<AudioFile> audioFile;
            std::atomic<bool> updating{ false };
            ALint processed = 0;
            SDL_Thread* thread = nullptr;
            std::int32_t sourceID = -1;
            std::atomic<bool> looped{ false };
            ALenum state = AL_STOPPED;
        };

        class OpenALImpl final : public cro::AudioRendererImpl
        {
        public:
            OpenALImpl();

            bool init() override;
            void shutdown() override;

            void setListenerPosition(glm::vec3) override;
            void setListenerOrientation(glm::vec3, glm::vec3) override;
            void setListenerVolume(float) override;

            std::int32_t requestNewBuffer(const std::string& path) override;
            std::int32_t requestNewBuffer(const PCMData&) override;
            void deleteBuffer(std::int32_t) override;

            std::int32_t requestNewStream(const std::string&) override;
            void updateStream(std::int32_t) override;
            void deleteStream(std::int32_t) override;

            std::int32_t requestAudioSource(std::int32_t, bool) override;
            void updateAudioSource(std::int32_t, std::int32_t, bool) override;
            void deleteAudioSource(std::int32_t) override;

            void playSource(std::int32_t, bool) override;
            void pauseSource(std::int32_t) override;
            void stopSource(std::int32_t) override;

            std::int32_t getSourceState(std::int32_t src) const override;

            void setSourcePosition(std::int32_t, glm::vec3) override;
            void setSourcePitch(std::int32_t, float) override;
            void setSourceVolume(std::int32_t, float) override;
            void setSourceRolloff(std::int32_t, float) override;

        private:
            ALCdevice* m_device;
            ALCcontext* m_context;

            std::array<OpenALStream, 64> m_streams = {};
            std::int32_t m_nextStream;
        };
    }
}