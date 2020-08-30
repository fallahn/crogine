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

#include <crogine/audio/AudioSource.hpp>

#include <string>

namespace cro
{
    /*!
    \brief An AudioSource which streams data from storage.
    When playing longer audio files such as music it is recommended
    that an AudioEmitter component use an AudioStream as its data
    source rather than a buffer. Streams will stream either *.wav
    or *.ogg data from disk, as opposed to attempting to load the
    entire file into memory.
    As with AudioBuffers, AudioStreams must live at least as long as
    the AudioEmitter which use them. Unlike AudioBuffers, however,
    AudioStreams can be attached only to one AudioEmitter. Assigning
    an AudioStream to further sources will only result in multiple
    sources playing back the same stream.
    */
    class CRO_EXPORT_API AudioStream final : public AudioSource
    {
    public:
        AudioStream();
        ~AudioStream();
        AudioStream(const AudioStream&) = delete;
        AudioStream(AudioStream&&) noexcept;
        AudioStream& operator = (const AudioStream&) = delete;
        AudioStream& operator = (AudioStream&&) noexcept;

        /*!
        \brief Attempts to open the *.ogg or *.wav file fomr the given path.
        \returns true on success else false.
        */
        bool loadFromFile(const std::string& path) override;

        /*!
        \brief Returns the underlying data type
        */
        AudioSource::Type getType() const override { return AudioSource::Type::Stream; }

    private:

    };
}