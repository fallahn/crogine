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
            int32 sourceID = -1;
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

            cro::int32 requestNewBuffer(const std::string& path) override;
            cro::int32 requestNewBuffer(const PCMData&) override;
            void deleteBuffer(cro::int32) override;

            cro::int32 requestNewStream(const std::string&) override;
            void updateStream(cro::int32) override;
            void deleteStream(cro::int32) override;

            cro::int32 requestAudioSource(cro::int32, bool) override;
            void updateAudioSource(cro::int32, cro::int32, bool) override;
            void deleteAudioSource(cro::int32) override;

            void playSource(cro::int32, bool) override;
            void pauseSource(cro::int32) override;
            void stopSource(cro::int32) override;

            int32 getSourceState(int32 src) const override;

            void setSourcePosition(int32, glm::vec3) override;
            void setSourcePitch(int32, float) override;
            void setSourceVolume(int32, float) override;
            void setSourceRolloff(int32, float) override;

        private:
            ALCdevice* m_device;
            ALCcontext* m_context;

            std::array<OpenALStream, 64> m_streams = {};
            int32 m_nextStream;
        };
    }
}