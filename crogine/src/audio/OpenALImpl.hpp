/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

#include <crogine/core/ConsoleClient.hpp>
#include <crogine/gui/GuiClient.hpp>

#ifdef __APPLE__
#include "al.h"
#include "alc.h"
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <atomic>
#include <array>
#include <thread>
#include <memory>

namespace cro
{
    namespace Detail
    {
        struct OpenALStream final
        {
            std::unique_ptr<AudioFile> audioFile;

            std::array<ALuint, 4u> buffers{};
            std::size_t currentBuffer = 0;
            //ALint processed = 0;

            std::atomic<bool> busy{ false }; //signifies the thread is processing
            std::atomic<bool> running{ false }; //signifies the thread is running
            std::atomic<bool> accessed{ false }; //signifies the main thread is modifying the stream
            
            std::unique_ptr<std::thread> thread;
            void updateStream(); //runs in own thread

            std::int32_t sourceID = -1;
            std::atomic<bool> looped{ false };
            ALenum state = AL_STOPPED;
        };

        class OpenALImpl final : public cro::AudioRendererImpl,
            public cro::ConsoleClient, public cro::GuiClient
        {
        public:
            OpenALImpl();

            bool init() override;
            void shutdown() override;

            void setListenerPosition(glm::vec3) override;
            void setListenerOrientation(glm::vec3, glm::vec3) override;
            void setListenerVolume(float) override;
            void setListenerVelocity(glm::vec3) override;

            glm::vec3 getListenerPosition() const override;

            std::int32_t requestNewBuffer(const std::string& path) override;
            std::int32_t requestNewBuffer(const PCMData&) override;
            void deleteBuffer(std::int32_t) override;

            std::int32_t requestNewStream(const std::string&) override;
            void deleteStream(std::int32_t) override;

            std::int32_t requestAudioSource(std::int32_t, bool) override;
            void updateAudioSource(std::int32_t, std::int32_t, bool) override;
            void deleteAudioSource(std::int32_t) override;

            void playSource(std::int32_t, bool) override;
            void pauseSource(std::int32_t) override;
            void stopSource(std::int32_t) override;

            void setPlayingOffset(std::int32_t, cro::Time) override;
            std::int32_t getSourceState(std::int32_t src) const override;

            void setSourcePosition(std::int32_t, glm::vec3) override;
            void setSourcePitch(std::int32_t, float) override;
            void setSourceVolume(std::int32_t, float) override;
            void setSourceRolloff(std::int32_t, float) override;
            void setSourceVelocity(std::int32_t, glm::vec3) override;
            void setDopplerFactor(float) override;
            void setSpeedOfSound(float) override;

            void printDebug() override;

        private:
            ALCdevice* m_device;
            ALCcontext* m_context;

            static constexpr std::size_t MaxStreams = 128;
            std::array<OpenALStream, MaxStreams> m_streams = {};
            std::array<std::int32_t, MaxStreams> m_streamIDs = {};
            std::size_t m_nextFreeStream;

            static constexpr std::size_t SourceResizeCount = 32;
            std::vector<ALuint> m_sourcePool;
            std::size_t m_nextFreeSource;

            ALuint getSource();
            void freeSource(ALuint);
            void resizeSourcePool();

            std::vector<std::string> m_devices;
            std::string m_preferredDevice;
            void enumerateDevices();
        };
    }
}