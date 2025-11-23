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

#include <crogine/audio/AudioDevice.hpp>
#include <crogine/util/Easings.hpp>
#include <AchievementStrings.hpp>
#include <Social.hpp>

namespace
{
    constexpr float ButtonHeight = 20.f;
    constexpr float VerticalPadding = 12.f;
    constexpr float NavColWidth = 26.f;

    constexpr float SliderWidth = 120.f;
    constexpr float LeftPadding = 10.f;

    std::string tipText;

    float getSliderSteps(float scale)
    {
        float steps = 1.f;
        for (auto i = 0; i < scale; ++i) steps /= 8.f;
        return steps;
    }
}

void showTip(const std::string& s)
{
    if (ImGui::IsItemHovered())
    {
        tipText = s;
    }
}

static inline void pushImageButtonStyle(float scale, bool rounding = true)
{
    ImGui::PushStyleColor(ImGuiCol_Button, cro::Colour::Transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cro::Colour::Transparent);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, cro::Colour::Transparent);
    if (rounding)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 0.f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.f * scale);
    }
}

static inline void popImageButtonStyle(bool rounding = true)
{
    if(rounding) ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(3);
}

static inline bool keyBinding(SharedStateData& sd, std::int32_t index, Icon& button, cro::TextureID texture, float scale)
{
    const std::string label = "Change##" + std::to_string(index);
    const auto keyString = cro::Keyboard::keyString(sd.inputBinding.keys[index]).toUtf8Char();

    bool ret = false;
    ImGui::TableNextColumn();
    if (ImGui::ImageButton(label.c_str(), texture, button.size * scale, button.getUVStart(index), button.getUVEnd(index)))
    {
        //don't early out here!
        ret = true;
    }
    button.hovered = ImGui::IsItemHovered() ? index : button.hovered;
    ImGui::TableNextColumn();
    ImGui::Text("%s", InputLabels[index].c_str());
    ImGui::TableNextColumn();
    ImGui::Text("%s", keyString.c_str());
       
    ImGui::TableNextRow();
    return ret;
}

