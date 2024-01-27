#pragma once

#include "golf/SharedStateData.hpp"

#include <crogine/gui/Gui.hpp>


static inline void applyImGuiStyle(SharedStateData& sd)
{
    //golf style
    ImGuiStyle& style = ImGui::GetStyle();

    style.Alpha = 1.0f;
    style.DisabledAlpha = 0.6000000238418579f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.WindowRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowMinSize = ImVec2(32.0f, 32.0f);
    style.WindowTitleAlign = ImVec2(0.5f, 0.2f);
    style.WindowMenuButtonPosition = ImGuiDir_Left;
    style.ChildRounding = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupRounding = 5.2f;
    style.PopupBorderSize = 1.0f;
    style.FramePadding = ImVec2(3.9f, 3.0f);
    style.FrameRounding = 7.5f;
    style.FrameBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
    style.CellPadding = ImVec2(4.0f, 2.0f);
    style.IndentSpacing = 21.0f;
    style.ColumnsMinSpacing = 6.0f;
    style.ScrollbarSize = 20.0f;
    style.ScrollbarRounding = 2.5f;
    style.GrabMinSize = 10.0f;
    style.GrabRounding = 3.8f;
    style.TabRounding = 4.0f;
    style.TabBorderSize = 0.0f;
    style.TabMinWidthForCloseButton = 0.0f;
    style.ColorButtonPosition = ImGuiDir_Right;
    style.ButtonTextAlign = ImVec2(0.6f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.0f);

    style.Colors[ImGuiCol_Text] = ImVec4(1.0f, 0.9725490212440491f, 0.8823529481887817f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.7843137383460999f, 0.7215686440467834f, 0.6235294342041016f, 1.0f);
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.3137255012989044f, 0.1568627506494522f, 0.1843137294054031f, 0.9f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.3137255012989044f, 0.1568627506494522f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_Border] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.4274509847164154f, 0.4274509847164154f, 0.4274509847164154f, 0.3899999856948853f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_TitleBg] = ImVec4(0.4941176474094391f, 0.4274509847164154f, 0.2156862765550613f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.2000000029802322f, 0.2470588237047195f, 0.2980392277240753f, 0.6000000238418579f);
    style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.9490196108818054f, 0.8117647171020508f, 0.3607843220233917f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.9490196108818054f, 0.8117647171020508f, 0.3607843220233917f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.9490196108818054f, 0.8117647171020508f, 0.3607843220233917f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.1882352977991104f, 0.3333333432674408f, 0.3568627536296844f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.4313725531101227f, 0.7019608020782471f, 0.615686297416687f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4313725531101227f, 0.7019608020782471f, 0.615686297416687f, 1.0f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.3960784375667572f, 0.2627451121807098f, 0.1843137294054031f, 1.0f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.4941176474094391f, 0.4274509847164154f, 0.2156862765550613f, 1.0f);
    style.Colors[ImGuiCol_Separator] = ImVec4(0.4980392158031464f, 0.4980392158031464f, 0.4980392158031464f, 0.6000000238418579f);
    style.Colors[ImGuiCol_SeparatorHovered] = ImVec4(0.6000000238418579f, 0.6000000238418579f, 0.6980392336845398f, 1.0f);
    style.Colors[ImGuiCol_SeparatorActive] = ImVec4(0.6980392336845398f, 0.6980392336845398f, 0.8980392217636108f, 1.0f);
    style.Colors[ImGuiCol_ResizeGrip] = ImVec4(1.0f, 0.9725490212440491f, 0.8823529481887817f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.7843137383460999f, 0.7215686440467834f, 0.6235294342041016f, 1.0f);
    style.Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.7843137383460999f, 0.7215686440467834f, 0.6235294342041016f, 1.0f);
    style.Colors[ImGuiCol_Tab] = ImVec4(0.1882352977991104f, 0.3333333432674408f, 0.3568627536296844f, 1.0f);
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.4313725531101227f, 0.7019608020782471f, 0.615686297416687f, 1.0f);
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.4313725531101227f, 0.7019608020782471f, 0.615686297416687f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.1882352977991104f, 0.3333333432674408f, 0.3568627536296844f, 1.0f);
    style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.1882352977991104f, 0.3333333432674408f, 0.3568627536296844f, 1.0f);
    style.Colors[ImGuiCol_PlotLines] = ImVec4(1.0f, 0.9725490212440491f, 0.8823529481887817f, 1.0f);
    style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.9490196108818054f, 0.8117647171020508f, 0.3607843220233917f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogram] = ImVec4(0.9490196108818054f, 0.8117647171020508f, 0.3607843220233917f, 1.0f);
    style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.9254902005195618f, 0.4666666686534882f, 0.239215686917305f, 1.0f);
    style.Colors[ImGuiCol_TableHeaderBg] = ImVec4(0.4941176474094391f, 0.4274509847164154f, 0.2156862765550613f, 1.0f);
    style.Colors[ImGuiCol_TableBorderStrong] = ImVec4(0.4941176474094391f, 0.4274509847164154f, 0.2156862765550613f, 1.0f);
    style.Colors[ImGuiCol_TableBorderLight] = ImVec4(0.2588235437870026f, 0.2588235437870026f, 0.2784313857555389f, 1.0f);
    style.Colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 0.9725490212440491f, 0.8823529481887817f, 1.0f);
    style.Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.4313725531101227f, 0.7450980544090271f, 0.4392156898975372f, 1.0f);
    style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.9490196108818054f, 0.8117647171020508f, 0.3607843220233917f, 1.0f);
    style.Colors[ImGuiCol_NavHighlight] = ImVec4(0.4470588266849518f, 0.4470588266849518f, 0.8980392217636108f, 0.800000011920929f);
    style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.0f, 0.9725490212440491f, 0.8823529481887817f, 1.0f);
    style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.7843137383460999f, 0.7215686440467834f, 0.6235294342041016f, 1.0f);
    style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.2000000029802322f, 0.2000000029802322f, 0.2000000029802322f, 0.3499999940395355f);


    //load specific fonts
    auto* fonts = ImGui::GetIO().Fonts;
    fonts->AddFontDefault();

    ImFontConfig config;
    config.MergeMode = true;
    config.FontBuilderFlags |= (1 << 8) | (1 << 9); /*ImGuiFreeTypeBuilderFlags_LoadColor ImGuiFreeTypeBuilderFlags_LoadBitmap*/;
    //config.OversampleH = config.OversampleV = 1;

    //expands the default glyph set - default is 32-255
    static const std::vector<ImWchar> rangesA = { 0x1, 0xFFFF, 0 }; //TODO what's the third number? Plane? Terminator?
    fonts->AddFontFromFileTTF("assets/golf/fonts/ProggyClean.ttf", 13.f, &config, rangesA.data());
    
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSans-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesCyrillic());
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSans-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesGreek());
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSans-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesVietnamese());
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSansThai-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesThai());
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSansKR-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesKorean());
    //fonts->AddFontFromFileTTF("assets/golf/fonts/ark-pixel-10px-monospaced-ko.ttf", 10.f, &config, fonts->GetGlyphRangesKorean());
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSansJP-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesJapanese());
    //fonts->AddFontFromFileTTF("assets/golf/fonts/ark-pixel-10px-monospaced-ja.ttf", 10.f, &config, fonts->GetGlyphRangesJapanese());
    fonts->AddFontFromFileTTF("assets/golf/fonts/NotoSansTC-Regular.ttf", 10.f, &config, fonts->GetGlyphRangesChineseFull());
    //fonts->AddFontFromFileTTF("assets/golf/fonts/ark-pixel-10px-monospaced-zh_cn.ttf", 10.f, &config, fonts->GetGlyphRangesChineseFull());
    
    static const std::vector<ImWchar> rangesB = { 0x231a, 0x23fe, 0x256d, 0x2bd1, 0x10000, 0x10FFFF, 0 };
    ImFontConfig configB;
    configB.FontBuilderFlags |= (1 << 8) | (1 << 9);
#ifdef _WIN32
    const std::string winPath = "C:/Windows/Fonts/seguiemj.ttf";
    if (cro::FileSystem::fileExists(winPath))
    {
        fonts->AddFontFromFileTTF(winPath.c_str(), 10.f, &config, rangesB.data());
        sd.chatFonts.buttonLarge = fonts->AddFontFromFileTTF(winPath.c_str(), 16.f, &configB, rangesB.data());
        sd.chatFonts.buttonHeight = 28.f;
    }
    else
#endif
    {
        fonts->AddFontFromFileTTF("assets/golf/fonts/NotoEmoji-Regular.ttf", 13.f, &config, rangesB.data());
        sd.chatFonts.buttonLarge = fonts->AddFontFromFileTTF("assets/golf/fonts/NotoEmoji-Regular.ttf", 18.0f, &configB, rangesB.data());
        sd.chatFonts.buttonHeight = 30.f;
    }

    fonts->Build();
}