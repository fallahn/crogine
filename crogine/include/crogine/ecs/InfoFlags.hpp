/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
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
	/*!
	\brief Flags used with cro::Scene to display information useful for debugging
	*/

	static constexpr std::uint32_t INFO_FLAG_SYSTEMS_ACTIVE = 0x1; //<! displays a window that prints active systems in the Scene
	static constexpr std::uint32_t INFO_FLAG_SYSTEM_TIME    = 0x2; //<! displays a window containing timing information about System updates
}