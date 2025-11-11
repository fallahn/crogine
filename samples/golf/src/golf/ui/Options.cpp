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
#include "../../Colordome-32.hpp"

#include <crogine/gui/Gui.hpp>

namespace
{
    constexpr float ButtonHeight = 20.f;
    constexpr float VerticalPadding = 12.f;
    constexpr float NavColWidth = 60.f;
}

void showTip(const std::string& s)
{
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::Text("%s", s.c_str());
        ImGui::EndTooltip();
    }
}


static inline void settingsTab(SharedStateData& sharedData)
{
    if (ImGui::BeginTabItem("Game Settings"))
    {
        ImGui::Text("Display");
        ImGui::Checkbox("Show Flag Beacon", &sharedData.showBeacon); showTip("Display a coloured beacon at the flag\nvisible from a distance");
        //TODO beacon colour
        ImGui::Checkbox("Show Ball Trail", &sharedData.showBallTrail);
        //TODO trail colour (beacon or white)
        ImGui::Checkbox("Imperial Measurements", &sharedData.imperialMeasurements);
        ImGui::Checkbox("Use Large Power Bar", &sharedData.useLargePowerBar);
        ImGui::Checkbox("Use High Contrast Power Bar", &sharedData.useContrastPowerBar);
        ImGui::Checkbox("Decimated Power Bar", &sharedData.decimatePowerBar);
        ImGui::Checkbox("Decimalised Distances", &sharedData.decimateDistance);
        ImGui::Checkbox("Show Monthly Rival", &sharedData.showRival);
        ImGui::Checkbox("Use Follow Cam When Putting", &sharedData.puttFollowCam);
        ImGui::Checkbox("Zoom Follow Cam", &sharedData.zoomFollowCam);
        ImGui::Checkbox("Rotate Camera When Aiming", &sharedData.rotateCamera);
        ImGui::Checkbox("Show Lens Flare", &sharedData.useLensFlare);
        //TODO flag selection
        //TODO post FX selection
        //TODO tee ball colour

        ImGui::NewLine();
        ImGui::Text("Difficulty & Behaviour");
        ImGui::Checkbox("Enable Putt Assist", &sharedData.showPuttingPower);
        ImGui::Checkbox("Precise Range Indicator", &sharedData.calculateRange);
        ImGui::Checkbox("Use Minimal UI", &sharedData.showMinimap); //hmmm this needs to be inverse
        ImGui::Checkbox("Show In Game Tips", &sharedData.showInGameTips);
        ImGui::Checkbox("Fixed Range Putter", &sharedData.fixedPuttingRange);


        ImGui::NewLine();
        ImGui::Text("Configuration");
        ImGui::Checkbox("Enable Web Socket", &sharedData.webSocket);
        //TODO socket port
        ImGui::Checkbox("Log Scores To CSV", &sharedData.logCSV);
        ImGui::Checkbox("Disable Multiplayer Chat", &sharedData.blockChat);
        ImGui::Checkbox("Log Chat To Text File", &sharedData.logChat);
        ImGui::Checkbox("Enable Remote Content", &sharedData.remoteContent);
        //TODO reset buttons

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

static inline void achievementsTab() {}
static inline void statsTab() {}

void optionsWindow(SharedStateData& sharedData)
{
    //fit to screen
    const auto size = glm::vec2(cro::App::getWindow().getSize());
    const auto scale = getViewScale();
    ImGui::SetNextWindowSize({ size.x, size.y });
    ImGui::SetNextWindowPos({ 0.f, 0.f });

    const auto HPadding = ImGui::GetStyle().ItemSpacing.x;

    //set background to semi-black
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha));

    ImGui::GetFont()->Scale *= scale;
    ImGui::PushFont(ImGui::GetFont());
    ImGui::Begin("Options", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBringToFrontOnFocus);
    //top row to contain main body
    ImGui::BeginChild("##child_main", { -1.f, size.y - ((ButtonHeight + (VerticalPadding * 2.f)) * scale) }, ImGuiChildFlags_NavFlattened);
    //left col for prev tab icon (eg LB)
    ImGui::BeginChild("##nav_left", { (NavColWidth * scale), -1.f }, ImGuiChildFlags_NavFlattened);
    ImGui::EndChild(); //nav_left
    ImGui::SameLine();
    //centre col for tabbed area
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.f, 0.f, 0.f, BackgroundAlpha)/*CD32::Colours[CD32::Brown]*/);
    ImGui::BeginChild("##tab_pane", { size.x - (((NavColWidth + (HPadding * 2.f)) * 2.f) * scale), -1.f }, ImGuiChildFlags_Border | ImGuiChildFlags_NavFlattened);
    ImGui::BeginTabBar("##tab_bar");
    settingsTab(sharedData);
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

    //bottom row for credits/HTP/close buttons
    ImGui::BeginChild("##child_bottom", {-1.f, -1.f}, ImGuiChildFlags_NavFlattened);
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
}