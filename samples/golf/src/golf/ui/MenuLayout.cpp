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
#include "../MessageIDs.hpp"
#include "../PlayerColours.hpp"
#include "../SharedStateData.hpp"
#include "../../Colordome-32.hpp"

#include <crogine/core/App.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIElement.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/detail/OpenGL.hpp>

static constexpr float RatioX = 4.f; //21.f
static constexpr float RatioY = 3.f; //9.f
static constexpr float MaxRatio = RatioX / RatioY;

static inline glm::vec2 getWindowSize()
{
    //this *almost* works - so perhaps we'll revisit it sometime

    /*glm::vec2 ret = cro::App::getWindow().getSize();
    const auto ratio = ret.x / ret.y;

    if (ratio > MaxRatio)
    {
        ret.x = std::floor((ret.y / RatioY) * RatioX);
    }

    return ret;*/

    return cro::App::getWindow().getSize();
}

UILayout::UILayout(std::int32_t tabCount, const SharedStateData& sd)
    : m_sharedData(sd)
{
    tabBar.items.resize(tabCount);
    menuLayout.items.resize(tabCount);
    detailsPane.tabDetails.resize(tabCount);
}

void UILayout::loadAssets(cro::ResourceCollection & resources)
{
    const auto& font = resources.fonts.get(FontID::Info);
    m_menuText.setFont(font);
    m_menuText.setCharacterSize(InfoTextSize);

    const auto& largeFont = resources.fonts.get(FontID::UI);
    m_menuTextLarge.setFont(largeFont);
    m_menuTextLarge.setCharacterSize(UITextSize);
    m_menuTextLarge.setAlignment(cro::SimpleText::Alignment::Centre);

    m_itemSlider.setPrimitiveType(GL_TRIANGLES);

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
        parseSprite("tab_active_left", m_tabActive[0]);
        parseSprite("tab_active_right", m_tabActive[1]);

        //inactive tab
        parseSprite("tab_inactive_left", m_tabInactive[0]);
        parseSprite("tab_inactive_right", m_tabInactive[1]);

        //highlight tab
        parseSprite("tab_highlight_left", m_tabHighlight[0]);
        parseSprite("tab_highlight_right", m_tabHighlight[1]);



        //background 9-patch
        parseSprite("background_centre", m_backgroundSections[BackgroundSection::Centre]);
        parseSprite("background_top", m_backgroundSections[BackgroundSection::Top]);
        parseSprite("background_left", m_backgroundSections[BackgroundSection::Left]);
        parseSprite("background_right", m_backgroundSections[BackgroundSection::Right]);
        parseSprite("background_bottom", m_backgroundSections[BackgroundSection::Bottom]);
        parseSprite("background_tl", m_backgroundSections[BackgroundSection::TL]);
        parseSprite("background_tr", m_backgroundSections[BackgroundSection::TR]);
        parseSprite("background_bl", m_backgroundSections[BackgroundSection::BL]);
        parseSprite("background_br", m_backgroundSections[BackgroundSection::BR]);



        //item backgrounds
        parseSprite("item_background_left", m_itemSection[0]);
        parseSprite("item_background_right", m_itemSection[1]);

        //item active
        parseSprite("item_active_left", m_itemActiveSection[0]);
        parseSprite("item_active_right", m_itemActiveSection[1]);

        //item active highlight
        parseSprite("item_highlight_active_left", m_itemActiveHighlightSection[0]);
        parseSprite("item_highlight_active_right", m_itemActiveHighlightSection[1]);

        //item highlight
        parseSprite("item_highlight_left", m_itemHighlightSection[0]);
        parseSprite("item_highlight_right", m_itemHighlightSection[1]);

        //item title
        parseSprite("item_title_left", m_itemTitleSection[0]);
        parseSprite("item_title_right", m_itemTitleSection[1]);


        m_itemBackground.setTexture(*uiTexture);
        m_itemBackground.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundActive.setTexture(*uiTexture);
        m_itemBackgroundActive.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundActiveHighlight.setTexture(*uiTexture);
        m_itemBackgroundActiveHighlight.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundHighlight.setTexture(*uiTexture);
        m_itemBackgroundHighlight.setPrimitiveType(GL_TRIANGLES);

        m_itemBackgroundTitle.setTexture(*uiTexture);
        m_itemBackgroundTitle.setPrimitiveType(GL_TRIANGLES);
    }
}

