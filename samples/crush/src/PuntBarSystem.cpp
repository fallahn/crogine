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

#include "PuntBarSystem.hpp"
#include "UIConsts.hpp"
#include "PlayerSystem.hpp"

#include <crogine/ecs/components/Drawable2D.hpp>

PuntBarSystem::PuntBarSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(PuntBarSystem))
{
    requireComponent<PuntBar>();
    requireComponent<cro::Drawable2D>();
}

//public
void PuntBarSystem::process(float dt)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        const auto& puntBar = entity.getComponent<PuntBar>();

        const float percent = puntBar.player->puntLevel / Player::PuntCoolDown;
        auto& verts = entity.getComponent<cro::Drawable2D>().getVertexData();
        
        const float current = verts[2].position.x;
        const float target = percent * PuntBarSize.x;
        const float amount = (target - current) * dt * 10.f;
        
        verts[2].position.x += amount;
        verts[3].position.x += amount;
        entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    }
}

//private
void PuntBarSystem::onEntityAdded(cro::Entity entity)
{
    CRO_ASSERT(entity.getComponent<PuntBar>().player, "player proeprties not set");
}