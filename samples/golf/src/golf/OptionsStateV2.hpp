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

#pragma once

#include "../StateIDs.hpp"
#include "ui/FlagPreview.hpp"
#include "ui/MenuLayout.hpp"

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

    UILayout m_uiLayout;
    
    cro::Entity m_infoString;
    cro::Entity m_infoSprite;
    std::array<cro::FloatRect, 2u> m_infoRects = {};
    cro::Texture m_colourPreview; //TODO this is 1x1px so we could just atlas into another texture...

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
            TeeMarker,
            ZoomFlight,
            PuttFollow,
            RangeIndicator,
            Warning,
            SteamIcon,

            Count
        };
    };
    std::array<cro::Sprite, OptionIcon::Count> m_optionIcons = {};


    struct TabID final
    {
        enum
        {
            Settings, Keyboard, Controller,
            Display, Audio, Achievements,
            Stats,

            Count
        };
    };

    std::int32_t m_keybindIndex;
    std::int32_t m_keybindItemIndex; //the menu item to update
    void updateKeybind(SDL_Scancode key);
    void cancelKeybind();

    cro::String m_controllerString;
    std::array<cro::Colour, 4u> m_activityColours = {};
    void refreshControllerDevices();

    void refreshAudioDevices(Menu::Item&);
    void quitState();
};