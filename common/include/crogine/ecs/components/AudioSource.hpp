/*-----------------------------------------------------------------------

Matt Marchant 2017
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
#include <crogine/audio/AudioDataSource.hpp>

namespace cro
{
    /*!
    \brief Audio source component.
    AudioSource components are use to play sounds within the scene.
    An AudioSource requires an AudioBuffer ins the same way a sprite
    requires a texture - ie multiple AudioSource components may use
    the same AudioBuffer (and should when playing the same sound!).
    AudioSources which use mono sound buffers are generally panned
    according to their position in the world (assuming the parent
    entity also has a Transform component attached) but this can 
    depend on the platform and AudioRenderer in use.
    */
    class CRO_EXPORT_API AudioSource final
    {
    public:
        AudioSource();
        AudioSource(const AudioDataSource&);
        ~AudioSource();

        AudioSource(const AudioSource&) = delete;
        AudioSource(AudioSource&&) noexcept;

        AudioSource& operator = (const AudioSource&) = delete;
        AudioSource& operator = (AudioSource&&) noexcept;

        /*!
        \brief Sets the AudioBuffer or AudioStream used by this AudioSource.
        If an AudioBuffer is deleted while in use results are undefined.
        */
        void setAudioDataSource(const AudioDataSource&);

        /*!
        \brief Plays the AudioSource
        \param loop Whether or not this sound should be played looped
        */
        void play(bool looped = false);

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
        \brief Sets the playback pitch of the sound.
        Must be a positive value, where 1 is normal speed.
        */
        void setPitch(float);

        /*!
        \brief Sets the volume of the AudioSource.
        Must be a positive value, where 0 is silent, 1 is normal.
        Anything above will attempt to amplify the sound. This
        value will be multiplied by the value of the AudioMixer
        channel to which this AudioSource is assigned.
        */
        void setVolume(float);

        /*!
        \brief Sets the volume rolloff of the sound.
        The larger this value the more quickly the sounds
        volume will fade with distance from the active listener.
        The default value is 1.0, and a value of 0 will cause the
        AudioSource to not fade at all.
        */
        void setRolloff(float);

        /*!
        \brief Sets the AudioMixer channel for the AudioSource.
        AudioSources may be grouped via AudioMixer channels, which
        in turn affect the playback volume of the sources. For example
        an AudioSource which plays music could be routed through a
        different channel than AudioSources which play sound effects
        so that music may be quickly and easily adusted or muted
        independently of soud effect. By default all sound sources
        are mapped to channel 0
        \see AudioMixer
        */
        void setMixerChannel(uint8 channel);

        /*!
        \brief Returns the current ID of the mixer channel to which
        the AudioSource is assigned.
        */
        uint8 getMixerChannel() const { return m_mixerChannel; }

        enum class State{Playing = 0, Paused = 1, Stopped = 2};
        /*!
        \brief Returns the current state of the AudioSource 
        */
        State getState() const { return m_state; }

    private:

        friend class AudioSystem;

        State m_state;

        float m_pitch;
        float m_volume;
        float m_rolloff;

        uint8 m_mixerChannel;

        enum
        {
            Play = 0x1,
            Pause = 0x2,
            Stop = 0x4,
            Looped = 0x8
        };
        uint8 m_transportFlags;

        bool m_newDataSource;
        int32 m_ID;
        int32 m_dataSourceID;
        AudioDataSource::Type m_sourceType;
    };
}