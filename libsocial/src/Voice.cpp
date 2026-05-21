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

#include "Voice.hpp"

void Voice::startRecording()
{
    //TODO option to choose which device to capture
    //TODO create encoder, decoder and capture device
}

void Voice::stopRecording()
{
    //TODO tidy up capture, encoder and decoder objects
}

std::int32_t Voice::getVoice(std::uint8_t* dst, std::uint32_t dstSize)
{
    //TODO dequeue from audio device and push to encoder
    //TODO pop from encoder and return output
    return 0;
}

std::int32_t Voice::decompressVoice(const std::uint8_t* src, std::uint32_t srcSize, std::int16_t* dst)
{
    //TODO push to decoder
    //TODO pop from decoder and push to SDL Stream to resmaple
    //TODO return resmapled data
    return 0;
}