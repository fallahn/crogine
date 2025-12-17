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

#pragma once

#include "../StateIDs.hpp"
#include "ui/FlagPreview.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

struct SharedStateData;

class OptionsStateV2 final : public cro::State
{
public:
    OptionsStateV2(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Options; }

private:

    cro::Scene m_scene;
    SharedStateData& m_sharedData;

    cro::Entity m_rootNode;
    void loadAssets();
    void buildScene();

    void createSettingsItems();
    void createKeyboardItems();
    void createControllerItems();
    void createDisplayItems();
    void createAudioItems();
    void createAchievementItems();
    void createStatItems();

    void onCachedPush() override;
    void onCachedPop() override;

    std::array<cro::Clock, 4u> m_inputRepeatClocks = {};
    std::array<cro::Time, 4u> m_repeatTimes = {};
    std::array<std::uint8_t, 4u> m_controllerMasks = {};
    std::array<std::uint8_t, 4u> m_controllerPrevMasks = {};
    void resetRepeatTimer(std::int32_t, cro::Time);

    struct SpriteSection final
    {
        cro::FloatRect uv;
        glm::vec2 size = { 0.f, 0.f };
    };
    const cro::Texture* m_uiTexture;
    std::array<SpriteSection, 2u> m_tabActive = {};
    std::array<SpriteSection, 2u> m_tabInactive = {};
    std::array<SpriteSection, 2u> m_tabHighlight = {};

    struct BackgroundSection final
    {
        enum
        {
            Top, Left, Right, Centre, Bottom,
            TR, TL, BR, BL,
            Count
        };
    };
    
    std::array<SpriteSection, BackgroundSection::Count> m_backgroundSections = {};
    

    struct TabBar final
    {
        struct Item final
        {
            enum
            {
                Settings, Keyboard, Controller,
                Display, Audio, Achievements,
                Stats,

                Count
            };
            cro::Entity text;
            float displayWidth = 0.375f; //how much horizontal space items in this tab use

            enum
            {
                Left, Centre, Right
            }alignment = Left;

            cro::FloatRect hitbox; //in screen coords
            cro::Sprite sprite;
        };

        cro::Entity background;
        std::array<Item, Item::Count> items = {};
        std::size_t activeIndex = 0;

        std::int32_t hoveredIndex = -1;

        cro::Entity navLeft;
        cro::Entity navRight;

        cro::Entity navLeftSprite;
        cro::Entity navRightSprite;

        std::array<cro::FloatRect, 2U> navLeftRects = {};
        std::array<cro::FloatRect, 2U> navRightRects = {};
    }m_tabBar;

    void updateTabBar();
    void nextTab();
    void prevTab();

    struct Menu final
    {
        struct Item final
        {
            //optional image to display colour selection
            //or achievement ID
            const cro::Texture* texture = nullptr;
            cro::FloatRect uv; //pixel coords for SimpleQuad

            //display type depending on data eg float/slider etc
            enum
            {
                Default, //left/right arrows
                Slider, //represents a sliding amount
                TextOnly, //displays the description on the item
                Heading //only displays the title, with half height background
            }displayType = Default;

            //float-rect in menu space to test click against
            cro::FloatRect hitbox;

            bool wrapValue = true; //value wraps back to the beginning instead of clamping
            std::int32_t selectedIndex = 0; //currently selected entry
            std::int32_t count = 1; //number of items to cycle through when clicking
            std::vector<cro::String> labels; //display text for each setting when cycled
            cro::String title; //main display title
            cro::String subTitle; //shown below title in TextOnly items
            cro::String description; //shown when hovered

            std::function<void(const Item&)> selected; //called when selected
            std::function<void(Item&)> activated; //called when activated
            bool activateLeft()
            {
                if (count > 1)
                {
                    selectedIndex = wrapValue ?
                        (selectedIndex + (count - 1)) % count
                        : std::max(selectedIndex - 1, 0);

                    activated(*this);
                    return true;
                }
                return false;
            }

            bool activateRight()
            {
                if (count > 1)
                {
                    selectedIndex = wrapValue ? 
                        (selectedIndex + 1) % count
                        : std::min(selectedIndex + 1, count - 1);
                    activated(*this);
                    return true;
                }
                return false;
            }

            bool activate()
            {
                if (count == 1)
                {
                    activated(*this);
                    return true;
                }
                return false;
            }
        };
        std::array<std::vector<Item>, TabBar::Item::Count> items = {};

        cro::RenderTexture texture;
        cro::Entity sprite;

        std::uint32_t itemIndex = 0;
        std::int32_t hoveredIndex = -1;

        cro::FloatRect itemBox; //size is menu coords, position is updated during testing with current scroll position
    }m_menuLayout;

    cro::SimpleQuad m_menuQuad; //item image/thumb if it exists
    cro::SimpleText m_menuText;
    cro::SimpleText m_menuTextLarge;        

    std::array<SpriteSection, 2u> m_itemSection = {};
    std::array<SpriteSection, 2u> m_itemActiveSection = {};
    std::array<SpriteSection, 2u> m_itemActiveHighlightSection = {};
    std::array<SpriteSection, 2u> m_itemHighlightSection = {};
    std::array<SpriteSection, 2u> m_itemTitleSection = {};
    cro::SimpleVertexArray m_itemBackground;
    cro::SimpleVertexArray m_itemBackgroundActive;
    cro::SimpleVertexArray m_itemBackgroundActiveHighlight;
    cro::SimpleVertexArray m_itemBackgroundHighlight;
    cro::SimpleVertexArray m_itemBackgroundTitle;
    cro::SimpleVertexArray m_itemSlider;


    cro::Entity m_infoString;
    cro::Entity m_infoSprite;
    std::array<cro::FloatRect, 2u> m_infoRects = {};
    cro::Texture m_beaconPreview; //TODO this is 1x1px so we could just atlas into another texture...

    FlagPreview m_flagPreview;
    struct OptionIcon final
    {
        enum
        {
            GridDensity,
            BeaconColour,
            HighContrast,
            LargePower,
            DecimatePower,
            WidgetSpeed,
            PuttAssist,
            BallTrail,
            Warning,

            Count
        };
    };
    std::array<cro::Sprite, OptionIcon::Count> m_optionIcons = {};

    struct DetailsPane final
    {
        std::array<cro::Entity, TabBar::Item::Count> tabDetails = {};
        cro::Entity root;
        cro::Entity text;
        cro::Entity image;
        cro::Entity background;
        cro::Entity applyButton;

        //track this so we can resize items which appear within it
        //NOTE that is *without* the view scaling
        glm::vec2 backgroundSize = { 0.f, 0.f };
    }m_detailsPane;

    void resizeItemGraphics();
    void updateSliderGraphic(std::int32_t amt, std::int32_t total);

    void updateMenuItems();
    void nextItem();
    void prevItem();
    void activateLeft();
    void activateRight();
    void activate();

    void checkMouseOver(glm::vec2);
    void doMouseClick(glm::vec2);

    std::int32_t m_keybindIndex;
    std::int32_t m_keybindItemIndex; //the menu item to update
    void updateKeybind(SDL_Keycode key);
    void cancelKeybind();

    cro::String m_controllerString;
    std::array<cro::Colour, 4u> m_activityColours = {};
    void refreshControllerDevices();

    void refreshAudioDevices(Menu::Item&);
    void refreshView();
    void quitState();
};