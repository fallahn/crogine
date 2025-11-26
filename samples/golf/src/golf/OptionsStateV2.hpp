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

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>

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

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    cro::Entity m_rootNode;
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
        };

        cro::Entity background;
        std::array<Item, Item::Count> items = {};
        std::size_t activeIndex = 0;
    }m_tabBar;

    void updateTabBar();
    void nextTab();
    void prevTab();

    struct Menu final
    {
        struct Item final
        {
            //TODO optional image to display colour selection
            //or achievement ID

            //TODO display type depending on data eg float/slider etc

            //TODO float-rects in menu space to test clich against

            std::int32_t itemIndex = 0; //currently selected entry
            std::int32_t itemCount = 2; //number of items to cycle through when clicking
            std::vector<cro::String> itemLabels; //display text for each setting when cycled
            cro::String itemTitle; //main display title
            cro::String description; //shown when hovered

            std::function<void(Item&)> callback; //called when activated
            void activateLeft()
            {
                if (itemCount > 1)
                {
                    itemIndex = (itemIndex + (itemCount - 1)) % itemCount;
                    callback(*this);
                }
            }

            void activateRight()
            {
                if (itemCount > 1)
                {
                    itemIndex = (itemIndex + 1) % itemCount;
                    callback(*this);
                }
            }
        };
        std::array<std::vector<Item>, TabBar::Item::Count> items = {};

        cro::RenderTexture texture;

        std::int32_t itemIndex = 0;
    }m_menuLayout;

    void updateMenuItems();
    void nextItem();
    void prevItem();
    void activateLeft();
    void activateRight();

    void refreshView();
    void quitState();
};