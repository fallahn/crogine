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

#include <crogine/detail/Types.hpp>

namespace cro::Detail
{
    /*!
    \brief Declares pooled resources which inherit this not have their component
    pools resized in cases where it will harmfully invalidate references. For 
    example a component which contains a transform that has pointers to it should
    not be moved during reallocation of the component pool, else those references
    will become invalidated.

    Classes inheriting this should be components in the ECS (else this base class
    will have no effect), and will have the maximum memory pool size of MaxGenerations
    components reserved for any potential pool resize events.
    */
    class CRO_EXPORT_API NonResizeable
    {
    public: virtual ~NonResizeable() {};
    };
}