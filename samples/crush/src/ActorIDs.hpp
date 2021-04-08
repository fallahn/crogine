/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <cstddef>
#include <cstdint>

//it is important that players 0 - 3 are listed first so that
//they match the indices into data arrays pertinent to their client
namespace ActorID
{
    //currently limited to 127, increase data size in update packet if more are needed
    enum
    {
        None = -1,
        PlayerOne = 0,
        PlayerTwo,
        PlayerThree,
        PlayerFour
    };
}


struct Actor final
{
    std::int32_t id = -1;
    std::uint32_t serverEntityId = 0;
};