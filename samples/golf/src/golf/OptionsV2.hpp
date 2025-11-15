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
#include <crogine/gui/GuiClient.hpp>

//TODO remove this and the options context
#include "UserInterface.hpp"

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

    void onCachedPush() override;
    void onCachedPop() override;
    void closeWindow();
    
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

    FlagPreview m_flagPreview;
    void optionsWindow();

    void settingsTab(float scale);
    void keyboardTab(float scale);
    void controllerTab(float scale);
    void displayTab(float scale);
    void audioTab(float scale);
    void achievementsTab(float scale);
    void statsTab(float scale);
};