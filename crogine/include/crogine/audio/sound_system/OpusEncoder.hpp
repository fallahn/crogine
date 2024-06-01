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

#include <crogine/Config.hpp>

#include <vector>
#include <cstdint>

namespace cro
{
    /*!
    \brief Creates an opus stream encoder and decoder from the given
    opus encoder context.
    */

    class CRO_EXPORT_API Opus
    {
    public:
        struct CRO_EXPORT_API Context final
        {
            std::uint32_t channelCount = 1;
            std::uint32_t sampleRate = 24000;

            std::uint32_t maxPacketSize = 1276 * 3;
            std::uint32_t bitrate = 64000;
        };

        explicit Opus(const Context& ctx);
        ~Opus();

        Opus(const Opus&) = delete;
        Opus(Opus&&) = delete;

        const Opus& operator = (const Opus&) = delete;
        const Opus& operator = (Opus&&) = delete;

        /*!
        \brief Encodes the given pcm data using the context settings
        supplied in the constructor.
        \param data Pointer to PCM data to encode. This should contain enough
        data to fill one frame, which is calculated as (sampleRate / 25) * channelCount
        \param dst A vector of bytes into which the packet is placed.
        This will be empty if no encoding occurred
        */
        void encode(const std::int16_t* data, std::vector<std::uint8_t>& dst) const;

        /*!
        \brief Encodes the given floating point pcm data using the context settings
        supplied in the constructor.
        \param data Pointer to PCM data to encode.This should contain enough
        data to fill one frame, which is calculated as (sampleRate / 25) * channelCount
        \param dst A vector of bytes into which the packet is placed.
        This will be empty if no encoding occurred
        */
        void encode(const float* data, std::vector<std::uint8_t>& dst) const;

        /*!
        \brief Attempts to decode the given opus packet using the channel count and sample rate
        passed via context at construstion.
        \param packet A std::vector of bytes containing the packet data
        \returns std::vector of 16 bit PCM samples
        */
        std::vector<std::int16_t> decode(const std::vector<std::uint8_t>& packet) const;

    private:

        void* m_encoder;
        void* m_decoder;

        const std::uint32_t m_frameSize;
        const std::uint32_t m_maxFrameSize;
        const std::uint32_t m_channelCount;

        mutable std::vector<std::uint8_t> m_outBuffer;
        mutable std::vector<std::int16_t> m_decodeBuffer;
    };
}
