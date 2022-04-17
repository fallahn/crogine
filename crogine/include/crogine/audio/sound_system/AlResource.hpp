/*-----------------------------------------------------------------------

Matt Marchant 2022
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

namespace cro
{
    /*!
    \brief Reference counting base class for OpenAL resources used by
    the Sound System.
    The Sound System is distinct from Audio classes in that it can be
    used independently of the ECS and maintains its own context. The
    Sound System classes are heavily inspired by the audio module of
    SFML Copyright (C) 2007-2021 Laurent Gomila (laurent@sfml-dev.org)
    which is licensed under the zlib license (outlined above)

    Any classes which use the Sound System should inherit this class
    so that they are reference counted and the OpenAL context can
    be created or destroyed as needed.
    */
    class CRO_EXPORT_API AlResource
    {
    protected:
        AlResource();

        virtual ~AlResource();
    };
}