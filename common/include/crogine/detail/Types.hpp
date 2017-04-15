/*---------------------------------------------------------------------- -

Matt Marchant 2017
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

---------------------------------------------------------------------- -*/

#ifndef CRO_TYPES_HPP_
#define CRO_TYPES_HPP_

#include <crogine/Config.hpp>

#include <SDL_stdinc.h>
#include <SDL_events.h>

#include <glm/vec2.hpp>

/*
Aliases for SDL types
*/

namespace cro
{
	using uint8 = Uint8;
	using int8 = Sint8;
	using uint16 = Uint16;
	using int16 = Sint16;
	using uint32 = Uint32;
	using int32 = Sint32;
	using uint64 = Uint64;
	using int64 = Sint64;

	using Event = SDL_Event;

    namespace ImageFormat
    {
        enum Type
        {
            None,
            RGB,
            RGBA
        };
    }

#ifdef PLATFORM_MOBILE
    static const glm::uvec2 DefaultSceneSize(1280, 720);
#else
    static const glm::uvec2 DefaultSceneSize(1920, 1080);
#endif //PLATFORM
}
#endif //CRO_TYPES_HPP_