void UILayout::updateTabBar()
{
    const glm::vec2 WindowSize = cro::App::getWindow().getSize();// getWindowSize();

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
            const auto hovered = (i == tabBar.hoveredIndex && m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard);

            const float kludgeOffset = (2.f * viewScale);
            glm::vec2 position = { (std::round(TabWidth / 2.f) + kludgeOffset) + ((i * TabWidth) + viewScale), 0.f };
            if (active)
            {
                addQuad(position, m_tabActive[0], m_tabActive[1]);
            }
            else if (hovered)
            {
                addQuad(position, m_tabHighlight[0], m_tabHighlight[1]);
            }
            else
            {
                addQuad(position, m_tabInactive[0], m_tabInactive[1]);
            }

            //set the text
            position += glm::vec2(tabBar.background.getComponent<cro::Transform>().getPosition());
            position += WindowSize / 2.f; //screen centre
            tabBar.items[i].hitbox = { position, glm::vec2(width, height) };
            tabBar.items[i].text.getComponent<cro::Text>().setFillColour(active ? TextNormalColour :
                hovered ? CD32::Colours[CD32::Yellow] : CD32::Colours[CD32::BeigeMid]);
        }

        //add a quad to the verts as an underline
        const auto backgroundCentre = m_backgroundSections[BackgroundSection::Centre].uv;
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
            const auto hovered = (i == tabBar.hoveredIndex && m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard);

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
    //switch (tabBar.activeIndex)
    //{
    //default:
    //    detailsPane.text.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f });
    //    break;
    //case TabID::Controller:
    //{
    //    //this is hacky but it means the text only goes out of bounds in the edge
    //    //case where there are 4 controllers and the resolution of the window is one
    //    //of 3 obscure sizes (1176x664, 1600x1024 and 1680x1050 - that I know of)
    //    /*const float Offset = cro::GameController::getControllerCount() > 3 ? 16.f : 0.f;
    //    detailsPane.text.getComponent<cro::Transform>().setOrigin({ 0.f, Offset });*/
    //}
    //    break;
    //}

    resizeItemGraphics();
    updateMenuItems();
}

