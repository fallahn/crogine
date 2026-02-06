/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
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
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>

std::pair<cro::FloatRect, cro::FloatRect> scrollToTarget(TabBar& tabBar, Menu& menuLayout, float dt)
{
    const float viewScale = cro::UIElementSystem::getViewScale();
    const float texHeight = static_cast<float>(menuLayout.texture.getSize().y);
    static constexpr float Stride = UI::ItemHeight + UI::ItemSpacing;
    const float Extents = tabBar.background.getComponent<cro::Transform>().getPosition().y / viewScale;

    const glm::vec2 WindowSizeScaled = glm::vec2(cro::App::getWindow().getSize()) / viewScale;
    const float bottom = menuLayout.sprite.getComponent<cro::Drawable2D>().getCroppingArea().bottom / viewScale;
    //origin is CENTRE of the screen
    const cro::FloatRect viewRect = { 0.f, bottom, WindowSizeScaled.x, (Extents + (WindowSizeScaled.y / 2.f)) - bottom };
    
    auto origin = menuLayout.sprite.getComponent<cro::Transform>().getOrigin();
    //these aren't fixed width, so we use a bare minimum
    const cro::FloatRect itemRect = { 10.f, (texHeight - (Stride * (menuLayout.itemIndex + 1))) - origin.y, UI::ItemHeight, UI::ItemHeight };

    if (!viewRect.contains(itemRect))
    {
        const float target = std::clamp((texHeight - (Stride * menuLayout.itemIndex)) - Extents, -UI::ItemHeight, texHeight - (Extents * 2.f));
        const float diff = target - origin.y;
        origin.y += diff * (dt * 5.f);
        menuLayout.sprite.getComponent<cro::Transform>().setOrigin(origin);
    }
    else
    {
        //make sure to scroll to nearest whole pixel
        origin.y = std::round(origin.y);
        menuLayout.sprite.getComponent<cro::Transform>().setOrigin(origin);
    }

    return { viewRect, itemRect };
}

void focusToIndex(TabBar& tabBar, Menu& menuLayout)
{
    const float viewScale = cro::UIElementSystem::getViewScale();
    const float texHeight = static_cast<float>(menuLayout.texture.getSize().y);
    static constexpr float Stride = UI::ItemHeight + UI::ItemSpacing;
    const float Extents = tabBar.background.getComponent<cro::Transform>().getPosition().y / viewScale;
    const float target = std::clamp((texHeight - (Stride * menuLayout.itemIndex)) - Extents, -UI::ItemHeight, texHeight - (Extents * 2.f));

    auto origin = menuLayout.sprite.getComponent<cro::Transform>().getOrigin();
    origin.y = target - UI::ItemHeight;
    menuLayout.sprite.getComponent<cro::Transform>().setOrigin(origin);
}