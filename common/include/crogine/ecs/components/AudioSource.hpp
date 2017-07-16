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

#ifndef CRO_AUDIO_SOURCE_HPP_
#define CRO_AUDIO_SOURCE_HPP_

#include <crogine/Config.hpp>
#include <crogine/audio/AudioBuffer.hpp>

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
        AudioSource(const AudioBuffer&);
        ~AudioSource();

        /*!
        \brief Sets the AudioBuffer used by this AudioSource.
        If an AudioBuffer is deleted while in use results are undefined.
        */
        void setBuffer(const AudioBuffer&);

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

        //TODO pitch, volume, mixer channel

    private:

        friend class AudioSystem;

        enum
        {
            Play = 0x1,
            Pause = 0x2,
            Stop = 0x4,
            Looped = 0x8
        };
        uint8 m_transportFlags;

        bool m_newBuffer;
        int32 m_ID;
        int32 m_bufferID;
    };
}

#endif //CRO_AUDIO_SOURCE_HPP_