void UILayout::updateMenuItems()
{
    //NOTE this is all done 1:1 scale and the resulting sprite set to window scale
    auto& items = menuLayout.items[tabBar.activeIndex];
    const auto viewScale = cro::UIElementSystem::getViewScale();


    //if we didn't resize the actual size might be bigger than we expect
    //on other tabs...
    glm::vec2 renderSize = glm::vec2(menuLayout.texture.getSize());
    renderSize.x = std::round(renderSize.x * tabBar.items[tabBar.activeIndex].displayWidth);

    menuLayout.sprite.getComponent<cro::Sprite>().setTexture(menuLayout.texture.getTexture());
    menuLayout.sprite.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));

    cro::FloatRect crop = { 0.f, UI::InfoBarHeight * viewScale,
                            getWindowSize().x,
                            (tabBar.background.getComponent<cro::Transform>().getPosition().y - (UI::InfoBarHeight * viewScale)) + (getWindowSize().y / 2.f) };
    menuLayout.sprite.getComponent<cro::Drawable2D>().setCroppingArea(crop, true);

    m_menuText.setFillColour(TextNormalColour);

    constexpr float LineSpacing = 12.f;
    const auto renderItem =
        [&](Menu::Item& item, glm::vec2 pos, std::int32_t idx)
        {
            auto* background = &m_itemBackground;
            if (idx == menuLayout.hoveredIndex
                && m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
            {
                background = idx == menuLayout.itemIndex ? &m_itemBackgroundActiveHighlight : &m_itemBackgroundHighlight;
            }
            else if (idx == menuLayout.itemIndex)
            {
                background = &m_itemBackgroundActive;
            }

            if (item.displayType == Menu::Item::Heading)
            {
                m_itemBackgroundTitle.setPosition(pos);
                m_itemBackgroundTitle.draw();
            }
            else
            {
                background->setPosition(pos);
                background->draw();
            }

            if (item.texture)
            {
                if (item.displayType == Menu::Item::TextOnly)
                {
                    //achievement icon
                    m_menuQuad.setPosition(pos + glm::vec2(UI::ItemSpacing, UI::ItemSpacing));
                    pos.x += UI::ItemSpacing + UI::ItemImage.x; //moves title text over
                }
                else
                {
                    //align to the right
                    m_menuQuad.setPosition(pos + glm::vec2(tabBar.items[tabBar.activeIndex].renderWidth - (UI::ItemSpacing + UI::ItemImage.x), UI::ItemSpacing));
                }
                m_menuQuad.setTexture(*item.texture);
                m_menuQuad.setScale(UI::ItemImage / glm::vec2(item.uv.width, item.uv.height));
                m_menuQuad.setTextureRect(item.uv);
                m_menuQuad.setColour(item.previewColour);

                m_menuQuad.draw();
            }

            pos.x += UI::ItemSpacing;
            pos.y += UI::ItemHeight - LineSpacing;

            if (idx == menuLayout.itemIndex
                || idx == menuLayout.hoveredIndex)
            {
                m_menuText.setFillColour(CD32::Colours[CD32::Yellow]);
                m_menuTextLarge.setFillColour(CD32::Colours[CD32::Yellow]);
            }
            else
            {
                m_menuText.setFillColour(TextNormalColour);
                m_menuTextLarge.setFillColour(TextNormalColour);
            }

            if (item.displayType != Menu::Item::Heading)
            {
                m_menuText.setPosition(pos);
                m_menuText.setString(item.title);
                m_menuText.draw();
            }

            switch (item.displayType)
            {
            case Menu::Item::Slider:
                updateSliderGraphic(item.selectedIndex, static_cast<std::int32_t>(item.labels.size() - 1));
                m_itemSlider.setPosition({ std::floor(renderSize.x / 2.f), pos.y - 22.f/*std::floor(LineSpacing * 1.7f)*/ });
                m_itemSlider.draw();
                [[fallthrough]];
            default:
                if (item.displayType == Menu::Item::Slider)
                {
                    m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - (LineSpacing - 1.f) });
                }
                else
                {
                    m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - std::round(LineSpacing * 1.7f) });
                }

                if (item.labels.size() > 1)
                {
                    m_menuTextLarge.setString("< " + item.labels[item.selectedIndex] + " >");
                }
                else
                {
                    //this is a button
                    m_menuTextLarge.setString(item.labels[item.selectedIndex]);
                }
                m_menuTextLarge.draw();

                {
                    static constexpr float HitPadding = 4.f;
                    item.hitbox = m_menuTextLarge.getLocalBounds();
                    item.hitbox.left += m_menuTextLarge.getPosition().x;
                    item.hitbox.left -= HitPadding;
                    item.hitbox.bottom += m_menuTextLarge.getPosition().y;
                    item.hitbox.bottom -= HitPadding;
                    item.hitbox.width += (2.f * HitPadding);
                    item.hitbox.height += (2.f * HitPadding);
                }
                break;
            case Menu::Item::TextOnly:
                if (!item.description.empty())
                {
                    m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - std::round(LineSpacing * 1.7f) });
                    m_menuTextLarge.setString(item.description);
                    m_menuTextLarge.draw();
                }
                else
                {
                    m_menuText.move({ 0.f, -(LineSpacing - 1.f) });
                    m_menuText.setString(item.subTitle);
                    m_menuText.draw();
                }
                break;
            case Menu::Item::Heading:
                m_menuTextLarge.setPosition({ std::round(renderSize.x / 2.f), pos.y - std::round(LineSpacing * 1.7f) });
                m_menuTextLarge.setString(item.title);
                m_menuTextLarge.setFillColour(TextNormalColour);
                m_menuTextLarge.draw();
                break;
            }

        };

    constexpr float Stride = UI::ItemHeight + UI::ItemSpacing;
    glm::vec2 pos = { UI::ItemSpacing, renderSize.y - Stride };

    //hide the preview image and let the selection callback
    //display/update it as needed.
    detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    detailsPane.applyButton.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    updatePalettePreview(-1, -1, 0.f); //reset this allow item selected callback to update it

    menuLayout.texture.clear(cro::Colour::Transparent);
    //render current item selection to render texture
    //this includes either setting item highlight colour or rendering a highlight box
    auto i = 0;
    for (auto& item : items)
    {
        if (i == menuLayout.itemIndex)
        {
            auto txt = item.description;
            cro::Util::String::wordWrap(txt, UI::WordWrapLarge);

            detailsPane.text.getComponent<cro::Text>().setString(txt);

            const auto b = (cro::Text::getLocalBounds(detailsPane.text).width / viewScale) + UI::DetailBackgroundPadding;
            if (b > detailsPane.backgroundSize.x)
            {
                cro::Util::String::wordWrap(txt, UI::WordWrapSmall);
                detailsPane.text.getComponent<cro::Text>().setString(txt);
            }

            if (item.selected)
            {
                item.selected(item);
            }
            else if (tabBar.items[tabBar.activeIndex].sprite.getTexture())
            {
                //set this sprite if it's available
                detailsPane.image.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                detailsPane.image.getComponent<cro::Sprite>() = tabBar.items[tabBar.activeIndex].sprite;
                const auto bounds = detailsPane.image.getComponent<cro::Sprite>().getTextureBounds();
                detailsPane.image.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f,0.f });
            }
        }

        //TODO we could skip rendering if this is outside
        //the visible area, but it's not presenting a problem yet.
        renderItem(item, pos, i++);
        pos.y -= Stride;
    }

    menuLayout.texture.display();

    menuLayout.itemBox = { 0.f, 0.f, renderSize.x - (UI::ItemSpacing * 2.f), UI::ItemHeight };
    menuLayout.itemBox *= viewScale;
}

