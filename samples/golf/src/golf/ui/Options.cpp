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

#include "../UserInterface.hpp"
#include "../SharedStateData.hpp"
#include "../GameConsts.hpp"
#include "../Career.hpp"
#include "../MessageIDs.hpp"
#include "../MenuConsts.hpp"
#include "../../WebsocketServer.hpp"
#include "../../Colordome-32.hpp"

#include <crogine/gui/Gui.hpp>

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

static inline void settingsTab(SharedStateData& sharedData, float scale)
{
    if (ImGui::BeginTabItem("Game Settings"))
    {
        ImGui::BeginChild("##settings_child", {-1.f, -1.f}, ImGuiChildFlags_NavFlattened);
        ImGui::SeparatorText("Display");
        ImGui::Checkbox("Show Flag Beacon", &sharedData.showBeacon); showTip("Display a coloured beacon at the flag visible from a distance");
        ImGui::ColorButton("##bc", getBeaconColour(sharedData.beaconColour), ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.f * scale);
        //ImGui::SliderFloat("Beacon Colour", &sharedData.beaconColour, 0.f, 1.f, "%.1f", ImGuiSliderFlags_NoInput);
        ImGui::DragFloat("Beacon Colour", &sharedData.beaconColour, 0.1f, 0.f, 1.f, "%.1f", ImGuiSliderFlags_NoInput); //slider doesn't appear to have kb input
        ImGui::Checkbox("Show Ball Trail", &sharedData.showBallTrail);
        ImGui::Checkbox("Ball Trail Uses Beacon Colour", &sharedData.trailBeaconColour); showTip("Trail colour is white if unselected");
        ImGui::Checkbox("Imperial Measurements", &sharedData.imperialMeasurements); showTip("Display measurements in Yards, Feet and Inches instead of Metres and Centimetres");
        ImGui::Checkbox("Use Large Power Bar", &sharedData.useLargePowerBar);
        ImGui::Checkbox("Use High Contrast Power Bar", &sharedData.useContrastPowerBar);
        ImGui::Checkbox("Decimated Power Bar", &sharedData.decimatePowerBar); showTip("Divide the power bar into 10 segments instead of 8");
        ImGui::Checkbox("Decimalised Distances", &sharedData.decimateDistance); showTip("Display distances in decimal units");
        ImGui::Checkbox("Show Monthly Rival", &sharedData.showRival); showTip("Display this month's leader on the scoreboard if available");
        ImGui::Checkbox("Use Follow Cam When Putting", &sharedData.puttFollowCam); showTip("Follow the ball when putting instead of the overhead view");
        ImGui::Checkbox("Zoom Follow Cam", &sharedData.zoomFollowCam);
        ImGui::Checkbox("Rotate Camera When Aiming", &sharedData.rotateCamera);
        ImGui::Checkbox("Show Lens Flare", &sharedData.useLensFlare);

        //TODO we can't actually preview this... as it's not applied to ImGui
        if (ImGui::Checkbox("Use Post Process", &sharedData.usePostProcess))
        {
            //we end up with a circular event here because the action flips
            //the setting, but then this message flips it back again...
            sharedData.usePostProcess = !sharedData.usePostProcess;
            auto* msg = cro::App::postMessage<SystemEvent>(cl::MessageID::SystemMessage);
            msg->type = SystemEvent::PostProcessToggled;
        }
        //post selection
        ImGui::SameLine();
        ImGui::SetNextItemWidth(170.f);
        if (ImGui::BeginCombo("##post_process", ShaderNames[sharedData.postProcessIndex].c_str()))
        {
            for (auto i = 0u; i < ShaderNames.size(); ++i)
            {
                const bool selected = i == sharedData.postProcessIndex;
                if (ImGui::Selectable(ShaderNames[i].c_str(), selected))
                {
                    sharedData.postProcessIndex = i;

                    auto* msg = cro::App::postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessIndexChanged;
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        //TODO tee ball colour


        //flag selection
        ImGui::NewLine();
        ImGui::Text("Flag");
        const auto s = sharedData.flagPreview.getSize() * scale;
        const auto& t = sharedData.flagPreview.getTexure();
        const auto uv = sharedData.flagPreview.getUV();
        ImGui::Image(t, { s.x, s.y }, { uv.left, uv.height }, { uv.width, uv.bottom });
        if (ImGui::Button("<"))
        {
            sharedData.flagPreview.prev();
            sharedData.flagPath = sharedData.flagPreview.getPath();
        }
        ImGui::SameLine();
        if (ImGui::Button(">"))
        {
            sharedData.flagPreview.next();
            sharedData.flagPath = sharedData.flagPreview.getPath();
        }
        ImGui::SameLine();

        static const std::vector<std::string> NumTypes = { "None","Black","White" };
        ImGui::SetNextItemWidth(114.f);
        if (ImGui::BeginCombo("Number", NumTypes[sharedData.flagText].c_str()))
        {
            for (auto i = 0u; i < NumTypes.size(); ++i)
            {
                const auto selected = i == sharedData.flagText;
                if (ImGui::Selectable(NumTypes[i].c_str(), selected))
                {
                    sharedData.flagText = i;
                    sharedData.flagPreview.setText(i);
                }
                if(selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }


        ImGui::NewLine();
        ImGui::NewLine();
        ImGui::SeparatorText("Difficulty & Behaviour");
        //ImGui::Text("Difficulty & Behaviour");
        ImGui::Checkbox("Enable Putt Assist", &sharedData.showPuttingPower); showTip("Display a small flag above the power bar when putting to estimate distance");
        ImGui::Checkbox("Precise Range Indicator", &sharedData.calculateRange); showTip("Accounts for terrain elevation and wind when drawing the range indicator instead of estimating the range");
        ImGui::Checkbox("Use Full UI", &sharedData.showMinimap); showTip("Uncheck this for a minimal UI, hiding the minimap for increased challenge");
        ImGui::Checkbox("Show In Game Tips", &sharedData.showInGameTips);
        ImGui::Checkbox("Fixed Range Putter", &sharedData.fixedPuttingRange); showTip("Disable dynamically adjusting the putting range and fix to 10m/33ft");


        ImGui::NewLine();
        ImGui::NewLine();
        ImGui::SeparatorText("Configuration");
        ImGui::Checkbox("Enable Web Socket", &sharedData.webSocket); showTip("See https://github.com/fallahn/svs for more info");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.f * scale);
        if (ImGui::InputInt("Port", &sharedData.webPort))
        {
            sharedData.webPort = std::clamp(sharedData.webPort, WebSock::MinPort, WebSock::MaxPort);
        }

        ImGui::Checkbox("Log Scores To CSV", &sharedData.logCSV); showTip("Files are saved to your user directory");
        ImGui::Checkbox("Disable Multiplayer Chat", &sharedData.blockChat);
        ImGui::Checkbox("Log Chat To Text File", &sharedData.logChat); showTip("Files are saved to your user directory");
        ImGui::Checkbox("Enable Remote Content", &sharedData.remoteContent); showTip("Allow downloading remote content in multiplayer, such as Workshop items");
        
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
                if (ImGui::Button("Cancel", {buttonWidth, 0.f}))
                {
                    ImGui::CloseCurrentPopup();
                }   
                ImGui::SameLine();
                if (ImGui::Button("OK", {buttonWidth, 0.f}))
                {
                    cb();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            };

        ImGui::NewLine();
        if (ImGui::Button("Reset Hints"))
        {
            ImGui::OpenPopup("Reset Hints?");
        }

        ImGui::SetNextWindowSize(ModalSize);
        ImGui::SetNextWindowPos(pos);
        if (ImGui::BeginPopupModal("Reset Hints?", nullptr, modalFlags))
        {
            showModal("This will reset any previously\ndisplayed hints",
                [&]()
                {
                    sharedData.showClubUpdate = true;
                    sharedData.showRosterTip = true;
                    sharedData.showTutorialTip = true;
                });
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Career"))
        {
            ImGui::OpenPopup("Reset Career?");
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
                    sharedData.gameMode = GameMode::Reset;// Tutorial;
                    sharedData.leagueTable = 0; //must reset this else league browser tries to open non-existent table
                    sharedData.leagueRoundID = 0;

                    Career::instance(sharedData).reset();

                    Tournament t;
                    t.id = 0;
                    resetTournament(t);
                    writeTournamentData(t);
                    sharedData.tournaments[0] = t;

                    t.id = 1;
                    resetTournament(t);
                    writeTournamentData(t);
                    sharedData.tournaments[1] = t;

                    //requestStackClear();
                    //requestStackPush(StateID::SplashScreen);
                    sharedData.showOptionsWindow = false;
                });
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, CD32::Colours[CD32::Red]);
        if (ImGui::Button("Reset Profile"))
        {
            ImGui::OpenPopup("Reset Profile?");
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
                    Career::instance(sharedData).reset();

                    League l(LeagueRoundID::Club, sharedData);
                    l.reset();

                    Tournament t;
                    t.id = 0;
                    resetTournament(t);
                    writeTournamentData(t);
                    t.id = 1;
                    resetTournament(t);
                    writeTournamentData(t);

                    readTournamentData(sharedData.tournaments[0]);
                    readTournamentData(sharedData.tournaments[1]);

                    cro::App::quit();
                });
        }


        ImGui::EndChild();
        ImGui::EndTabItem();
    }
}

static inline void keyboardTab(SharedStateData&)
{
    if (ImGui::BeginTabItem("Keyboard"))
    {


        ImGui::EndTabItem();
    }
}

static inline void controllerTab(SharedStateData&)
{
    if (ImGui::BeginTabItem("Controller"))
    {


        ImGui::EndTabItem();
    }
}

static inline void displayTab(SharedStateData&)
{
    if (ImGui::BeginTabItem("Display"))
    {


        ImGui::EndTabItem();
    }
}

static inline void audioTab(SharedStateData&)
{
    if (ImGui::BeginTabItem("Audio"))
    {


        ImGui::EndTabItem();
    }
}

static inline void achievementsTab()
{
    if (ImGui::BeginTabItem("Achievements"))
    {


        ImGui::EndTabItem();
    }
}

static inline void statsTab()
{
    if (ImGui::BeginTabItem("Stats"))
    {


        ImGui::EndTabItem();
    }
}

void optionsWindow(SharedStateData& sharedData)
{
    tipText = {};

    //fit to screen
    const auto size = glm::vec2(cro::App::getWindow().getSize());
    const auto scale = getViewScale();
    ImGui::SetNextWindowSize({ size.x, size.y });
    ImGui::SetNextWindowPos({ 0.f, 0.f });

    ImGui::GetStyle() = sharedData.uiScales[static_cast<std::int32_t>(scale) - 1];
    const auto HPadding = ImGui::GetStyle().ItemSpacing.x;

    //set background to semi-black
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha));

    ImGui::GetFont()->Scale *= scale;
    ImGui::PushFont(ImGui::GetFont());
    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
    //top row to contain main body
    ImGui::BeginChild("##child_main", { -1.f, size.y - (((ButtonHeight * 2.f) + (VerticalPadding * 2.f)) * scale) }, ImGuiChildFlags_NavFlattened);
    //left col for prev tab icon (eg LB)
    ImGui::BeginChild("##nav_left", { (NavColWidth * scale), -1.f }, ImGuiChildFlags_NavFlattened);
    ImGui::EndChild(); //nav_left
    ImGui::SameLine();
    //centre col for tabbed area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha));
    const float TabPaneWidth = size.x - ((((NavColWidth * scale) + (HPadding * 2.f)) * 2.f));
    ImGui::BeginChild("##tab_pane", { TabPaneWidth, -1.f }, ImGuiChildFlags_Border | ImGuiChildFlags_NavFlattened);
    ImGui::BeginTabBar("##tab_bar");
    settingsTab(sharedData, scale);
    keyboardTab(sharedData);
    controllerTab(sharedData);
    displayTab(sharedData);
    audioTab(sharedData);
    achievementsTab();
    statsTab();
    ImGui::EndTabBar();
    ImGui::EndChild(); //tab_pane
    ImGui::PopStyleColor(); //child BG
    ImGui::SameLine();
    //right col for next tab item
    ImGui::BeginChild("##nav_right", { (NavColWidth * scale), -1.f }, ImGuiChildFlags_NavFlattened);
    ImGui::EndChild();//nav_right
    ImGui::EndChild();//child_main

    ImGui::BeginChild("##child_tiptext", {-1.f, ButtonHeight * scale});
    ImGui::Text("%s", tipText.c_str());
    ImGui::EndChild();

    //bottom row for credits/HTP/close buttons
    ImGui::BeginChild("##child_bottom", {-1.f, ButtonHeight * scale}, ImGuiChildFlags_NavFlattened);
    if (ImGui::Button("How To Play", { 0.f, ButtonHeight * scale }))
    {
        sharedData.showHelp = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Credits", { 0.f, ButtonHeight * scale }))
    {
        sharedData.showOptionsWindow = false;
        //TODO raise message to request pushing credits
    }
    ImGui::SameLine();
    if (ImGui::Button("Close", { 0.f, ButtonHeight * scale }))
    {
        sharedData.showOptionsWindow = false;
    }
    ImGui::EndChild(); //child_bottom
    ImGui::End();
    
    ImGui::PopStyleColor(); //background

    ImGui::GetFont()->Scale = 1.f;
    ImGui::PopFont();

    ImGui::GetStyle() = sharedData.uiScales[0];
}