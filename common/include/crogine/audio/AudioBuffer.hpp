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

#ifndef CRO_AUDIO_BUFFER_HPP_
#define CRO_AUDIO_BUFFER_HPP_

#include <crogine/Audio/AudioDataSource.hpp>

#include <string>

namespace cro
{
    /*!
    \brief Buffers audio data used by AudioSource components.
    AudioBuffers are the audio equivalent to textures, as AudioSources
    are to sprites or materials. Audio data is loaded, usually from disc,
    into an AudioBuffer, which can then be used by multiple sound sources.
    For this reason AudioBuffers should be resource managed in the same way
    as textures or other resources, using the appropriate resource management
    class - AudioResource. This means that AudioBuffers, again like Textures,
    are non-copyable, but are movable. Destroying an AudioBuffer which is
    still in use by one or more AudioSources will result in undefined
    behviour, depending on the audio renderer currently in use.
    */
    class CRO_EXPORT_API AudioBuffer final : public AudioDataSource
    {
    public:
        AudioBuffer();
        ~AudioBuffer();

        AudioBuffer(const AudioBuffer&) = delete;
        AudioBuffer(AudioBuffer&&);

        AudioBuffer& operator = (const AudioBuffer&) = delete;
        AudioBuffer& operator = (AudioBuffer&&);

        /*!
        \brief Attempts to load a file from the given path.
        Currently supported formats are PCM wav files (mono or stereo)
        or *.ogg vorbis files (mono or stereo).
        \returns true on success, else false
        */
        bool loadFromFile(const std::string&);

        /*!
        \brief Attempts to load the buffer with data stored in memory.
        Data should be uncompressed PCM audio in either mono or stereo
        format, 8 or 16 bit sample size.
        \param data Pointer to data in memory. Data is copied into the
        buffer so it is safe to free it afterwards.
        \param bitdepth Valid values are 8 or 16
        \param sampleRate The frequency of the sample rate. Usually 
        22500, 44100 or 48000. Other rates are supported but performance
        may vary.
        \param stereo If this is true the data is assumed to be stereo,
        else it is treated as mono. More than 2 channels are currently not
        supported.
        \param size The size of the data pointed to by data, in bytes.
        \returns true on success, else false
        */
        bool loadFromMemory(void* data, uint8 bitDepth, uint32 sampleRate, bool stereo, std::size_t size);

        /*!
        \brief Identifies this as an AudioBuffer
        */
        AudioDataSource::Type getType() const override { return AudioDataSource::Type::Buffer; }

    private:

    };
}

#endif //CRO_AUDIO_BUFFER_HPP_