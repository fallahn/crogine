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

#include "ui/FlagPreview.hpp"
#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/TextureResource.hpp>

static inline constexpr std::array<std::uint32_t, 4u> AASamples =
{
    0, 2, 4, 8
};

//wrapper state for the ImGui version of the Options window
class OptionsV2 final : public cro::State, public cro::GuiClient
{
public:
    OptionsV2(cro::StateStack&, cro::State::Context, struct SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Options; }

private:

    SharedStateData& m_sharedData;
    bool m_showOptions;

    float m_animationTarget;
    float m_animationProgress;
    bool m_itemActive; //set this true if a combo box etc it open to prevent closing with controller
    bool m_closeModal;
    std::int32_t m_prevFocus;
    std::int32_t m_prevHovered;

    void onCachedPush() override;
    void onCachedPop() override;
    void closeWindow();
    void playSound(std::int32_t);
    
    struct Icon final
    {
        ImVec2 size = {};
        ImVec2 uv0 = {};
        ImVec2 uv1 = {};

        //used in instances such as image buttons
        ImVec2 uv2 = {};
        ImVec2 uv3 = {};

        ImVec2 getUVStart() const
        {
            return hovered ? uv2 : uv0;
        }
        ImVec2 getUVEnd() const
        {
            return hovered ? uv3 : uv1;
        }

        bool hovered = false;
    };

    struct NavigationContext final
    {
        struct TabID final
        {
            enum
            {
                Game, Keyboard, Controller,
                Display, Audio, Achievements,
                Stats,

                Count
            };
        };

        std::int32_t tabIndex = TabID::Game;
        std::int32_t requestedTab = -1;
    }m_navigationContext;

    struct NavIcon final
    {
        enum
        {
            PSPrev, PSNext,
            XBPrev, XBNext,
            Count
        };
    };
    cro::TextureResource m_textureResource;
    cro::TextureID m_navTexture;
    std::array<Icon, NavIcon::Count> m_navIcons = {};

    struct ButtonIcon final
    {
        enum
        {
            ResetHints, ResetCareer,
            ResetProfile, HowToPlay,
            Credits, Close,
            Count
        };
    };
    cro::TextureID m_buttonTexture;
    std::array<Icon, ButtonIcon::Count> m_buttonIcons = {};

    struct ControllerIcon final
    {
        enum
        {
            Xbox, Deck, PS,
            Count
        };
    };
    cro::TextureID m_controllerTexture;
    std::array<Icon, ControllerIcon::Count> m_controllerIcons = {};

    std::int32_t m_rebindIndex;
    std::string m_rebindMessage;

    struct ComboContext final
    {
        std::vector<std::string> displayNames;
        std::size_t index = 0;
    };
    ComboContext m_presetCombo;
    ComboContext m_aaCombo;
    ComboContext m_resolutions;
    ComboContext m_treeQuality;
    ComboContext m_shadowQuality;
    ComboContext m_crowdDensity;


    FlagPreview m_flagPreview;
    void optionsWindow();

    void settingsTab(float scale);
    void keyboardTab(float scale);
    void controllerTab(float scale, float parentWidth);
    void displayTab(float scale);
    void audioTab(float scale);
    void achievementsTab(float scale);
    void statsTab(float scale);

    //shortcut to render checkbox and play sound when activated
    void checkbox(const char*, bool*);
    //note THIS INCLUDES EndPopUp()
    void confirmModal(const char*, std::function<void()>, ImVec2 size, float scale);
    void updateKeybind(SDL_Keycode key);

    void applyDisplayPreset(std::int32_t);
};