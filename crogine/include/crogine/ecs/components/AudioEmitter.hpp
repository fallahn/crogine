/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

#include <crogine/Config.hpp>
#include <crogine/audio/AudioSource.hpp>

#include <crogine/detail/glm/vec3.hpp>

namespace cro
{
    /*!
    \brief Audio emitter component.
    AudioEmitter components are use to play sounds within the scene.
    An AudioEmitter requires an AudioSource in the same way a sprite
    requires a texture - ie multiple AudioEmitter components may use
    the same AudioSource (and should when playing the same sound!).
    AudioEmitters which use mono sound sources are generally panned
    according to their position in the world (assuming the parent
    entity also has a Transform component attached) but this can 
    depend on the platform and AudioRenderer in use.
    */
    class CRO_EXPORT_API AudioEmitter final
    {
    public:
        AudioEmitter();
        AudioEmitter(const AudioSource&);
        ~AudioEmitter();

        AudioEmitter(const AudioEmitter&) = delete;
        AudioEmitter(AudioEmitter&&) noexcept;

        AudioEmitter& operator = (const AudioEmitter&) = delete;
        AudioEmitter& operator = (AudioEmitter&&) noexcept;

        /*!
        \brief Sets the AudioBuffer or AudioStream used by this AudioEmitter.
        If an AudioBuffer is deleted while in use results are undefined.
        */
        void setSource(const AudioSource&);

        /*!
        \brief Plays the AudioEmitter
        */
        void play();

        /*!
        \brief Pauses the currently playing sound if it is playing
        */
        void pause();

        /*!
        \brief Stops the currently playing sound, if it is playing,
        and rewinds it to the beginning
        */
        void stop();

        /*!
        \brief Sets whether this sound should be played looped or not.
        Note that this is not effective on sounds which are already playing
        until they are stopped/paused then restarted.
        */
        void setLooped(bool looped);

        /*!
        \brief Returns true is this emitter is set to play looped
        */
        bool getLooped() const { return m_transportFlags & Looped; }

        /*!
        \brief Sets the playback pitch of the sound.
        Must be a positive value, where 1 is normal speed.
        */
        void setPitch(float);

        /*!
        \brief Returns the current pitch of the emitter
        */
        float getPitch() const { return m_pitch; }

        /*!
        \brief Sets the volume of the AudioEmitter.
        Must be a positive value, where 0 is silent, 1 is normal.
        Anything above will attempt to amplify the sound. This
        value will be multiplied by the value of the AudioMixer
        channel to which this AudioEmitter is assigned.
        */
        void setVolume(float);

        /*!
        \brief Returns the current volume of this emitter
        */
        float getVolume() const { return m_volume; }

        /*!
        \brief Sets the volume rolloff of the sound.
        The larger this value the more quickly the sounds
        volume will fade with distance from the active listener.
        The default value is 1.0, and a value of 0 will cause the
        AudioEmitter to not fade at all.
        */
        void setRolloff(float);

        /*!
        \brief Returns the current rolloff value of this emitter
        */
        float getRolloff() const { return m_rolloff; }

        /*
        \brief Sets the perceived velocity of the emitter.
        Note that this stays set at the supplied value - it does not
        automatically update with any motion of the entity to which
        the emitter is connected.
        The velocity of the emitter is used to calculate the doppler
        effect of moving objects.
        \see AudioRenderer::setDopplerFactor()
        \see AudioRenderer::setSpeedOfSound()
        */
        void setVelocity(glm::vec3 velocity);

        /*!
        \brief Returns the current velocity setting of the emitter
        \see setVelocity()
        */
        glm::vec3 getVelocity() const { return m_velocity; }


        /*!
        \brief Sets the AudioMixer channel for the AudioEmitter.
        AudioEmitters may be grouped via AudioMixer channels, which
        in turn affect the playback volume of the sources. For example
        an AudioEmitter which plays music could be routed through a
        different channel than AudioEmitters which play sound effects
        so that music may be quickly and easily adusted or muted
        independently of soud effect. By default all sound sources
        are mapped to channel 0
        \see AudioMixer
        */
        void setMixerChannel(std::uint8_t channel);

        /*!
        \brief Returns the current ID of the mixer channel to which
        the AudioEmitter is assigned.
        */
        std::uint8_t getMixerChannel() const { return m_mixerChannel; }

        enum class State{Playing = 0, Paused = 1, Stopped = 2};
        /*!
        \brief Returns the current state of the AudioEmitter 
        */
        State getState() const { return m_state; }

    private:

        friend class AudioSystem;

        State m_state;

        float m_pitch;
        float m_volume;
        float m_rolloff;
        glm::vec3 m_velocity;

        std::uint8_t m_mixerChannel;

        enum
        {
            Play = 0x1,
            Pause = 0x2,
            Stop = 0x4,
            Looped = 0x8
        };
        std::uint8_t m_transportFlags;

        bool m_newDataSource;
        std::int32_t m_ID;
        std::int32_t m_dataSourceID;
        AudioSource::Type m_sourceType;
    };
}