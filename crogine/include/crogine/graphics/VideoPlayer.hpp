/*-----------------------------------------------------------------------

Matt Marchant 2022
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <crogine/Config.hpp>
#include <crogine/audio/sound_system/SoundStream.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <vector>

struct plm_t;
typedef plm_t plm_t;

struct plm_plane_t;
typedef plm_plane_t plm_plane_t;

struct plm_frame_t;
typedef plm_frame_t plm_frame_t;

struct plm_samples_t;
typedef plm_samples_t plm_samples_t;

namespace cro
{
    void videoCallback(plm_t*, plm_frame_t*, void*);
    void audioCallback(plm_t*, plm_samples_t*, void*);

    /*
    Video player class, which renders MPEG1 video to a texture using
    pl_mpeg: https://github.com/phoboslab/pl_mpeg

    Note that the audio is streamed separately from the ECS and so is
    not assignable to any mixer channels. The only volume channel which
    affects audio playback is the Master control for all audio.

    In testing VCD video files have been found to not present audio
    channels to the plm decoder, and need to be remuxed as MPG-PS.
    */

    class CRO_EXPORT_API VideoPlayer final : public cro::Detail::SDLResource
    {
    public:
        VideoPlayer();
        ~VideoPlayer();

        VideoPlayer(const VideoPlayer&) = delete;
        VideoPlayer(VideoPlayer&&) noexcept = delete;

        //hmm...need to move plm pointer if we want to implement this
        //at the very least, not to mention what happens if you
        //try to move an instance which is currently playing...
        //VideoPlayer(VideoPlayer&&) noexcept; 

        VideoPlayer& operator = (const VideoPlayer&) = delete;
        VideoPlayer& operator = (VideoPlayer&&) noexcept = delete;
        //VideoPlayer& operator = (VideoPlayer&&) noexcept;

        /*!
        \brief Attempts to open an MPEG1 file.
        \returns true on success or false if the file doesn't
        exist or is not a valid MPEG1 file.
        */
        bool loadFromFile(const std::string& path);

        /*!
        \brief Updates the decoding of the file, if a file is open.
        This is automatically locked to the frame rate of the video
        up to the maximum rate at which this is called, at which point
        frames will be skipped.
        \param dt The time since this function was last called

        hmmmmm - shame we can't spin this off into a thread, but OpenGL.
        */
        void update(float dt);

        /*!
        \brief Starts the playback of the loaded file if available
        else does nothing.
        */
        void play();

        /*!
        \brief Pauses the current playback if it is playing
        else does nothing.
        */
        void pause();

        /*!
        \brief Stops the current playback if the file is playing, and
        returns the playback position to 0, else does nothing.
        */
        void stop();

        /*
        \brief Attempts to seek to the given time in the video file,
        if one is open. If the given time is out of range, or no file
        is loaded then it does nothing.
        \param position - Time in seconds to which to seek.
        */
        void seek(float position);

        /*!
        \brief Returns the duration of a loaded file in seconds, or zero
        if no file is currently loaded.
        */
        float getDuration() const;

        /*!
        \brief Return the current position, in seconds, within the loaded
        file, or zero if no file is loaded.
        */
        float getPosition() const;

        /*!
        \brief Set looped playback enabled
        \param looped - True to loop playback, or false to stop when
        playback reaches the end of the file.
        */
        void setLooped(bool looped);

        /*!
        \brief Gets whether or not playback is currently set to looped
        */
        bool getLooped() const { return m_looped; };

        /*!
        \brief Returns a reference to the texture to which the video is
        rendered.
        */
        const cro::Texture& getTexture() const { return m_outputBuffer.getTexture(); }

    private:

        plm_t* m_plm;
        bool m_looped;

        float m_timeAccumulator;
        float m_frameTime;

        enum class State
        {
            Stopped, Playing, Paused
        }m_state;

        cro::Shader m_shader;

        cro::Texture m_y;
        cro::Texture m_cb;
        cro::Texture m_cr;
        cro::SimpleQuad m_quad;
        cro::RenderTexture m_outputBuffer;

        void updateTexture(std::uint32_t, plm_plane_t*);
        void updateBuffer();


        class AudioStream final : public cro::SoundStream
        {
        public:
            bool hasAudio = false;

            bool onGetData(cro::SoundStream::Chunk&) override;
            void onSeek(std::int32_t) override {}

            void init(std::uint32_t channels, std::uint32_t sampleRate);

            void pushData(float*);

        private:
            static constexpr std::int32_t SAMPLES_PER_FRAME = 1152;
            std::array<std::int16_t, SAMPLES_PER_FRAME * 8> m_inBuffer = {};
            std::array<std::int16_t, SAMPLES_PER_FRAME * 2> m_outBuffer = {};

            std::uint32_t m_bufferIn = SAMPLES_PER_FRAME * 6;
            std::uint32_t m_bufferOut = 2;

        }m_audioStream;

        //because function pointers
        friend void videoCallback(plm_t*, plm_frame_t*, void*);
        friend void audioCallback(plm_t*, plm_samples_t*, void*);
    };
}