void OptionsV2::settingsTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Game;
    if (ImGui::BeginTabItem("Game Settings", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        //hmmm almost but not quite
        //if (m_navigationContext.tabIndex != NavigationContext::TabID::Game
        //    && !active) //only play this if it was clicked, not requested...
        //{
        //    playSound(MenuSoundEvent::Activate);
        //}
        m_navigationContext.tabIndex = NavigationContext::TabID::Game;

        ImGui::BeginChild("##settings_child", { -1.f, -1.f }/*, ImGuiChildFlags_NavFlattened*/);
        ImGui::SeparatorText("Display");

        ImGui::NewLine();

        float childHeight = 368.f;
        ImGui::BeginChild("##left_pad", { LeftPadding * scale, childHeight * scale }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##right", { 0.f, childHeight * scale }, ImGuiChildFlags_NavFlattened);

        checkbox("Show Flag Beacon", &m_sharedData.showBeacon); showTip("Display a coloured beacon at the flag visible from a distance");
        ImGui::ColorButton("##bc", getBeaconColour(m_sharedData.beaconColour), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(SliderWidth * scale);

        const float steps = getSliderSteps(scale);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
        {
            ImGui::SliderFloat("Beacon Colour", &m_sharedData.beaconColour, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput);
        }
        else
        {
            ImGui::DragFloat("Beacon Colour", &m_sharedData.beaconColour, steps, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput);
        }
        if (ImGui::IsItemActive()) m_itemActive = true;
        checkbox("Show Ball Trail", &m_sharedData.showBallTrail);
        checkbox("Ball Trail Uses Beacon Colour", &m_sharedData.trailBeaconColour); showTip("Trail colour is white if unselected");
        
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
        {
            ImGui::SliderFloat("Grid Intensity", &m_sharedData.gridTransparency, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput);
        }
        else
        {
            ImGui::DragFloat("Grid Intensity", &m_sharedData.gridTransparency, steps, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput);
        }
        if (ImGui::IsItemActive()) m_itemActive = true;
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
        //TODO putting grid transparency
        ImGui::EndChild(); //right


        //flag selection
        ImGui::NewLine();
        ImGui::SeparatorText("Flag");

        ImGui::NewLine();
        childHeight = 152.f;
        ImGui::BeginChild("##left_pad0", { LeftPadding * scale, childHeight * scale }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##right0", { 0.f, childHeight * scale }, ImGuiChildFlags_NavFlattened);


        const auto s = m_flagPreview.getSize() * scale;
        const auto& t = m_flagPreview.getTexure();
        const auto uv = m_flagPreview.getUV();
        ImGui::Image(t, { s.x, s.y }, { uv.left, uv.height }, { uv.width, uv.bottom });
        auto button = m_buttonIcons[ButtonIcon::Prev];
        pushImageButtonStyle(scale,false);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 2.f * scale });
        if (ImGui::ImageButton("##<", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            m_flagPreview.prev();
            m_sharedData.flagPath = m_flagPreview.getPath();
            playSound(MenuSoundEvent::Cancel);
        }
        m_buttonIcons[ButtonIcon::Prev].hovered = ImGui::IsItemHovered() ? 0 : -1;
        ImGui::PopStyleVar();
        popImageButtonStyle(false);
        ImGui::SameLine();
        button = m_buttonIcons[ButtonIcon::Next];
        pushImageButtonStyle(scale,false);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 2.f * scale });
        if (ImGui::ImageButton("##>", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            m_flagPreview.next();
            m_sharedData.flagPath = m_flagPreview.getPath();
            playSound(MenuSoundEvent::Activate);
        }
        m_buttonIcons[ButtonIcon::Next].hovered = ImGui::IsItemHovered() ? 0 : -1;
        ImGui::PopStyleVar();
        popImageButtonStyle(false);
        ImGui::SameLine();

        static const std::vector<std::string> NumTypes = { "None","Black","White" };
        ImGui::SetNextItemWidth(114.f * scale);
        if (ImGui::BeginCombo("Number Colour", NumTypes[m_sharedData.flagText].c_str()))
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
        ImGui::EndChild(); //right0

        //ImGui::NewLine();
        ImGui::NewLine();
        ImGui::SeparatorText("Difficulty & Behaviour");
        ImGui::NewLine();
        childHeight = 122.f;
        ImGui::BeginChild("##left_pad1", { LeftPadding * scale, childHeight * scale }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##right1", { 0.f, childHeight * scale }, ImGuiChildFlags_NavFlattened);

        checkbox("Enable Putt Assist", &m_sharedData.showPuttingPower); showTip("Display a small flag above the power bar when putting to estimate distance");
        checkbox("Precise Range Indicator", &m_sharedData.calculateRange); showTip("Accounts for terrain elevation and wind instead of estimating the range");
        checkbox("Use Full UI", &m_sharedData.showMinimap); showTip("Uncheck this for a minimal UI, hiding the minimap for increased challenge");
        checkbox("Show In Game Tips", &m_sharedData.showInGameTips);
        checkbox("Fixed Range Putter", &m_sharedData.fixedPuttingRange); showTip("Disable dynamically adjusting the putting range and fix to 10m/33ft");

        ImGui::EndChild();//right1
        
        ImGui::NewLine();
        ImGui::SeparatorText("Configuration");

        ImGui::NewLine();
        childHeight = 152.f;
        ImGui::BeginChild("##left_pad2", { LeftPadding * scale, childHeight * scale }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##right2", { 0.f, childHeight * scale }, ImGuiChildFlags_NavFlattened);

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


        ImGui::NewLine();
        pushImageButtonStyle(scale);
        button = m_buttonIcons[ButtonIcon::ResetHints];
        if (ImGui::ImageButton("Reset Hints", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            ImGui::OpenPopup("Reset Hints?");
            playSound(MenuSoundEvent::Activate);
        }
        m_buttonIcons[ButtonIcon::ResetHints].hovered = ImGui::IsItemHovered() ? 0 : -1;
        popImageButtonStyle();

        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Hints?", nullptr, modalFlags))
        {
            confirmModal("This will reset any previously\ndisplayed hints",
                [&]()
                {
                    m_sharedData.showClubUpdate = true;
                    m_sharedData.showRosterTip = true;
                    m_sharedData.showTutorialTip = true;
                }, ModalSize, scale);
        }

        ImGui::SameLine();
        pushImageButtonStyle(scale);
        button = m_buttonIcons[ButtonIcon::ResetCareer];
        if (ImGui::ImageButton("##rst_crc", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            ImGui::OpenPopup("Reset Career?");
            playSound(MenuSoundEvent::Activate);
        }
        m_buttonIcons[ButtonIcon::ResetCareer].hovered = ImGui::IsItemHovered() ? 0 : -1;
        popImageButtonStyle();
        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Career?", nullptr, modalFlags))
        {
            confirmModal("Are you sure?\n\nThis will reset all of your\ncareer progress, preserving\nany unlocked items.",
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
                }, ModalSize, scale);
        }

        ImGui::SameLine();
        pushImageButtonStyle(scale);
        button = m_buttonIcons[ButtonIcon::ResetProfile];
        if (ImGui::ImageButton("##rst_prf", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            ImGui::OpenPopup("Reset Profile?");
            playSound(MenuSoundEvent::Activate);
        }
        m_buttonIcons[ButtonIcon::ResetProfile].hovered = ImGui::IsItemHovered() ? 0 : -1;
        popImageButtonStyle();

        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Profile?", nullptr, modalFlags))
        {
            confirmModal("Are You REALLY Sure?\n\nThis will reset all of your\nprogress including all of\nyour XP and quit the game.",
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
                }, ModalSize, scale);
        }


        ImGui::EndChild(); //right2

        ImGui::EndChild(); //settings_child
        
        ImGui::EndTabItem();
    }
}

void OptionsV2::keyboardTab(float scale)
{
    /*
    Note to self: Sliders must set m_itemActive with GetItemActive()
    and combos can set this directly - this stops the back button
    from closing the window when finishing an edit.
    Modals need to check m_closeModal to make sure there's no close request.
    */

    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Keyboard;
    if (ImGui::BeginTabItem("Keyboard", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Keyboard;

        ImGui::BeginChild("##keyboard_child", { -1.f, -1.f }/*, ImGuiChildFlags_NavFlattened*/);
        ImGui::SeparatorText("Key Bindings");

        ImGui::NewLine();
        const float PadHeight = 280.f;
        ImGui::BeginChild("##left_pad_keys", { LeftPadding * scale, PadHeight * scale }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##keys_right", { 0.f, PadHeight * scale }, ImGuiChildFlags_NavFlattened);

        //shared by all modals, below
        ImVec2 ModalSize = { 300.f, 120.f };
        ModalSize *= scale;

        const auto pos = (ImGui::GetIO().DisplaySize - ModalSize) / 2.f;
        const auto modalFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration;

        //ImGui::PushStyleColor(ImGuiCol_TableRowBg, CD32::Colours[CD32::Black]);
        if (ImGui::BeginTable("##table", 3, ImGuiTableFlags_NoSavedSettings, ImVec2(454.f * scale, 0.f)))
        {
            ImGui::TableSetupColumn("##button", ImGuiTableColumnFlags_WidthFixed, 50.f * scale);
            ImGui::TableSetupColumn("Assignment", ImGuiTableColumnFlags_WidthFixed, 100.f * scale);
            ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed/*, 50.f * scale*/);
            ImGui::TableHeadersRow();
            std::int32_t rebindIndex = -1;
            for (auto i = 0; i < InputBinding::Count; ++i)
            {
                pushImageButtonStyle(scale);
                if (keyBinding(m_sharedData, i, m_buttonIcons[ButtonIcon::ChangeKey], m_buttonTexture, scale))
                {
                    rebindIndex = i;
                }
                popImageButtonStyle();
            }

            ImGui::EndTable();

            if (rebindIndex != -1)
            {
                m_rebindIndex = rebindIndex;
                ImGui::OpenPopup("Rebind Key");
            }

            ImGui::SetNextWindowSize(ModalSize);
            ImGui::SetNextWindowPos(pos);
            if (ImGui::BeginPopupModal("Rebind Key", 0, modalFlags))
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, 0.f));
                ImGui::BeginChild("##child_modal", { -1.f, ModalSize.y - (40.f * scale) }, ImGuiChildFlags_NavFlattened);
                ImGui::Text("Press Any Key or ESC to Cancel");

                if (!m_rebindMessage.empty())
                {
                    ImGui::NewLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, TextGoldColour);
                    ImGui::Text("%s", m_rebindMessage.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();

                if (m_closeModal)
                {
                    m_rebindIndex = -1;
                    m_rebindMessage.clear();
                    ImGui::CloseCurrentPopup();
                    playSound(MenuSoundEvent::Cancel);
                }

                ImGui::EndPopup();
                m_itemActive = true;
            }
        }
        //ImGui::PopStyleColor(); //table row bg

        ImGui::NewLine();

        auto button = m_buttonIcons[ButtonIcon::ResetKeybinds];
        pushImageButtonStyle(scale);
        if (ImGui::ImageButton("##Reset To Default", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            ImGui::OpenPopup("Reset Keybindings");
        }
        m_buttonIcons[ButtonIcon::ResetKeybinds].hovered = ImGui::IsItemHovered() ? 0 : -1;
        popImageButtonStyle();

        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Keybindings", 0, modalFlags))
        {
            confirmModal("Really reset keys to their default?", 
                [&]()
                {
                    const auto playerID = m_sharedData.inputBinding.playerID;
                    const auto clubset = m_sharedData.inputBinding.clubset;
                    m_sharedData.inputBinding = {};
                    m_sharedData.inputBinding.playerID = playerID;
                    m_sharedData.inputBinding.clubset = clubset; 
                }, ModalSize, scale);
        }

        ImGui::EndChild(); //keys_right
        ImGui::PushStyleColor(ImGuiCol_Text, TextGoldColour);
        ImGui::Text("Fixed Keys (Cannot be assigned):");
        ImGui::PopStyleColor();
        ImGui::BeginChild("##Fixed Keys", { -1.f, 292.f * scale }, ImGuiChildFlags_NavFlattened | ImGuiChildFlags_Border);
        ImGui::Text("Number Row:\n 1 - Drone Camera (Fairway)/Measure Putt (Green)\n 2 - Freecam\n 3 - Rotate Camera Left\n 4 - Rotate Camera Right\n 5 - Zoom Minimap\n 6 - Toggle DOF (Freecam)\n 7 - Emote(Applaud)\n 8 - Emote(Laugh)\n 9 - Emote(Happy)\n 0 - Emote(Angry)\n");
        ImGui::NewLine();
        ImGui::Text("F2 - Toggle Ball Labels\nF3 - Toggle UI\nF4 - Toggle Chat\nF5 - Take Screenshot\nF7 - Toggle Putting Grid\nF11 - Toggle Full Screen");
        ImGui::Text("Tab - Show Scores\nEscape - Open Menu");
        ImGui::EndChild(); //fixed keys

        checkbox("Enable Left Mouse as Action Button", &m_sharedData.useMouseAction);
        checkbox("Hold For Power", &m_sharedData.pressHold); showTip("Press and hold the Action button to select power, instead of 3-click");

        ImGui::EndChild(); //keyboard_child
        ImGui::EndTabItem();
    }
}

void OptionsV2::controllerTab(float scale, float parentWidth)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Controller;
    if (ImGui::BeginTabItem("Controller", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Controller;
        ImGui::BeginChild("##controller_child", { -1.f, -1.f }/*, ImGuiChildFlags_NavFlattened*/);

        //we're assuming all of the controllers are the same size
        const float controlWidth = m_controllerIcons[0].size.x * scale;
        const float controlHeight = m_controllerIcons[0].size.y * scale;

        ImGui::SeparatorText("Controller Layout");
        ImGui::NewLine();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
        ImGui::PushStyleColor(ImGuiCol_ChildBg, cro::Colour::Transparent);
        ImGui::BeginChild("##pad_left", {50.f * scale, controlHeight}, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##controller_icon", {controlWidth, controlHeight}, ImGuiChildFlags_NavFlattened | ImGuiChildFlags_Border);
        
        if (Social::isSteamdeck())
        {
            //TODO *technically* this could be an external controller
            ImGui::Image(m_controllerTexture, m_controllerIcons[ControllerIcon::Deck].size * scale,
                m_controllerIcons[ControllerIcon::Deck].uv0, m_controllerIcons[ControllerIcon::Deck].uv1);
        }
        else
        {
            if (m_sharedData.activeInput == SharedStateData::ActiveInput::PS)
            {
                ImGui::Image(m_controllerTexture, m_controllerIcons[ControllerIcon::PS].size * scale,
                    m_controllerIcons[ControllerIcon::PS].uv0, m_controllerIcons[ControllerIcon::PS].uv1);
            }
            else
            {
                const auto idx = /*(cro::GameController::getControllerCount() != 0 && cro::GameController::hasPSLayout(0)) ? ControllerIcon::PS :*/ ControllerIcon::Xbox;

                ImGui::Image(m_controllerTexture, m_controllerIcons[idx].size * scale,
                    m_controllerIcons[idx].uv0, m_controllerIcons[idx].uv1);
            }
        }
        ImGui::EndChild(); //controller_icon
        ImGui::PopStyleVar(); //window padding


        ImGui::SameLine();
        ImGui::BeginChild("##inputs", { 0.f, controlHeight + (30.f * scale)}, ImGuiChildFlags_NavFlattened);

#ifdef USE_GNS
        if (ImGui::Button("Rebind Controls"))
        {
            //open overlay to guide
            Social::showWebPage("https://steamcommunity.com/sharedfiles/filedetails/?id=3445947141");
        }
#endif
        const float steps = getSliderSteps(scale);
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
        {
            ImGui::SliderFloat("Sensitivity", &m_sharedData.mouseSpeed, 0.5f, 2.f, "%.2f", ImGuiSliderFlags_NoInput);
        }
        else
        {
            ImGui::DragFloat("Sensitivity", &m_sharedData.mouseSpeed, steps, 0.5f, 2.f, "%.2f", ImGuiSliderFlags_NoInput);
        }
        if (ImGui::IsItemActive()) m_itemActive = true;

        static constexpr auto MinDeadZone = -3000;
        static constexpr auto MaxDeadzone = 24000;
        float distance = static_cast<float>((cro::GameController::LeftThumbDeadZone.getOffset() - MinDeadZone)) / (MaxDeadzone - MinDeadZone);

        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
        {
            if (ImGui::SliderFloat("Deadzone", &distance, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput))
            {
                cro::GameController::LeftThumbDeadZone.setOffset(MinDeadZone + std::int16_t(static_cast<float>(MaxDeadzone - MinDeadZone) * distance));
            }
        }
        else
        {
            if (ImGui::DragFloat("Deadzone", &distance, steps, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput))
            {
                cro::GameController::LeftThumbDeadZone.setOffset(MinDeadZone + std::int16_t(static_cast<float>(MaxDeadzone - MinDeadZone) * distance));
            }
        }
        if (ImGui::IsItemActive()) m_itemActive = true;
        checkbox("Invert X", &m_sharedData.invertX); showTip("Invert the controller X axis when in camera mode");
        checkbox("Invert Y", &m_sharedData.invertY); showTip("Invert the controller Y axis when in camera mode");
        checkbox("Enable Swingput", &m_sharedData.useSwingput); showTip("Use the Thumbstick to swing whilst holding one of the triggers");
        checkbox("Hold For Power", &m_sharedData.pressHold); showTip("Press and hold the Action button to select power, instead of 3-click");
        
        //rats.
        bool rumble = m_sharedData.enableRumble != 0;
        if (ImGui::Checkbox("Use Vibration", &rumble))
        {
            playSound(MenuSoundEvent::Activate);
            m_sharedData.enableRumble = rumble ? 1 : 0;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(); //childbg


        for (auto i = 0; i < cro::GameController::getControllerCount(); ++i)
        {
            const auto col = m_controllerStates[i] ? ImVec4(1.f, 0.f, 0.f, 1.f) : ImVec4(0.f, 0.f, 0.f, 1.f);
            const auto buttSize = ImVec2({ 16.f * scale, 16.f * scale });
            const auto idStr = std::to_string(i);
            
            
            //if (cro::GameController::getControllerCount() > 1)
            //{
            //    //re-order
            //    std::string l = "Up##" + idStr;
            //    if (ImGui::Button(l.c_str())) //this moves up on the screen but down in index...
            //    {
            //        cro::GameController::moveControllerIndexDown(i);
            //    }
            //    ImGui::SameLine();
            //    l = "Down##" + idStr;
            //    if (ImGui::Button(l.c_str()))
            //    {
            //        cro::GameController::moveControllerIndexUp(i);
            //    }
            //    ImGui::SameLine();
            //}            
            
            const auto id = "##b" + idStr;

            ImGui::ColorButton(id.c_str(), col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip, buttSize);
            ImGui::SameLine();
            ImGui::Text("%d. %s", i+1, SDL_GameControllerNameForIndex(/*cro::GameController::deviceID*/(i)));
        }

        ImGui::EndChild(); //controller_child
        ImGui::EndTabItem();
    }
}

void OptionsV2::displayTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Display;
    if (ImGui::BeginTabItem("Graphics", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Display;
        ImGui::BeginChild("##display_child", { -1.f, -1.f }/*, ImGuiChildFlags_NavFlattened*/);

        ImGui::SeparatorText("Graphics Options");
        ImGui::NewLine();

        ImGui::BeginChild("##left_pad", { LeftPadding * scale, 0.f }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##right", { 0.f, 0.f }, ImGuiChildFlags_NavFlattened);
        
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (ImGui::BeginCombo("Preset", m_presetCombo.displayNames[m_presetCombo.index].c_str()))
        {
            for (auto i = 0u; i < m_presetCombo.displayNames.size(); ++i)
            {
                const bool selected = i == m_presetCombo.index;
                if (ImGui::Selectable(m_presetCombo.displayNames[i].c_str(), selected))
                {
                    m_presetCombo.index = i;
                    applyDisplayPreset(i);

                    m_itemActive = true;
                }
            }
            m_itemActive = true;
            ImGui::EndCombo();
        }

        //anti-aliasing
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (ImGui::BeginCombo("Anti-aliasing", m_aaCombo.displayNames[m_aaCombo.index].c_str()))
        {
            for (auto i = 0u; i < m_aaCombo.displayNames.size(); ++i)
            {
                const bool selected = i == m_aaCombo.index;
                if (ImGui::Selectable(m_aaCombo.displayNames[i].c_str(), selected))
                {
                    m_aaCombo.index = i;
                    m_presetCombo.index = 3;
                    m_itemActive = true;

                    toggleAntialiasing(m_sharedData, AASamples[m_aaCombo.index] != 0, AASamples[m_aaCombo.index]);
                }
            }
            
            m_itemActive = true;
            ImGui::EndCombo();
        }


        //resolution
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (ImGui::BeginCombo("Resolution", m_resolutions.displayNames[m_resolutions.index].c_str()))
        {
            for (auto i = 0u; i < m_resolutions.displayNames.size(); ++i)
            {
                const bool selected = i == m_resolutions.index;
                if (ImGui::Selectable(m_resolutions.displayNames[i].c_str(), selected))
                {
                    m_resolutions.index = i;

                    cro::App::getWindow().setSize(m_sharedData.resolutions[m_resolutions.index]);
                }
            }
            
            m_itemActive = true;
            ImGui::EndCombo();
        }
        

        const auto updateFOV =
            []() 
            {
                //raise a window resize message to trigger callbacks
                auto size = cro::App::getWindow().getSize();
                auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
                msg->data0 = size.x;
                msg->data1 = size.y;
                msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;
            };

        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
        {
            if (ImGui::SliderFloat("FOV", &m_sharedData.fov, MinFOV, MaxFOV, "%.2f", ImGuiSliderFlags_NoInput))
            {
                //we need to raise a message here to say it changed and actually apply it
                updateFOV();
            }
        }
        else
        {
            const auto steps = getSliderSteps(scale);
            if (ImGui::DragFloat("FOV", &m_sharedData.fov, steps, MinFOV, MaxFOV, "%.2f", ImGuiSliderFlags_NoInput))
            {
                updateFOV();
            }
        }
        if (ImGui::IsItemActive()) m_itemActive = true;

        if (ImGui::Checkbox("Pixel Scaling", &m_sharedData.pixelScale))
        {
            //hum, we get a double-toggle here so we actually
            //un-toggle what we've just toggled so we can toggle it...
            m_sharedData.pixelScale = !m_sharedData.pixelScale;
            togglePixelScale(m_sharedData, !m_sharedData.pixelScale);
            playSound(MenuSoundEvent::Activate);
            m_presetCombo.index = 3;
        }
        showTip("Use large pixels");

        if (ImGui::Checkbox("Vertex Snapping", &m_sharedData.vertexSnap))
        {
            playSound(MenuSoundEvent::Activate);
        }
        showTip("For that retro 'wobble' - may cause Z-Fighting. Default OFF, Requires restart.");


        bool fs = cro::App::getWindow().isFullscreen();
        if (ImGui::Checkbox("Full Screen", &fs))
        {
            cro::App::getWindow().setFullScreen(fs);
            playSound(MenuSoundEvent::Activate);
        }

        fs = cro::App::getWindow().getExclusiveFullscreen();
        if (ImGui::Checkbox("Exclusive Full Screen", &fs))
        {
            cro::App::getWindow().setExclusiveFullscreen(fs);
            playSound(MenuSoundEvent::Activate);
        }
        showTip("Gives the game exclusive full-screen at any resolution, else displays a borderless window at desktop resolution");

        fs = cro::App::getWindow().getVsyncEnabled();
        if (ImGui::Checkbox("Enable V-Sync", &fs))
        {
            cro::App::getWindow().setVsyncEnabled(fs);
            playSound(MenuSoundEvent::Activate);
        }
        showTip("Synchronise the refresh rate with your display to prevent tearing");

        //tree quality
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (ImGui::BeginCombo("Tree Quality", m_treeQuality.displayNames[m_treeQuality.index].c_str()))
        {
            for (auto i = 0u; i < m_treeQuality.displayNames.size(); ++i)
            {
                const bool selected = i == m_treeQuality.index;
                if (ImGui::Selectable(m_treeQuality.displayNames[i].c_str(), selected))
                {
                    m_treeQuality.index = i;
                    m_sharedData.treeQuality = i;

                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::TreeQualityChanged;
                    m_itemActive = true;
                    m_presetCombo.index = 3;
                }
            }
            m_itemActive = true;
            ImGui::EndCombo();
        }
        showTip("Toggling Classic Trees will require a restart");

        //shadow quality
        ImGui::SetNextItemWidth(SliderWidth * scale);
        if (ImGui::BeginCombo("Shadow Quality", m_shadowQuality.displayNames[m_shadowQuality.index].c_str()))
        {
            for (auto i = 0u; i < m_shadowQuality.displayNames.size(); ++i)
            {
                const bool selected = i == m_shadowQuality.index;
                if (ImGui::Selectable(m_shadowQuality.displayNames[i].c_str(), selected))
                {
                    m_shadowQuality.index = i;
                    m_sharedData.shadowQuality = i;

                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::ShadowQualityChanged;
                    m_itemActive = true;
                    m_presetCombo.index = 3;
                }
            }
            m_itemActive = true;
            ImGui::EndCombo();
        }
        showTip("Toggling Classic shadows requires a restart and may cause visual artifacts until done so.");

        //crowd density
        ImGui::SetNextItemWidth(SliderWidth* scale);
        if (ImGui::BeginCombo("Crowd Density", m_crowdDensity.displayNames[m_crowdDensity.index].c_str()))
        {
            for (auto i = 0u; i < m_crowdDensity.displayNames.size(); ++i)
            {
                const bool selected = i == m_crowdDensity.index;
                if (ImGui::Selectable(m_crowdDensity.displayNames[i].c_str(), selected))
                {
                    m_crowdDensity.index = i;
                    m_sharedData.crowdDensity = i;

                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::CrowdDensityChanged;
                    m_itemActive = true;
                    m_presetCombo.index = 3;
                }
            }
            m_itemActive = true;
            ImGui::EndCombo();
        }
        showTip("Very high density crowds may cause a drop in performance.");

        ImGui::EndChild(); //right

        ImGui::EndChild(); //display_child
        ImGui::EndTabItem();
    }
}

void OptionsV2::audioTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Audio;
    if (ImGui::BeginTabItem("Audio", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Audio;
        ImGui::BeginChild("##audio_child", { -1.f, -1.f }/*, ImGuiChildFlags_NavFlattened*/);
        ImGui::SeparatorText("Audio Options");
        ImGui::NewLine();

        ImGui::BeginChild("##audio_pad", { LeftPadding * scale, 0.f }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##audio_right", { 0.f, 0.f }, ImGuiChildFlags_NavFlattened);

        //device selection
        if (!m_audioCombo.displayNames.empty())
        {
            if (ImGui::BeginCombo("Audio Device", m_audioCombo.displayNames[m_audioCombo.index].c_str()))
            {
                for (auto i = 0u; i < m_audioCombo.displayNames.size(); ++i)
                {
                    if (ImGui::Selectable(m_audioCombo.displayNames[i].c_str(), i == m_audioCombo.index))
                    {
                        m_audioCombo.index = i;

                        const auto& devices = cro::AudioDevice::getDeviceList();
                        if (!devices.empty())
                        {
                            cro::AudioDevice::setActiveDevice(devices[m_audioCombo.index]);
                        }
                    }
                }

                m_itemActive = true;
                ImGui::EndCombo();
            }

            ImGui::NewLine();
            ImGui::Separator();
            ImGui::NewLine();
        }

        const auto step = getSliderSteps(scale);
        auto v = cro::AudioMixer::getMasterVolume();
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
        {
            if (ImGui::SliderFloat("Master", &v, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput))
            {
                cro::AudioMixer::setMasterVolume(v);
            }
        }
        else
        {
            if (ImGui::DragFloat("Master", &v, step, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput))
            {
                cro::AudioMixer::setMasterVolume(v);
            }
        }
        if (ImGui::IsItemActive()) m_itemActive = true;

        for (auto i = 0; i < MixerChannel::Count; ++i)
        {
            auto vol = cro::AudioMixer::getVolume(i);
            if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard)
            {
                if (ImGui::SliderFloat(cro::AudioMixer::getLabel(i).c_str(), &vol, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput))
                {
                    cro::AudioMixer::setVolume(vol, i);
                }
            }
            else
            {
                if (ImGui::DragFloat(cro::AudioMixer::getLabel(i).c_str(), &vol, step, 0.f, 1.f, "%.2f", ImGuiSliderFlags_NoInput))
                {
                    cro::AudioMixer::setVolume(vol, i);
                }
            }
            if (ImGui::IsItemActive()) m_itemActive = true;
        }
        checkbox("Use Text To Speech", &m_sharedData.useTTS); showTip("Use Text To Speech to read in-game chat messages");

        ImGui::EndChild(); //audio_right
        ImGui::EndChild(); //audio_child
        ImGui::EndTabItem();
    }
}

void OptionsV2::achievementsTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Achievements;
    if (ImGui::BeginTabItem("Achievements", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Achievements;
        ImGui::BeginChild("##container", { 0.f, 0.f }/*, ImGuiChildFlags_NavFlattened*/);
        ImGui::NewLine();

        constexpr float IconSize = 32.f;
        constexpr float ChildHeight = IconSize + 20.f;
        static constexpr std::array Cols = {cro::Colour(0.f, 0.f, 0.f, 0.f), CD32::Colours[CD32::Black]};
        std::int32_t i = 0;
        for (const auto& [icon, ach] : m_achievements)
        {
            if (icon.texture)
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, Cols[i % 2]);

                const std::string index = std::to_string(i++);
                ImGui::BeginChild(("##" + index).c_str(), { 0.f, ChildHeight * scale }, ImGuiChildFlags_NavFlattened);
                ImGui::BeginChild(("##padding" + index).c_str(), { 4.f * scale, ChildHeight * scale }, ImGuiChildFlags_NavFlattened);
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild(("##image" + index).c_str(), { IconSize * scale, ChildHeight * scale }, ImGuiChildFlags_NavFlattened);
                ImGui::BeginChild(("##vpadding" + index).c_str(), { 0.f, 2.f * scale }, ImGuiChildFlags_NavFlattened);
                ImGui::EndChild();
                ImGui::Image(cro::TextureID(icon.texture->getGLHandle()), { IconSize * scale, IconSize * scale },
                    { icon.textureRect.left, icon.textureRect.bottom + icon.textureRect.height },
                    { icon.textureRect.left + icon.textureRect.width, icon.textureRect.bottom });
                ImGui::EndChild();
                ImGui::SameLine();
                ImGui::BeginChild(("##desc" + index).c_str(), { 0.f, ChildHeight * scale }, ImGuiChildFlags_NavFlattened);
                
                ImGui::PushStyleColor(ImGuiCol_Text, TextGoldColour);
                ImGui::Text("%s", ach->name.c_str());
                ImGui::PopStyleColor();
                

                //TODO we could probably pre-process this on load...
                //esp as we probably need to word-wrap these
                std::string desc = AchievementDesc[ach->id].second ?
                    ach->achieved ? AchievementDesc[ach->id].first : "Hidden"
                    : AchievementDesc[ach->id].first;
                ImGui::Text("%s", desc.c_str());

                if (ach->achieved)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, CD32::Colours[CD32::GreyLight]);
                    const auto ts = cro::SysTime::dateString(ach->timestamp);
                    ImGui::Text("Achieved: %s", ts.c_str());
                    ImGui::PopStyleColor();
                }

                ImGui::EndChild();
                ImGui::EndChild();

                ImGui::PopStyleColor();
            }
        }

        ImGui::EndChild();
        ImGui::EndTabItem();
    }
}

void OptionsV2::statsTab(float scale)
{
    const auto active = m_navigationContext.requestedTab == NavigationContext::TabID::Stats;
    if (ImGui::BeginTabItem("Stats", 0, active ? ImGuiTabItemFlags_SetSelected : 0))
    {
        m_navigationContext.tabIndex = NavigationContext::TabID::Stats;
        ImGui::BeginChild("##region", { 0.f, 0.f }/*, ImGuiChildFlags_NavFlattened*/);

        ImGui::NewLine();
        /*ImGui::BeginChild("##stat_pad", {LeftPadding * scale, 0.f}, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##stat_right", {0.f, 0.f}, ImGuiChildFlags_NavFlattened);*/

        constexpr float ChildHeight = 40.f;
        static constexpr std::array Cols = { cro::Colour(0.f, 0.f, 0.f, 0.f), CD32::Colours[CD32::Black] };
        std::int32_t i = 0;

        for (const auto* stat : m_stats)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Cols[i % 2]);
            const std::string index = std::to_string(i++);
            ImGui::BeginChild(("##" + index).c_str(), { 0.f, ChildHeight * scale }, ImGuiChildFlags_NavFlattened);
            
            ImGui::PushStyleColor(ImGuiCol_Text, TextGoldColour);
            ImGui::Text("%s", StatLabels[stat->id].c_str());
            ImGui::PopStyleColor();
            
            //like the achievements we could process this on load
            switch (StatTypes[stat->id])
            {
            default: break;
            case StatType::Float:
                ImGui::Text("%3.2f", stat->value);
                break;
            case StatType::Integer:
                ImGui::Text("%d", static_cast<std::int32_t>(stat->value));
                break;
            case StatType::Percent:
                ImGui::Text("%3.2f", stat->value * 100.f);
                break;
            case StatType::Time:
            {
                std::int32_t v = static_cast<std::int32_t>(Achievements::getStat(StatStrings[i])->value);
                auto seconds = v % 60;
                auto minutes = v / 60;
                auto hours = minutes / 60;
                minutes %= 60;

                ImGui::Text("%dh%dm%ds", hours, minutes, seconds);
            }
                break;
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        //ImGui::EndChild(); //stat_right
        ImGui::EndChild(); //region
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

        ImGui::PushStyleVar(ImGuiStyleVar_TabRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.f); //also does buttons etc

        //set background to semi-black
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha * m_animationProgress));

        ImGui::GetFont()->Scale *= scale;
        ImGui::PushFont(ImGui::GetFont());
        ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
        //top row to contain main body
        ImGui::BeginChild("##child_main", { -1.f, size.y - (((ButtonHeight * 2.f) + (VerticalPadding * 2.f)) * scale) }, ImGuiChildFlags_NavFlattened);
        //left col for prev tab icon (eg LB)
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.f, 0.f });
        ImGui::BeginChild("##nav_left", { (NavColWidth * scale), -1.f }, ImGuiChildFlags_NavFlattened);
        const auto buttonWidth = (m_navIcons[NavIcon::PSPrev].size.x * scale);
        ImGui::BeginChild("##pad_left", { (NavColWidth * scale) - buttonWidth, 0.f }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        
        if (m_sharedData.activeInput == SharedStateData::ActiveInput::PS)
        {
            ImGui::SameLine();
            ImGui::Image(m_navTexture, m_navIcons[NavIcon::PSPrev].size * scale, m_navIcons[NavIcon::PSPrev].uv0, m_navIcons[NavIcon::PSPrev].uv1);
        }
        else if (m_sharedData.activeInput == SharedStateData::ActiveInput::XBox)
        {
            ImGui::SameLine();
            ImGui::Image(m_navTexture, m_navIcons[NavIcon::XBPrev].size * scale, m_navIcons[NavIcon::XBPrev].uv0, m_navIcons[NavIcon::XBPrev].uv1);
        }
        ImGui::EndChild(); //nav_left
        ImGui::PopStyleVar();//item spacing
        ImGui::SameLine();
        //centre col for tabbed area
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha * m_animationProgress));
        const float TabPaneWidth = size.x - ((((NavColWidth * scale) + (HPadding * 2.f)) * 2.f));
        ImGui::BeginChild("##tab_pane", { TabPaneWidth, -1.f }, ImGuiChildFlags_Border | ImGuiChildFlags_NavFlattened);
        ImGui::BeginTabBar("##tab_bar");
        settingsTab(scale);
        keyboardTab(scale);
        controllerTab(scale, TabPaneWidth);
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
        
        const auto padWidth = (m_buttonIcons[ButtonIcon::HowToPlay].size.x +
            m_buttonIcons[ButtonIcon::Credits].size.x +
            (m_buttonIcons[ButtonIcon::Close].size.x * 1.8f)) * scale;
        ImGui::BeginChild("##button_pad_left", { size.x - padWidth, 0.f }, ImGuiChildFlags_NavFlattened);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("##button_right", { 0.f, 0.f }, ImGuiChildFlags_NavFlattened);

        auto button = m_buttonIcons[ButtonIcon::HowToPlay];
        pushImageButtonStyle(scale);
        if (ImGui::ImageButton("How To Play", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            m_sharedData.showHelp = true;
            playSound(MenuSoundEvent::Activate);
        }
        popImageButtonStyle();
        m_buttonIcons[ButtonIcon::HowToPlay].hovered = ImGui::IsItemHovered() ? 0 : -1;
        ImGui::SameLine();
        button = m_buttonIcons[ButtonIcon::Credits];
        pushImageButtonStyle(scale);
        if (ImGui::ImageButton("Credits", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            requestStackPop();
            requestStackPush(StateID::Credits);
            playSound(MenuSoundEvent::Activate);
        }
        popImageButtonStyle();
        m_buttonIcons[ButtonIcon::Credits].hovered = ImGui::IsItemHovered() ? 0 : -1;
        ImGui::SameLine();
        button = m_buttonIcons[ButtonIcon::Close];
        pushImageButtonStyle(scale);
        if (ImGui::ImageButton("Close", m_buttonTexture, button.size * scale, button.getUVStart(), button.getUVEnd()))
        {
            playSound(MenuSoundEvent::Cancel);
            m_animationTarget = 0.f;
        }
        popImageButtonStyle();
        m_buttonIcons[ButtonIcon::Close].hovered = ImGui::IsItemHovered() ? 0 : -1;
        
        ImGui::EndChild(); //button_right
        ImGui::EndChild(); //child_bottom
        ImGui::End();

        ImGui::PopStyleColor(); //background
        ImGui::PopStyleVar(3); //tab rounding, scroll bar rounding, frame rounding

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

        const auto hoveredID = ImGui::getHoveredID();
        if (m_prevHovered != hoveredID)
        {
            if (m_sharedData.activeInput == SharedStateData::ActiveInput::Keyboard
                && hoveredID != 0)
            {
                playSound(MenuSoundEvent::Switch);
            }
            m_prevHovered = hoveredID;
        }
    }
}