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

#include <crogine/detail/Assert.hpp>

namespace cro
{
    namespace Detail
    {
        /*!
        \brief Any classes which rely on SDL subsystems or opengl
        should inherit this class.
        This is done to ensure that the class required to maintain SDL
        (the App class) exists before any resources can be created.
        */
        class CRO_EXPORT_API SDLResource
        {
        public:
            SDLResource();
            virtual ~SDLResource() = default;

            static bool valid();
        };
    }
}