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
#include "../MenuConsts.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/detail/OpenGL.hpp>

void UILayout::loadAssets(cro::ResourceCollection & resources)
{
    const auto& font = resources.fonts.get(FontID::Info);
    menuText.setFont(font);
    menuText.setCharacterSize(InfoTextSize);

    const auto& largeFont = resources.fonts.get(FontID::UI);
    menuTextLarge.setFont(largeFont);
    menuTextLarge.setCharacterSize(UITextSize);
    menuTextLarge.setAlignment(cro::SimpleText::Alignment::Centre);

    itemSlider.setPrimitiveType(GL_TRIANGLES);

    cro::SpriteSheet spriteSheet;
    if (spriteSheet.loadFromFile("assets/golf/sprites/options_buttons.spt", resources.textures))
    {
        uiTexture = spriteSheet.getTexture();

        const auto parseSprite = [&](const std::string& spr, SpriteSection& dst)
            {
                auto bounds = spriteSheet.getSprite(spr).getTextureBounds();
                auto uv = spriteSheet.getSprite(spr).getTextureRectNormalised();
                dst.size = { bounds.width, bounds.height };
                dst.uv = { uv.left, uv.bottom, uv.left + uv.width, uv.bottom + uv.height };
            };

        //active tab
        parseSprite("tab_active_left", tabActive[0]);
        parseSprite("tab_active_right", tabActive[1]);

        //inactive tab
        parseSprite("tab_inactive_left", tabInactive[0]);
        parseSprite("tab_inactive_right", tabInactive[1]);

        //highlight tab
        parseSprite("tab_highlight_left", tabHighlight[0]);
        parseSprite("tab_highlight_right", tabHighlight[1]);



        //background 9-patch
        parseSprite("background_centre", backgroundSections[BackgroundSection::Centre]);
        parseSprite("background_top", backgroundSections[BackgroundSection::Top]);
        parseSprite("background_left", backgroundSections[BackgroundSection::Left]);
        parseSprite("background_right", backgroundSections[BackgroundSection::Right]);
        parseSprite("background_bottom", backgroundSections[BackgroundSection::Bottom]);
        parseSprite("background_tl", backgroundSections[BackgroundSection::TL]);
        parseSprite("background_tr", backgroundSections[BackgroundSection::TR]);
        parseSprite("background_bl", backgroundSections[BackgroundSection::BL]);
        parseSprite("background_br", backgroundSections[BackgroundSection::BR]);



        //item backgrounds
        parseSprite("item_background_left", itemSection[0]);
        parseSprite("item_background_right", itemSection[1]);

        //item active
        parseSprite("item_active_left", itemActiveSection[0]);
        parseSprite("item_active_right", itemActiveSection[1]);

        //item active highlight
        parseSprite("item_highlight_active_left", itemActiveHighlightSection[0]);
        parseSprite("item_highlight_active_right", itemActiveHighlightSection[1]);

        //item highlight
        parseSprite("item_highlight_left", itemHighlightSection[0]);
        parseSprite("item_highlight_right", itemHighlightSection[1]);

        //item title
        parseSprite("item_title_left", itemTitleSection[0]);
        parseSprite("item_title_right", itemTitleSection[1]);


        itemBackground.setTexture(*uiTexture);
        itemBackground.setPrimitiveType(GL_TRIANGLES);

        itemBackgroundActive.setTexture(*uiTexture);
        itemBackgroundActive.setPrimitiveType(GL_TRIANGLES);

        itemBackgroundActiveHighlight.setTexture(*uiTexture);
        itemBackgroundActiveHighlight.setPrimitiveType(GL_TRIANGLES);

        itemBackgroundHighlight.setTexture(*uiTexture);
        itemBackgroundHighlight.setPrimitiveType(GL_TRIANGLES);

        itemBackgroundTitle.setTexture(*uiTexture);
        itemBackgroundTitle.setPrimitiveType(GL_TRIANGLES);
    }
}

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
        const float target = std::min(std::max((texHeight - (Stride * menuLayout.itemIndex)) - Extents, -UI::ItemHeight), texHeight - (Extents * 2.f));
        const float diff = target - origin.y;
        origin.y += diff * (dt * 4.f);
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
    const float target = std::min(std::max((texHeight - (Stride * menuLayout.itemIndex)) - Extents, -UI::ItemHeight), texHeight - (Extents * 2.f));

    auto origin = menuLayout.sprite.getComponent<cro::Transform>().getOrigin();
    origin.y = target - UI::ItemHeight;
    menuLayout.sprite.getComponent<cro::Transform>().setOrigin(origin);
}