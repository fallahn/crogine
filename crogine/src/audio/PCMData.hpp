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

#include <cstdint>

namespace cro
{
    namespace Detail
    {
        //describes the format of PCM data returned by an audio parser
        struct PCMData final
        {
            enum class Format
            {
                MONO8, MONO16, STEREO8, STEREO16, NONE
            }format = Format::NONE;
            std::uint32_t size = 0; //<! size in *bytes*
            std::uint32_t frequency = 0;
            void* data = nullptr;
        };
    }
}