void UILayout::updateSliderGraphic(std::int32_t amt, std::int32_t total)
{
    std::vector<cro::Vertex2D> verts;

    if (total)
    {
        static constexpr float SliderWidth = 40.f;
        static constexpr float SliderHeight = 6.f;
        const float amtNorm = static_cast<float>(amt) / total;

        const float width = std::round(amtNorm * (SliderWidth * 2.f));
        static constexpr cro::Colour c = CD32::Colours[CD32::Red];
        static constexpr cro::Colour d = CD32::Colours[CD32::BlueDarkest];

        verts =
        {
            cro::Vertex2D(glm::vec2(-(SliderWidth + 1.f), SliderHeight + 1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2(-(SliderWidth + 1.f), -1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight + 1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight + 1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2(-(SliderWidth + 1.f), -1.f), CD32::Colours[CD32::TanDarkest]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), -1.f), CD32::Colours[CD32::TanDarkest]),

            cro::Vertex2D(glm::vec2(-(SliderWidth), SliderHeight), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2(-(SliderWidth), -1.f), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), SliderHeight), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2(-(SliderWidth), -1.f), CD32::Colours[CD32::Olive]),
            cro::Vertex2D(glm::vec2((SliderWidth + 1.f), -1.f), CD32::Colours[CD32::Olive]),


            cro::Vertex2D(glm::vec2(-SliderWidth, SliderHeight), c),
            cro::Vertex2D(glm::vec2(-SliderWidth, 0.f), c),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, SliderHeight), c),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, SliderHeight), c),
            cro::Vertex2D(glm::vec2(-SliderWidth, 0.f), c),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, 0.f), c),

            cro::Vertex2D(glm::vec2(-SliderWidth + width, SliderHeight), d),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, 0.f), d),
            cro::Vertex2D(glm::vec2(SliderWidth, SliderHeight), d),
            cro::Vertex2D(glm::vec2(SliderWidth, SliderHeight), d),
            cro::Vertex2D(glm::vec2(-SliderWidth + width, 0.f), d),
            cro::Vertex2D(glm::vec2(SliderWidth, 0.f), d)
        };
    }
    m_itemSlider.setVertexData(verts);
}

