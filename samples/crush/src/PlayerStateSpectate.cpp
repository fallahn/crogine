/*-----------------------------------------------------------------------

Matt Marchant 2021
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

#include "PlayerState.hpp"
#include "PlayerSystem.hpp"

PlayerStateSpectate::PlayerStateSpectate()
{

}

void PlayerStateSpectate::processMovement(cro::Entity entity, Input input, cro::Scene&)
{
    auto& player = entity.getComponent<Player>();
    
    if (player.local)
    {
        //if button pressed rotate through following other players
        if ((input.buttonFlags & InputFlag::Jump)
            && (player.previousInputFlags & InputFlag::Jump) == 0)
        {
            player.cameraTargetIndex = (player.cameraTargetIndex + 1) % 4;
        }
    }
    player.previousInputFlags = input.buttonFlags;
}

void PlayerStateSpectate::processCollision(cro::Entity, const std::vector<cro::Entity>&)
{

}