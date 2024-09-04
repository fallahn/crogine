/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "ButtonHoldSystem.hpp"

ButtonHoldSystem::ButtonHoldSystem(cro::MessageBus& mb)
    : cro::System(mb, typeid(ButtonHoldSystem))
{
    requireComponent<ButtonHoldContext>();
}

//public
void ButtonHoldSystem::process(float)
{
    auto& entities = getEntities();
    for (auto entity : entities)
    {
        auto& ctx = entity.getComponent<ButtonHoldContext>();
        if (ctx.isActive 
            && ctx.target != nullptr
            && ctx.timer.elapsed().asSeconds() > ButtonHoldContext::MinClickTime)
        {
            *ctx.target = std::clamp(*ctx.target + ctx.step, ctx.minVal, ctx.maxVal);
            ctx.callback(); //bound to view refresh func
        }
    }
}