void UILayout::updatePalettePreview(std::int32_t paletteID, std::int32_t selectedIdx, float targetHeight)
{
    constexpr float PreviewSize = 16.f;
    constexpr float BorderSize = 1.f;

    std::vector<cro::Vertex2D> verts;

    if (paletteID > -1 && selectedIdx > -1)
    {
        const auto rows = std::min(pc::PairCounts[paletteID] / 2, pc::PairCounts[paletteID] / 4);
        const auto cols = pc::PairCounts[paletteID] / rows;

        const cro::Colour bg = cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha);
        const float top = (rows + 1) * PreviewSize;
        constexpr float bottom = PreviewSize;
        const float width = cols * PreviewSize;
        verts.emplace_back(glm::vec2(0.f, top), bg);
        verts.emplace_back(glm::vec2(0.f, bottom), bg);
        verts.emplace_back(glm::vec2(width, top), bg);

        verts.emplace_back(glm::vec2(width, top), bg);
        verts.emplace_back(glm::vec2(0.f, bottom), bg);
        verts.emplace_back(glm::vec2(width, bottom), bg);

        for (auto y = 0u; y < rows; ++y)
        {
            for (auto x = 0u; x < cols; ++x)
            {
                const auto i = y * cols + x;
                const glm::vec2 pos(x * PreviewSize, (rows * PreviewSize) - (y * PreviewSize));

                if (i == selectedIdx)
                {
                    //draw background tris first
                    verts.emplace_back(pos + glm::vec2(0.f, PreviewSize), CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos, CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos + glm::vec2(PreviewSize, PreviewSize), CD32::Colours[CD32::Yellow]);

                    verts.emplace_back(pos + glm::vec2(PreviewSize, PreviewSize), CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos, CD32::Colours[CD32::Yellow]);
                    verts.emplace_back(pos + glm::vec2(PreviewSize, 0.f), CD32::Colours[CD32::Yellow]);
                }

                verts.emplace_back(pos + glm::vec2(BorderSize, PreviewSize - BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2(BorderSize, BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2((PreviewSize - BorderSize), PreviewSize - BorderSize), pc::Palette[i]);

                verts.emplace_back(pos + glm::vec2((PreviewSize - BorderSize), PreviewSize - BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2(BorderSize, BorderSize), pc::Palette[i]);
                verts.emplace_back(pos + glm::vec2((PreviewSize - BorderSize), BorderSize), pc::Palette[i]);
            }
        }
        const float scale = cro::UIElementSystem::getViewScale();
        palettePreview.setPosition(glm::vec2(PreviewSize, std::round(targetHeight - ((((rows + 2) * PreviewSize) - (PreviewSize / 2.f)) * scale))));
        palettePreview.setScale(glm::vec2(scale));
    }
    palettePreview.setVertexData(verts);
}

void UILayout::resizeItemGraphics()
{
    //this is all done 1:1 as the ui element/nodes scale this for us

    const auto& items = menuLayout.items[tabBar.activeIndex];
    const auto viewScale = cro::UIElementSystem::getViewScale();

    //calc max texture size and resize first if necessary
    const auto texHeight = static_cast<std::uint32_t>(((UI::ItemHeight + UI::ItemSpacing) * items.size() + UI::ItemSpacing));
    const auto texWidth = static_cast<std::uint32_t>(getWindowSize().x / viewScale);

    if (!menuLayout.texture.available()
        || texWidth > menuLayout.texture.getSize().x
        || texHeight > menuLayout.texture.getSize().y)
    {
        menuLayout.texture.create(texWidth, texHeight, false);
    }


    //update all the item backgrounds based on current window size and selected tab
    //these aren't scaled by view size here - the target they're rendered to is
    tabBar.items[tabBar.activeIndex].renderWidth = static_cast<float>(menuLayout.texture.getSize().x);
    tabBar.items[tabBar.activeIndex].renderWidth = std::round(tabBar.items[tabBar.activeIndex].renderWidth * tabBar.items[tabBar.activeIndex].displayWidth);

    std::vector<cro::Vertex2D> verts;
    const auto calcVerts =
        [&](const SpriteSection& left, const SpriteSection& right)
        {
            glm::vec2 position(0.f);
            verts.emplace_back(glm::vec2(position.x, left.size.y), glm::vec2(left.uv.left, left.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + left.size.x, left.size.y), glm::vec2(left.uv.width, left.uv.height));
            verts.emplace_back(glm::vec2(position.x + left.size.x, left.size.y), glm::vec2(left.uv.width, left.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.left, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + left.size.x, position.y), glm::vec2(left.uv.width, left.uv.bottom));


            position.x += left.size.x;
            const auto centreWidth = tabBar.items[tabBar.activeIndex].renderWidth - (left.size.x * 2.f);
            verts.emplace_back(glm::vec2(position.x, left.size.y), glm::vec2(left.uv.width, left.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + centreWidth, left.size.y), glm::vec2(right.uv.left, right.uv.height));
            verts.emplace_back(glm::vec2(position.x + centreWidth, left.size.y), glm::vec2(right.uv.left, right.uv.height));
            verts.emplace_back(position, glm::vec2(left.uv.width, left.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + centreWidth, position.y), glm::vec2(right.uv.left, right.uv.bottom));


            position.x += centreWidth;
            verts.emplace_back(glm::vec2(position.x, right.size.y), glm::vec2(right.uv.left, right.uv.height));
            verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + right.size.x, right.size.y), glm::vec2(right.uv.width, right.uv.height));
            verts.emplace_back(glm::vec2(position.x + right.size.x, right.size.y), glm::vec2(right.uv.width, right.uv.height));
            verts.emplace_back(position, glm::vec2(right.uv.left, right.uv.bottom));
            verts.emplace_back(glm::vec2(position.x + right.size.x, position.y), glm::vec2(right.uv.width, right.uv.bottom));
        };

    calcVerts(m_itemSection[0], m_itemSection[1]);
    m_itemBackground.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemActiveSection[0], m_itemActiveSection[1]);
    m_itemBackgroundActive.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemActiveHighlightSection[0], m_itemActiveHighlightSection[1]);
    m_itemBackgroundActiveHighlight.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemHighlightSection[0], m_itemHighlightSection[1]);
    m_itemBackgroundHighlight.setVertexData(verts);

    verts.clear();
    calcVerts(m_itemTitleSection[0], m_itemTitleSection[1]);
    m_itemBackgroundTitle.setVertexData(verts);


    //update detail background
    glm::vec2 backgroundArea = { getWindowSize().x - (tabBar.items[tabBar.activeIndex].renderWidth * viewScale),
                        (tabBar.background.getComponent<cro::Transform>().getPosition().y - (UI::InfoBarHeight * viewScale)) + (getWindowSize().y / 2.f) };

    backgroundArea.x -= (UI::DetailBackgroundPadding * viewScale);
    backgroundArea.y -= (UI::DetailBackgroundPadding * viewScale);

    backgroundArea.x /= viewScale;
    backgroundArea.y /= viewScale;

    backgroundArea.x = std::round(backgroundArea.x / 2.f);
    backgroundArea.y = std::round(backgroundArea.y / 2.f);
    detailsPane.backgroundSize = backgroundArea * 2.f;

    const float CentreWidth = backgroundArea.x - m_backgroundSections[BackgroundSection::TL].size.x;
    const float CentreHeight = backgroundArea.y - m_backgroundSections[BackgroundSection::TL].size.y;

    verts.clear();

    const auto addQuad =
        [&](glm::vec2 position, glm::vec2 size, cro::FloatRect uv)
        {
            verts.emplace_back(glm::vec2(position.x, position.y + size.y), glm::vec2(uv.left, uv.height));
            verts.emplace_back(position, glm::vec2(uv.left, uv.bottom));
            verts.emplace_back(position + size, glm::vec2(uv.width, uv.height));

            verts.emplace_back(position + size, glm::vec2(uv.width, uv.height));
            verts.emplace_back(position, glm::vec2(uv.left, uv.bottom));
            verts.emplace_back(glm::vec2(position.x + size.x, position.y), glm::vec2(uv.width, uv.bottom));
        };

    //top left
    glm::vec2 p(-backgroundArea.x, CentreHeight);
    addQuad(p, m_backgroundSections[BackgroundSection::TL].size, m_backgroundSections[BackgroundSection::TL].uv);

    //top right
    p = { CentreWidth, CentreHeight };
    addQuad(p, m_backgroundSections[BackgroundSection::TR].size, m_backgroundSections[BackgroundSection::TR].uv);

    //bottom left
    p = { -backgroundArea.x, -backgroundArea.y };
    addQuad(p, m_backgroundSections[BackgroundSection::BL].size, m_backgroundSections[BackgroundSection::BL].uv);

    //bottom right
    p = { CentreWidth, -backgroundArea.y };
    addQuad(p, m_backgroundSections[BackgroundSection::BR].size, m_backgroundSections[BackgroundSection::BR].uv);


    //top
    p = { -CentreWidth, CentreHeight };
    glm::vec2 size = { CentreWidth * 2.f, m_backgroundSections[BackgroundSection::Top].size.y };
    cro::FloatRect uv =
    {
        m_backgroundSections[BackgroundSection::TL].uv.width,
        m_backgroundSections[BackgroundSection::TL].uv.bottom,
        m_backgroundSections[BackgroundSection::TR].uv.left,
        m_backgroundSections[BackgroundSection::TR].uv.height
    };
    addQuad(p, size, uv);

    //bottom
    p = { -CentreWidth, -backgroundArea.y };
    uv =
    {
        m_backgroundSections[BackgroundSection::BL].uv.width,
        m_backgroundSections[BackgroundSection::BL].uv.bottom,
        m_backgroundSections[BackgroundSection::BR].uv.left,
        m_backgroundSections[BackgroundSection::BR].uv.height
    };
    addQuad(p, size, uv);

    //left
    p = { -backgroundArea.x, -CentreHeight };
    size = { m_backgroundSections[BackgroundSection::Left].size.x, CentreHeight * 2.f };
    uv =
    {
        m_backgroundSections[BackgroundSection::BL].uv.left,
        m_backgroundSections[BackgroundSection::BL].uv.height,
        m_backgroundSections[BackgroundSection::TL].uv.width,
        m_backgroundSections[BackgroundSection::TL].uv.bottom
    };
    addQuad(p, size, uv);

    //right
    p = { CentreWidth, -CentreHeight };
    uv =
    {
        m_backgroundSections[BackgroundSection::BR].uv.left,
        m_backgroundSections[BackgroundSection::BR].uv.height,
        m_backgroundSections[BackgroundSection::TR].uv.width,
        m_backgroundSections[BackgroundSection::TR].uv.bottom
    };
    addQuad(p, size, uv);

    //centre
    p = { -CentreWidth, -CentreHeight };
    size = { CentreWidth * 2.f, CentreHeight * 2.f };
    addQuad(p, size, m_backgroundSections[BackgroundSection::Centre].uv);

    detailsPane.background.getComponent<cro::Drawable2D>().setVertexData(verts);


    if (resizeCallback)
    {
        resizeCallback(CentreWidth, CentreHeight);
    }
}

