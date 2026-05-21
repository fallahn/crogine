/*-----------------------------------------------------------------------

Matt Marchant 2026
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include <cstdint>
#include <vector>

class Voice final
{
public:

    /*!
    \brief Opens the selected device and starts listening for captured audio
    */
    static void startRecording();
    /*!
    \brief Closes any currently recording device
    */
    static void stopRecording();

    /*!
    \brief Fetches any available voice data as a compressed packet
    \param dst Destination buffer to fill with packet data
    \param dstSize The size of the destination buffer
    \returns Number of bytes written to the destination buffer
    */
    static std::int32_t getVoice(std::uint8_t* dst, std::uint32_t dstSize);


    /*!
    \brief Decompresses a packet buffer to 16bit mono PCM audio
    \param src Compressed packet to decompress
    \param srcSize size of the packet buffer in bytes
    \param dst Destination buffer for the uncompressed audio
    \returns Number of SAMPLES in the destination buffer
    */
    static std::int32_t decompressVoice(const std::uint8_t* src, std::uint32_t srcSize, std::int16_t* dst);
private:

};
