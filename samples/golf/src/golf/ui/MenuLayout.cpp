/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "MenuLayout.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>

void scrollToTarget(TabBar& tabBar, Menu& menuLayout, float dt)
{
    const float texHeight = static_cast<float>(menuLayout.texture.getSize().y);
    static constexpr float Stride = UI::ItemHeight + UI::ItemSpacing;
    const float Extents = tabBar.background.getComponent<cro::Transform>().getPosition().y / cro::UIElementSystem::getViewScale();
    const float target = std::clamp((texHeight - (Stride * menuLayout.itemIndex)) - Extents, -UI::ItemHeight, texHeight - (Extents * 2.f));

    auto origin = menuLayout.sprite.getComponent<cro::Transform>().getOrigin();
    const float diff = target - origin.y;
    origin.y += diff * (dt * 10.f);
    menuLayout.sprite.getComponent<cro::Transform>().setOrigin(origin);
}