void UILayout::nextTab()
{
    activateTab((tabBar.activeIndex + 1) % tabBar.items.size());
    playSound(MenuSoundEvent::Activate);
}

void UILayout::prevTab()
{
    activateTab((tabBar.activeIndex + (tabBar.items.size() - 1)) % tabBar.items.size());
    playSound(MenuSoundEvent::Cancel);
}

void UILayout::activateTab(std::int32_t idx)
{
    if (detailsPane.tabDetails[tabBar.activeIndex].isValid())
    {
        detailsPane.tabDetails[tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }

    tabBar.activeIndex = idx;
    menuLayout.itemIndex = 0;

    //skip over any title items if there's room
    if (!menuLayout.items[tabBar.activeIndex].empty())
    {
        while (menuLayout.itemIndex < menuLayout.items[tabBar.activeIndex].size() - 1
            && menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex].displayType == Menu::Item::Heading)
        {
            menuLayout.itemIndex++;
        }
    }

    if (detailsPane.tabDetails[tabBar.activeIndex].isValid())
    {
        detailsPane.tabDetails[tabBar.activeIndex].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }

    if (tabBar.items[tabBar.activeIndex].selected)
    {
        tabBar.items[tabBar.activeIndex].selected();
    }

    updateTabBar();

    //set item 0 as focused
    focusToIndex();
}

