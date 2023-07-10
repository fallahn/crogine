/*-----------------------------------------------------------------------

Matt Marchant 2023
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

#include "AudioFile.hpp"

#include <vector>
#include <memory>

struct mp3dec_ex_t;
namespace cro
{
    namespace Detail
    {
        /*!
        \brief File loader for opening mp3 files using minimp3
        */
        class Mp3Loader final : public AudioFile
        {
        public:
            Mp3Loader();

            ~Mp3Loader();
            Mp3Loader(const Mp3Loader&) = delete;
            Mp3Loader(Mp3Loader&&) noexcept = delete;
            const Mp3Loader& operator = (const Mp3Loader&) = delete;
            Mp3Loader& operator = (Mp3Loader&&) noexcept = delete;

            bool open(const std::string&) override;

            const PCMData& getData(std::size_t = 0, bool looped = false) const override;

            bool seek(cro::Time) override;

            PCMData::Format getFormat() const override { return m_dataChunk.format; }

            std::int32_t getSampleRate() const override { return m_dataChunk.frequency; }

            std::uint64_t getSampleCount() const override { return m_sampleCount; }

        private:
            std::unique_ptr<mp3dec_ex_t> m_decoder;
            std::int32_t m_channelCount;
            std::uint64_t m_sampleCount;

            mutable PCMData m_dataChunk;
            mutable std::vector<std::int16_t> m_buffer;
        };
    }
}