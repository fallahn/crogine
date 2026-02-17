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
#include "../SharedStateData.hpp"
#include "../../Colordome-32.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/detail/OpenGL.hpp>

UILayout::UILayout(std::int32_t tabCount)
{
    tabBar.items.resize(tabCount);
    menuLayout.items.resize(tabCount);
    detailsPane.tabDetails.resize(tabCount);
}

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

void UILayout::updateTabBar(const SharedStateData& sharedData)
{
    const glm::vec2 WindowSize = cro::App::getWindow().getSize();

    const float Spacing = 1.f / (tabBar.items.size() + 1); //leave equivalent of half a tab either end
    const float TabWidth = std::round(Spacing * WindowSize.x);

    std::vector<cro::Vertex2D> verts;
    const auto viewScale = cro::UIElementSystem::getViewScale();

    if (uiTexture)
    {
        const auto width = TabWidth - viewScale;
        const auto height = UI::TabBarHeight * viewScale;

        const auto addQuad =
            [&](glm::vec2 position, const SpriteSection& left, const SpriteSection& right)
            {
                const auto sectionWidth = left.size.x * viewScale;

                //left section
                verts.emplace_back(glm::vec2(position.x, position.y + height), glm::vec2(left.uv.left, left.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(left.uv.width, left.uv.height));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(left.uv.width, left.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y), glm::vec2(left.uv.width, left.uv.bottom));


                //middle section
                position.x += sectionWidth;
                const auto centreWidth = (TabWidth - (sectionWidth * 2.f));
                verts.emplace_back(glm::vec2(position.x, position.y + height), glm::vec2(left.uv.width, left.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + centreWidth, position.y + height), glm::vec2(right.uv.left, right.uv.height));
                verts.emplace_back(glm::vec2(position.x + centreWidth, position.y + height), glm::vec2(right.uv.left, right.uv.height));
                verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + centreWidth, position.y), glm::vec2(right.uv.left, right.uv.bottom));


                //right section
                position.x += centreWidth;
                verts.emplace_back(glm::vec2(position.x, position.y + height), glm::vec2(right.uv.left, right.uv.height));
                verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(right.uv.width, right.uv.height));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y + height), glm::vec2(right.uv.width, right.uv.height));
                verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
                verts.emplace_back(glm::vec2(position.x + sectionWidth, position.y), glm::vec2(right.uv.width, right.uv.bottom));
            };

        for (auto i = 0u; i < tabBar.items.size(); ++i)
        {
            const auto active = i == tabBar.activeIndex;
            const auto hovered = (i == tabBar.hoveredIndex && sharedData.activeInput == SharedStateData::ActiveInput::Keyboard);

            const float kludgeOffset = (2.f * viewScale);
            glm::vec2 position = { (std::round(TabWidth / 2.f) + kludgeOffset) + ((i * TabWidth) + viewScale), 0.f };
            if (active)
            {
                addQuad(position, tabActive[0], tabActive[1]);
            }
            else if (hovered)
            {
                addQuad(position, tabHighlight[0], tabHighlight[1]);
            }
            else
            {
                addQuad(position, tabInactive[0], tabInactive[1]);
            }

            //set the text
            position += glm::vec2(tabBar.background.getComponent<cro::Transform>().getPosition());
            position += WindowSize / 2.f; //screen centre
            tabBar.items[i].hitbox = { position, glm::vec2(width, height) };
            tabBar.items[i].text.getComponent<cro::Text>().setFillColour(active ? TextNormalColour :
                hovered ? CD32::Colours[CD32::Yellow] : CD32::Colours[CD32::BeigeMid]);
        }

        //add a quad to the verts as an underline
        const auto backgroundCentre = backgroundSections[BackgroundSection::Centre].uv;
        const glm::vec2 uv0(backgroundCentre.left, backgroundCentre.bottom);
        const glm::vec2 uv1(backgroundCentre.width, backgroundCentre.height);
        verts.emplace_back(glm::vec2(0.f, 0.f), glm::vec2(uv0.x, uv1.y));
        verts.emplace_back(glm::vec2(0.f, -viewScale), uv0);
        verts.emplace_back(glm::vec2(WindowSize.x, 0.f), uv1);
        verts.emplace_back(glm::vec2(WindowSize.x, 0.f), uv1);
        verts.emplace_back(glm::vec2(0.f, -viewScale), uv0);
        verts.emplace_back(glm::vec2(WindowSize.x, -viewScale), glm::vec2(uv1.x, uv0.y));
    }
    else
    {
        const auto addQuad =
            [&](cro::Colour c, glm::vec2 position, glm::vec2 size)
            {
                verts.emplace_back(glm::vec2(position.x, position.y + size.y), c);
                verts.emplace_back(position, c);
                verts.emplace_back(position + size, c);

                verts.emplace_back(position + size, c);
                verts.emplace_back(position, c);
                verts.emplace_back(glm::vec2(position.x + size.x, position.y), c);
            };

        //update the verts for the tab bar.
        for (auto i = 0u; i < tabBar.items.size(); ++i)
        {
            const auto active = i == tabBar.activeIndex;
            const auto hovered = (i == tabBar.hoveredIndex && sharedData.activeInput == SharedStateData::ActiveInput::Keyboard);

            const auto colour = active ? CD32::Colours[CD32::Brown] :
                hovered ?
                CD32::Colours[CD32::Yellow] : CD32::Colours[CD32::TanDarkest];

            glm::vec2 position = { std::round(TabWidth / 2.f) + (i * TabWidth), 0.f };
            const glm::vec2 size = { TabWidth - viewScale, UI::TabBarHeight * viewScale };
            addQuad(colour, position, size);

            position += glm::vec2(tabBar.background.getComponent<cro::Transform>().getPosition());
            position += WindowSize / 2.f; //screen centre
            tabBar.items[i].hitbox = { position, size };
            tabBar.items[i].text.getComponent<cro::Text>().setFillColour(active ? TextNormalColour :
                hovered ? CD32::Colours[CD32::Black] : CD32::Colours[CD32::BeigeMid]);
        }

        addQuad(CD32::Colours[CD32::Brown], { 0.f, -viewScale }, { WindowSize.x, viewScale });
    }

    tabBar.background.getComponent<cro::Drawable2D>().setVertexData(verts);

    const auto DetailOffset = (((1.f - tabBar.items[tabBar.activeIndex].displayWidth) / 2.f) + tabBar.items[tabBar.activeIndex].displayWidth) - 0.5f;

    switch (tabBar.items[tabBar.activeIndex].alignment)
    {
    default:
    case TabBar::Item::Left:
        menuLayout.sprite.getComponent<cro::Transform>().setPosition({ 0.f, 0.f });

        detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        detailsPane.root.getComponent<cro::UIElement>().relativePosition.x = DetailOffset;
        break;
    case TabBar::Item::Centre:
    {
        const float x = std::round((WindowSize.x - (static_cast<float>(menuLayout.texture.getSize().x * cro::UIElementSystem::getViewScale()) * tabBar.items[tabBar.activeIndex].displayWidth)) / 2.f);
        menuLayout.sprite.getComponent<cro::Transform>().setPosition({ x, 0.f });

        detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
    break;
    case TabBar::Item::Right:
    {
        const float x = std::round(WindowSize.x - ((static_cast<float>(menuLayout.texture.getSize().x) * tabBar.items[tabBar.activeIndex].displayWidth) * cro::UIElementSystem::getViewScale()));
        menuLayout.sprite.getComponent<cro::Transform>().setPosition({ x, 0.f });

        detailsPane.root.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        detailsPane.root.getComponent<cro::UIElement>().relativePosition.x = -DetailOffset;
    }
    break;
    }
    menuLayout.sprite.getComponent<cro::Transform>().move(-WindowSize / 2.f);

    //set the detail text alignment based on active tab
    //switch (m_uiLayout.tabBar.activeIndex)
    //{
    //default:
    //    m_uiLayout.detailsPane.text.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f });
    //    break;
    //case TabID::Controller:
    //{
    //    //this is hacky but it means the text only goes out of bounds in the edge
    //    //case where there are 4 controllers and the resolution of the window is one
    //    //of 3 obscure sizes (1176x664, 1600x1024 and 1680x1050 - that I know of)
    //    /*const float Offset = cro::GameController::getControllerCount() > 3 ? 16.f : 0.f;
    //    m_uiLayout.detailsPane.text.getComponent<cro::Transform>().setOrigin({ 0.f, Offset });*/
    //}
    //    break;
    //}
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