void UILayout::nextItem()
{
    //reset mouse hover highlight
   menuLayout.hoveredIndex = -1;

    if (menuLayout.itemIndex < menuLayout.items[tabBar.activeIndex].size() - 1)
    {
        do
        {
            menuLayout.itemIndex++;
        } while (menuLayout.itemIndex < menuLayout.items[tabBar.activeIndex].size() - 1
            && menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex].displayType == Menu::Item::Heading);
        updateMenuItems();

        playSound(MenuSoundEvent::Switch);
    }
}

void UILayout::prevItem()
{
    //reset mouse hover highlight
    menuLayout.hoveredIndex = -1;

    //hmm these are uints so we can't use max(0)
    if (menuLayout.itemIndex > 0)
    {
        do
        {
            //also hmmm this doesn't work if the heading *is* at 0
            menuLayout.itemIndex--;
        } while (menuLayout.itemIndex > 0
            && menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex].displayType == Menu::Item::Heading);
        updateMenuItems();

        playSound(MenuSoundEvent::Switch);
    }
}

void UILayout::activateLeft()
{
    //reset mouse hover highlight
    menuLayout.hoveredIndex = -1;

    /*const */auto& item = menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex];
    //ugh activate() ought to be const but that's a whole mess of mutables.
    if (item.activateLeft())
    {
        updateMenuItems();
        playSound(item.activatedAudioID);
    }
}

