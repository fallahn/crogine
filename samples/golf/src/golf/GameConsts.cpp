/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "GameConsts.hpp"
#include "Social.hpp"
#include "../M3UPlaylist.hpp"

#ifdef USE_GNS
#include "Input.hpp"
#endif

bool hasPSLayout(std::int32_t controllerID)
{
#ifdef USE_GNS
    if (Social::isSteamdeck())
    {
        return Input::isPSController(cro::GameController::getSteamHandle(controllerID));  
    }
#endif
    return cro::GameController::hasPSLayout(controllerID);
}