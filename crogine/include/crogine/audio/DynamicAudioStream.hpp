/*-----------------------------------------------------------------------

Matt Marchant 2024
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

namespace cro
{
    namespace Detail
    {
        class BufferedStreamLoader;
    }

    /*!
    \brief An AudioSource which streams data from an arbitrary source.
    Unlike an AudioStream this class doesn't rely on files stored on disk,
    rather the user can manually update the internal buffer with audio
    data themselves, such as from a waveform generator or network stream.

    DynamicAudioStreams must live at least as long as the AudioEmitter 
    which use them.
    */
    class CRO_EXPORT_API DynamicAudioStream final : public AudioSource
    {
    public:
        DynamicAudioStream();
        ~DynamicAudioStream();
        DynamicAudioStream(const DynamicAudioStream&) = delete;
        DynamicAudioStream(DynamicAudioStream&&) noexcept;
        DynamicAudioStream& operator = (const DynamicAudioStream&) = delete;
        DynamicAudioStream& operator = (DynamicAudioStream&&) noexcept;

        /*!
        \brief Allows submitting audio data to the internal buffer
        */
        void updateBuffer(const std::vector<std::uint8_t>&);

        /*!
        \brief Returns the underlying data type
        */
        AudioSource::Type getType() const override { return AudioSource::Type::Stream; }

    private:
        Detail::BufferedStreamLoader* m_bufferedStream;
    };
}