void UILayout::activateRight()
{
    //reset mouse hover highlight
    menuLayout.hoveredIndex = -1;

    auto& item = menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex];
    if (item.activateRight())
    {
        updateMenuItems();
        playSound(item.activatedAudioID);
    }
}

void UILayout::activate()
{
    auto& item = menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex];
    if (item.activate())
    {
        playSound(item.activatedAudioID);
    }
}

void UILayout::checkMouseOver(glm::vec2 screenPos)
{
    std::int32_t selectedTab = -1;
    std::int32_t selectedItem = -1;

    if (screenPos.y > tabBar.background.getComponent<cro::Transform>().getWorldPosition().y)
    {
        //check the tab bar
        for (auto i = 0u; i < tabBar.items.size(); ++i)
        {
            if (tabBar.items[i].hitbox.contains(screenPos))
            {
                selectedTab = static_cast<std::int32_t>(i);
                break;
            }
        }
    }
    else
    {
        const auto viewScale = cro::UIElementSystem::getViewScale();

        //check the item list - TODO only check against visible
        const glm::vec2 WindowOffset = cro::App::getWindow().getSize() / 2u;
        glm::vec2 basePos = menuLayout.sprite.getComponent<cro::Transform>().getPosition();
        basePos += WindowOffset;
        basePos.y -= menuLayout.sprite.getComponent<cro::Transform>().getOrigin().y * viewScale;

        const auto menuHeight = static_cast<float>(menuLayout.texture.getSize().y);

        for (auto i = 0u; i < menuLayout.items[tabBar.activeIndex].size(); ++i)
        {
            //TODO skip this if it's outside the drawable area
            const float vertOffset = (menuHeight - ((i * (UI::ItemHeight + UI::ItemSpacing))) - (UI::ItemHeight + UI::ItemSpacing)) * viewScale;
            auto testBox = menuLayout.itemBox;
            testBox.left += basePos.x;
            testBox.bottom += basePos.y + vertOffset;

            if (testBox.contains(screenPos))
            {
                selectedItem = i;
                break;
            }
        }
    }


    //we may have switched from tab to item list so we still need to redraw
    if (selectedTab != tabBar.hoveredIndex)
    {
        tabBar.hoveredIndex = selectedTab;
        updateTabBar();
    }

    if (selectedItem != menuLayout.hoveredIndex)
    {
        menuLayout.hoveredIndex = selectedItem;
        updateMenuItems();
    }
}

void UILayout::doMouseClick(glm::vec2 mousePos, const cro::Camera& cam)
{
    if (tabBar.hoveredIndex != -1)
    {
        activateTab(tabBar.hoveredIndex);
        tabBar.hoveredIndex = -1;
        playSound(MenuSoundEvent::Activate);
    }
    else
    {
        if (menuLayout.hoveredIndex == -1 ||
            menuLayout.items[tabBar.activeIndex][menuLayout.hoveredIndex].displayType != Menu::Item::Heading)
        {
            if (menuLayout.hoveredIndex != -1)
            {
                menuLayout.itemIndex = menuLayout.hoveredIndex;
                menuLayout.hoveredIndex = -1;
                updateMenuItems();

                playSound(MenuSoundEvent::Activate);
            }
            //else
            {
                //this is the active item, test for activation click
                const auto testbox = menuLayout.sprite.getComponent<cro::Transform>().getWorldTransform() *
                    menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex].hitbox;
                const auto testpos = cam.pixelToCoords(mousePos);

                if (testbox.contains(testpos))
                {
                    //this seems counter intuitive but it stops mouse input
                    //automatically activating items like resolution setting
                    if (!menuLayout.items[tabBar.activeIndex][menuLayout.itemIndex].alwaysActivate)
                    {
                        activate();
                    }

                    const float xPos = testpos.x - testbox.left;
                    if (xPos < testbox.width / 2.f)
                    {
                        activateLeft();
                    }
                    else
                    {
                        activateRight();
                    }
                }
            }
        }
    }
}

std::pair<cro::FloatRect, cro::FloatRect> UILayout::scrollToTarget(float dt)
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

void UILayout::focusToIndex()
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

void playSound(std::int32_t id)
{
    cro::App::postMessage<MenuSoundEvent>(cl::MessageID::MenuSoundMessage)->type = id;
}