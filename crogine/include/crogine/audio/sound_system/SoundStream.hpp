/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/audio/sound_system/SoundSource.hpp>

#include <array>
#include <thread>
#include <mutex>

namespace cro
{
    /*!
    \brief Abstract base class for streaming audio sources.

    The Sound System classes are heavily inspired by the audio module of
    SFML Copyright (C) 2007-2021 Laurent Gomila (laurent@sfml-dev.org)
    which is licensed under the zlib license (outlined above)
    */

    class CRO_EXPORT_API SoundStream : public SoundSource
    {
    public:

        struct Chunk final
        {
            std::size_t sampleCount = 0;
            const std::int16_t* samples = nullptr;
        };

        SoundStream(const SoundStream&) = delete;
        SoundStream(SoundStream&&) = delete;
        SoundStream& operator = (const SoundStream&) = delete;
        SoundStream& operator = (SoundStream&&) = delete;

        virtual ~SoundStream() noexcept;

        /*!
        \brief Play the sound stream.
        Resumes from the current position if the stream was paused
        else starts playback from the beginning.
        */
        void play() override;

        /*!
        \brief Pauses the playback of the stream        
        */
        void pause() override;

        /*!
        \brief Stops the playback of the stream, and resets the position
        to the beginning.
        */
        void stop() override;

        /*!
        \brief Returns the current status of the stream
        */
        Status getStatus() const override;

        /*!
        \brief Returns he numberb eof audio channels in the stream
        */
        std::uint32_t getChannelCount() const;

        /*!
        \brief Returns the sample rate of the stream
        */
        std::uint32_t getSampleRate() const;

    protected:

        static constexpr std::int32_t NoLoop = -1;

        SoundStream();

        /*!
        \brief Initialise the stream with the given parameters

        This function should be called as soon as possible by derived
        classes, in order that the stream be correctly initialised.

        Until it is called at least once operations such as play() or
        pause() will fail. This can be called multiple times with
        new paramters, but must be done while the stream is stopped.

        \param channelCount - The number of audio channels in the stream
        \param sampleRate - The stream sample rate.
        */
        void initialise(std::uint32_t channelCount, std::uint32_t sampleRate);


        /*!
        \break Request a new chunk of audio data from the stream

        This must be implemented by derived classes to provide the audio
        data to the stream. It is call continuously by the stream thread.

        Returning false from this function will stop the current playback,
        else return true to continue playing. When returning true the
        Chunk MUST point to some valid data.

        \param chunk The chunk of data to fill
        \return true to continue streaming, else false to stop
        */
        [[nodiscard]] virtual bool onGetData(Chunk& chunk) = 0;

        /*!
        \brief Seek to an offset in the stream
        \param offset Number of milliseconds into the stream to offset
        */
        virtual void onSeek(std::int32_t offset) = 0;

        /*!
        \brief Set the procesing interval at which onGetData() is called.
        Defaults to 10ms. Larger times create a larger, more reliable buffer
        however it also introduces greater latency.
        \param interval Number of milliseconds to buffer.
        */
        void setProcessingInterval(std::int32_t interval);


        /*!
        \brief Returns loop position if looped or -1 if no loop
        */
        virtual std::int64_t onLoop();

    private:

        static constexpr std::size_t BufferCount = 3;
        static constexpr std::size_t BufferRetries = 2;

        std::thread m_thread;
        mutable std::recursive_mutex m_mutex;
        Status m_startState;
        bool m_isStreaming;
        std::array<std::uint32_t, BufferCount> m_buffers = {};
        std::uint32_t m_channelCount;
        std::uint32_t m_sampleRate;
        std::int32_t m_format;
        bool m_loop;
        std::uint64_t m_samplesProcessed;
        std::int32_t m_processingInterval;
        std::array<std::int64_t, BufferCount> m_bufferSeeks = {};

        void threadFunc(); //called by stream's thread

        [[nodiscard]] bool fillAndPushBuffer(std::uint32_t bufferID, bool loopImmediate = false); //returns true if the stream requests to stop

        [[nodiscard]] bool fillQueue(); //returns true if the stream requested to stop

        void clearQueue();

        void launchThread(Status initialState);

        void waitThread();

    };
}