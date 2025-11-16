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

#include "../OptionsV2.hpp"
#include "../SharedStateData.hpp"
#include "../GameConsts.hpp"
#include "../Career.hpp"
#include "../MessageIDs.hpp"
#include "../MenuConsts.hpp"
#include "../../WebsocketServer.hpp"
#include "../../Colordome-32.hpp"

#include <crogine/util/Easings.hpp>

namespace
{
    constexpr float ButtonHeight = 20.f;
    constexpr float VerticalPadding = 12.f;
    constexpr float NavColWidth = 40.f;

    std::string tipText;
}

 void showTip(const std::string& s)
{
    if (ImGui::IsItemHovered())
    {
        tipText = s;
    }
}

void OptionsV2::settingsTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Game;
    if (ImGui::BeginTabItem("Game Settings", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Game;

        ImGui::BeginChild("##settings_child", {-1.f, -1.f}, ImGuiChildFlags_NavFlattened);
        ImGui::SeparatorText("Display");
        checkbox("Show Flag Beacon", &m_sharedData.showBeacon); showTip("Display a coloured beacon at the flag visible from a distance");
        ImGui::ColorButton("##bc", getBeaconColour(m_sharedData.beaconColour), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.f * scale);

        float steps = 1.f;
        for (auto i = 0; i < scale; ++i) steps /= 8.f;
        ImGui::DragFloat("Beacon Colour", &m_sharedData.beaconColour, steps, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput); //slider doesn't appear to have kb input
        if (ImGui::IsItemActive()) m_itemActive = true;
        checkbox("Show Ball Trail", &m_sharedData.showBallTrail);
        checkbox("Ball Trail Uses Beacon Colour", &m_sharedData.trailBeaconColour); showTip("Trail colour is white if unselected");
        checkbox("Imperial Measurements", &m_sharedData.imperialMeasurements); showTip("Display measurements in Yards, Feet and Inches instead of Metres and Centimetres");
        checkbox("Use Large Power Bar", &m_sharedData.useLargePowerBar);
        checkbox("Use High Contrast Power Bar", &m_sharedData.useContrastPowerBar);
        checkbox("Decimated Power Bar", &m_sharedData.decimatePowerBar); showTip("Divide the power bar into 10 segments instead of 8");
        checkbox("Decimalised Distances", &m_sharedData.decimateDistance); showTip("Display distances in decimal units");
        checkbox("Show Monthly Rival", &m_sharedData.showRival); showTip("Display this month's leader on the scoreboard if available");
        checkbox("Use Follow Cam When Putting", &m_sharedData.puttFollowCam); showTip("Follow the ball when putting instead of the overhead view");
        checkbox("Zoom Follow Cam", &m_sharedData.zoomFollowCam);
        checkbox("Rotate Camera When Aiming", &m_sharedData.rotateCamera);
        checkbox("Show Lens Flare", &m_sharedData.useLensFlare);

        //TODO we can't actually preview this... as it's not applied to ImGui
        if (ImGui::Checkbox("Use Post Process", &m_sharedData.usePostProcess))
        {
            //we end up with a circular event here because the action flips
            //the setting, but then this message flips it back again...
            m_sharedData.usePostProcess = !m_sharedData.usePostProcess;
            auto* msg = cro::App::postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::PostProcessToggled;

            playSound(MenuSoundEvent::Activate);
        }
        //post selection
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.f * scale);
        if (ImGui::BeginCombo("##post_process", ShaderNames[m_sharedData.postProcessIndex].c_str()))
        {
            for (auto i = 0u; i < ShaderNames.size(); ++i)
            {
                const bool selected = i == m_sharedData.postProcessIndex;
                if (ImGui::Selectable(ShaderNames[i].c_str(), selected))
                {
                    m_sharedData.postProcessIndex = i;

                    auto* msg = cro::App::postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessIndexChanged;

                    playSound(MenuSoundEvent::Activate);
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            m_itemActive = true;
            ImGui::EndCombo();
        }

        //TODO tee ball colour


        //flag selection
        ImGui::NewLine();
        ImGui::Text("Flag");
        const auto s = m_flagPreview.getSize() * scale;
        const auto& t = m_flagPreview.getTexure();
        const auto uv = m_flagPreview.getUV();
        ImGui::Image(t, { s.x, s.y }, { uv.left, uv.height }, { uv.width, uv.bottom });
        if (ImGui::Button("<"))
        {
            m_flagPreview.prev();
            m_sharedData.flagPath = m_flagPreview.getPath();
            playSound(MenuSoundEvent::Cancel);
        }
        ImGui::SameLine();
        if (ImGui::Button(">"))
        {
            m_flagPreview.next();
            m_sharedData.flagPath = m_flagPreview.getPath();
            playSound(MenuSoundEvent::Activate);
        }
        ImGui::SameLine();

        static const std::vector<std::string> NumTypes = { "None","Black","White" };
        ImGui::SetNextItemWidth(114.f * scale);
        if (ImGui::BeginCombo("Number", NumTypes[m_sharedData.flagText].c_str()))
        {
            for (auto i = 0u; i < NumTypes.size(); ++i)
            {
                const auto selected = i == m_sharedData.flagText;
                if (ImGui::Selectable(NumTypes[i].c_str(), selected))
                {
                    m_sharedData.flagText = i;
                    m_flagPreview.setText(i);
                    playSound(MenuSoundEvent::Activate);
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            m_itemActive = true;
            ImGui::EndCombo();
        }


        ImGui::NewLine();
        ImGui::NewLine();
        ImGui::SeparatorText("Difficulty & Behaviour");
        checkbox("Enable Putt Assist", &m_sharedData.showPuttingPower); showTip("Display a small flag above the power bar when putting to estimate distance");
        checkbox("Precise Range Indicator", &m_sharedData.calculateRange); showTip("Accounts for terrain elevation and wind when drawing the range indicator instead of estimating the range");
        checkbox("Use Full UI", &m_sharedData.showMinimap); showTip("Uncheck this for a minimal UI, hiding the minimap for increased challenge");
        checkbox("Show In Game Tips", &m_sharedData.showInGameTips);
        checkbox("Fixed Range Putter", &m_sharedData.fixedPuttingRange); showTip("Disable dynamically adjusting the putting range and fix to 10m/33ft");


        ImGui::NewLine();
        ImGui::NewLine();
        ImGui::SeparatorText("Configuration");
        checkbox("Enable Web Socket", &m_sharedData.webSocket); showTip("See https://github.com/fallahn/svs for more info");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f * scale);
        if (ImGui::InputInt("Port", &m_sharedData.webPort))
        {
            m_sharedData.webPort = std::clamp(m_sharedData.webPort, WebSock::MinPort, WebSock::MaxPort);
            playSound(MenuSoundEvent::Activate);
        }

        checkbox("Log Scores To CSV", &m_sharedData.logCSV); showTip("Files are saved to your user directory");
        checkbox("Disable Multiplayer Chat", &m_sharedData.blockChat);
        checkbox("Log Chat To Text File", &m_sharedData.logChat); showTip("Files are saved to your user directory");
        checkbox("Enable Remote Content", &m_sharedData.remoteContent); showTip("Allow downloading remote content in multiplayer, such as Workshop items");
        
        //reset buttons
        ImVec2 ModalSize = { 300.f, 120.f };
        ModalSize *= scale;

        const auto pos = (ImGui::GetIO().DisplaySize - ModalSize) / 2.f;
        const auto modalFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration;

        const auto showModal = 
            [&](const std::string& s, std::function<void()> cb)
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::BeginChild("##child_modal", {-1.f, ModalSize.y - (40.f * scale)}, ImGuiChildFlags_NavFlattened);
                ImGui::Text("%s", s.c_str());
                ImGui::EndChild();
                ImGui::PopStyleColor();

                const auto buttonWidth = ((ModalSize.x / 2.f) - (ImGui::GetStyle().ItemSpacing.x * 1.5f));
                if (ImGui::Button("Cancel", {buttonWidth, 0.f})
                    || m_closeModal)
                {
                    ImGui::CloseCurrentPopup();
                    playSound(MenuSoundEvent::Cancel);
                }   
                ImGui::SameLine();
                if (ImGui::Button("OK", {buttonWidth, 0.f}))
                {
                    cb();
                    ImGui::CloseCurrentPopup();
                    playSound(MenuSoundEvent::Activate);
                }
                ImGui::EndPopup();

                m_itemActive = true;
            };

        ImGui::NewLine();
        if (ImGui::Button("Reset Hints"))
        {
            ImGui::OpenPopup("Reset Hints?");
            playSound(MenuSoundEvent::Activate);
        }

        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Hints?", nullptr, modalFlags))
        {
            showModal("This will reset any previously\ndisplayed hints",
                [&]()
                {
                    m_sharedData.showClubUpdate = true;
                    m_sharedData.showRosterTip = true;
                    m_sharedData.showTutorialTip = true;
                });
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Career"))
        {
            ImGui::OpenPopup("Reset Career?");
            playSound(MenuSoundEvent::Activate);
        }
        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Career?", nullptr, modalFlags))
        {
            showModal("Are you sure?\n\nThis will reset all of your\ncareer progress, preserving\nany unlocked items.",
                [&]()
                {
                    //this is a kludge which tells the
                    //menu state to remove any existing connection/server instance
                    //if for some reason we're resetting mid-game
                    m_sharedData.gameMode = GameMode::Reset;// Tutorial;
                    m_sharedData.leagueTable = 0; //must reset this else league browser tries to open non-existent table
                    m_sharedData.leagueRoundID = 0;

                    Career::instance(m_sharedData).reset();

                    Tournament t;
                    t.id = 0;
                    resetTournament(t);
                    writeTournamentData(t);
                    m_sharedData.tournaments[0] = t;

                    t.id = 1;
                    resetTournament(t);
                    writeTournamentData(t);
                    m_sharedData.tournaments[1] = t;

                    requestStackClear();
                    requestStackPush(StateID::SplashScreen);
                });
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, CD32::Colours[CD32::Red]);
        if (ImGui::Button("Reset Profile"))
        {
            ImGui::OpenPopup("Reset Profile?");
            playSound(MenuSoundEvent::Activate);
        }
        ImGui::PopStyleColor();

        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Profile?", nullptr, modalFlags))
        {
            showModal("Are You REALLY Sure?\n\nThis will reset all of your\nprogress including all of\nyour XP and quit the game.",
                [&]()
                {
                    Social::resetProfile();
                    Career::instance(m_sharedData).reset();

                    League l(LeagueRoundID::Club, m_sharedData);
                    l.reset();

                    Tournament t;
                    t.id = 0;
                    resetTournament(t);
                    writeTournamentData(t);
                    t.id = 1;
                    resetTournament(t);
                    writeTournamentData(t);

                    readTournamentData(m_sharedData.tournaments[0]);
                    readTournamentData(m_sharedData.tournaments[1]);

                    cro::App::quit();
                });
        }


        ImGui::EndChild();
        ImGui::EndTabItem();
    }
}

void OptionsV2::keyboardTab(float scale)
{
    /*
    Note to self: Sliders must set m_itemActive with GetItemActive()
    and combos can  set this directly - this stops the back button
    from closing the window when finishing an edit.
    Modals need to check m_closeModal to makes sure there's no close request.
    */

    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Keyboard;
    if (ImGui::BeginTabItem("Keyboard", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Keyboard;

        ImGui::EndTabItem();
    }
}

void OptionsV2::controllerTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Controller;
    if (ImGui::BeginTabItem("Controller", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Controller;

        ImGui::EndTabItem();
    }
}

void OptionsV2::displayTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Display;
    if (ImGui::BeginTabItem("Display", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Display;

        ImGui::EndTabItem();
    }
}

void OptionsV2::audioTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Audio;
    if (ImGui::BeginTabItem("Audio", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Audio;

        ImGui::EndTabItem();
    }
}

void OptionsV2::achievementsTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Achievements;
    if (ImGui::BeginTabItem("Achievements", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Achievements;

        ImGui::EndTabItem();
    }
}

void OptionsV2::statsTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Stats;
    if (ImGui::BeginTabItem("Stats", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Stats;

        ImGui::EndTabItem();
    }
}

void OptionsV2::optionsWindow()
{
    if (m_showOptions)
    {
        tipText = {};
        m_itemActive = false;

        //fit to screen
        const auto size = glm::vec2(cro::App::getWindow().getSize());
        const auto scale = getViewScale();
        ImGui::SetNextWindowSize({ size.x, size.y });
        ImGui::SetNextWindowPos({ 0.f, size.y * (1.f - cro::Util::Easing::easeOutCubic(m_animationProgress))});

        ImGui::GetStyle() = m_sharedData.uiScales[static_cast<std::int32_t>(scale) - 1];
        const auto HPadding = ImGui::GetStyle().ItemSpacing.x;

        //set background to semi-black
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha * m_animationProgress));

        ImGui::GetFont()->Scale *= scale;
        ImGui::PushFont(ImGui::GetFont());
        ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
        //top row to contain main body
        ImGui::BeginChild("##child_main", { -1.f, size.y - (((ButtonHeight * 2.f) + (VerticalPadding * 2.f)) * scale) }, ImGuiChildFlags_NavFlattened);
        //left col for prev tab icon (eg LB)
        ImGui::BeginChild("##nav_left", { (NavColWidth * scale), -1.f }, ImGuiChildFlags_NavFlattened);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::PS)
        {
            ImGui::Image(m_navTexture, m_navIcons[NavIcon::PSPrev].size * scale, m_navIcons[NavIcon::PSPrev].uv0, m_navIcons[NavIcon::PSPrev].uv1);
        }
        else if (m_sharedData.activeInput == SharedStateData::ActiveInput::XBox)
        {
            ImGui::Image(m_navTexture, m_navIcons[NavIcon::XBPrev].size * scale, m_navIcons[NavIcon::XBPrev].uv0, m_navIcons[NavIcon::XBPrev].uv1);
        }
        //TODO ideally I want to handle keyboard input but rendering the text at the correct
        //size with the correct font is a right faff.
        ImGui::EndChild(); //nav_left
        ImGui::SameLine();
        //centre col for tabbed area
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha * m_animationProgress));
        const float TabPaneWidth = size.x - ((((NavColWidth * scale) + (HPadding * 2.f)) * 2.f));
        ImGui::BeginChild("##tab_pane", { TabPaneWidth, -1.f }, ImGuiChildFlags_Border | ImGuiChildFlags_NavFlattened);
        ImGui::BeginTabBar("##tab_bar");
        settingsTab(scale);
        keyboardTab(scale);
        controllerTab(scale);
        displayTab(scale);
        audioTab(scale);
        achievementsTab(scale);
        statsTab(scale);
        ImGui::EndTabBar();
        ImGui::EndChild(); //tab_pane
        ImGui::PopStyleColor(); //child BG
        ImGui::SameLine();
        //right col for next tab item
        ImGui::BeginChild("##nav_right", { (NavColWidth * scale), -1.f }, ImGuiChildFlags_NavFlattened);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::PS)
        {
            ImGui::Image(m_navTexture, m_navIcons[NavIcon::PSNext].size * scale, m_navIcons[NavIcon::PSNext].uv0, m_navIcons[NavIcon::PSNext].uv1);
        }
        else if (m_sharedData.activeInput == SharedStateData::ActiveInput::XBox)
        {
            ImGui::Image(m_navTexture, m_navIcons[NavIcon::XBNext].size * scale, m_navIcons[NavIcon::XBNext].uv0, m_navIcons[NavIcon::XBNext].uv1);
        }
        ImGui::EndChild();//nav_right
        ImGui::EndChild();//child_main

        ImGui::BeginChild("##child_tiptext", { -1.f, ButtonHeight * scale });
        ImGui::Text("%s", tipText.c_str());
        ImGui::EndChild();

        //bottom row for credits/HTP/close buttons
        ImGui::BeginChild("##child_bottom", { -1.f, ButtonHeight * scale }, ImGuiChildFlags_NavFlattened);
        if (ImGui::Button("How To Play", { 0.f, ButtonHeight * scale }))
        {
            m_sharedData.showHelp = true;
            playSound(MenuSoundEvent::Activate);
        }
        ImGui::SameLine();
        if (ImGui::Button("Credits", { 0.f, ButtonHeight * scale }))
        {
            requestStackPop();
            requestStackPush(StateID::Credits);
            playSound(MenuSoundEvent::Activate);
        }
        ImGui::SameLine();
        if (ImGui::Button("Close", { 0.f, ButtonHeight * scale }))
        {
            playSound(MenuSoundEvent::Cancel);
            m_animationTarget = 0.f;
        }
        ImGui::EndChild(); //child_bottom
        ImGui::End();

        ImGui::PopStyleColor(); //background

        ImGui::GetFont()->Scale = 1.f;
        ImGui::PopFont();

        ImGui::GetStyle() = m_sharedData.uiScales[0];

        m_navigationContext.requestedTab = -1;
        m_closeModal = false;


        if (m_prevFocus != ImGui::getFocusID())
        {
            //focus changes, play a sound
            if (m_sharedData.activeInput != SharedStateData::ActiveInput::Keyboard)
            {
                playSound(MenuSoundEvent::Switch);
            }

            m_prevFocus = ImGui::getFocusID();
        }

        if (m_prevHovered != ImGui::getHoveredID())
        {
            if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
            {
                playSound(MenuSoundEvent::Switch);
            }

            m_prevHovered = ImGui::getHoveredID();
        }
    }
}