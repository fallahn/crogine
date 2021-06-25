/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "LayoutState.hpp"
#include "UIConsts.hpp"
#include "SharedStateData.hpp"

#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/util/String.hpp>

namespace
{
    enum WindowID
    {
        Inspector,
        Browser,
        Info,

        Count
    };

    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(), glm::vec2())
    };
}

void LayoutState::initUI()
{
    registerWindow([&]()
        {
            drawMenuBar();
            drawInspector();
            drawBrowser();
            drawInfo();
        });

    getContext().appInstance.resetFrameTime();
    getContext().mainWindow.setTitle("Crogine Layout Editor");

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);

    //TODO load the last used workspace (keep a ref to this in the prefs file?)
}

void LayoutState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        //file menu
        if (ImGui::BeginMenu("File##Particle"))
        {
            if (ImGui::MenuItem("Open##Layout", nullptr, nullptr))
            {

            }
            if (ImGui::MenuItem("Save##Layout", nullptr, nullptr))
            {

            }

            if (ImGui::MenuItem("Save As...##Layout", nullptr, nullptr))
            {

            }

            if (getStateCount() > 1)
            {
                if (ImGui::MenuItem("Return To World Editor"))
                {
                    getContext().mainWindow.setTitle("Crogine Editor");
                    requestStackPop();
                }
            }

            if (ImGui::MenuItem("Quit##Layout", nullptr, nullptr))
            {
                cro::App::quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit##layout"))
        {
            if (ImGui::BeginMenu("Add"))
            {
                if (ImGui::MenuItem("Sprite"))
                {

                }
                
                if (ImGui::MenuItem("Text"))
                {

                }
                ImGui::EndMenu();
            }

            if (ImGui::MenuItem("Remove"))
            {
                if (cro::FileSystem::showMessageBox("", "Delete the selected node?", cro::FileSystem::YesNo, cro::FileSystem::IconType::Question))
                {
                    //TODO delete the selected node
                }
            }

            ImGui::EndMenu();
        }

        //view menu
        if (ImGui::BeginMenu("View##Layout"))
        {
            if (ImGui::MenuItem("Options", nullptr, nullptr))
            {
               // m_showPreferences = !m_showPreferences;
            }



            ImGui::EndMenu();
        }

        //workspace menu
        if (ImGui::BeginMenu("Workspace"))
        {
            if (ImGui::MenuItem("Open##workspace"))
            {

            }

            if (ImGui::MenuItem("Save##workspace"))
            {
                //TODO save background info,
                //the layout size
                //and any loaded spritesheets/fonts
                //the layout tree is saved as its own file
            }

            if (ImGui::MenuItem("Set Background"))
            {
                auto path = cro::FileSystem::openFileDialogue(m_sharedData.workingDirectory + "untitled.png", "png,jpg,bmp");
                if (!path.empty())
                {
                    if (m_backgroundTexture.loadFromFile(path))
                    {
                        //technically it's already set, just a lazy way of telling the sprite to resize
                        m_backgroundEntity.getComponent<cro::Sprite>().setTexture(m_backgroundTexture);
                        m_layoutSize = m_backgroundTexture.getSize();
                        updateView2D(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
                    }
                }
            }
            uiConst::showTipMessage("This will resize the the workspace to match the background image if possible");


            /*if (ImGui::MenuItem("Set Size"))
            {

            }*/

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }
}

void LayoutState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector##Layout", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("Scene");
        if (ImGui::BeginTabItem("Tree"))
        {


            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void LayoutState::drawBrowser()
{
    auto [pos, size] = WindowLayouts[WindowID::Browser];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser##Layout", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("##layoutBrowser");
        if (ImGui::BeginTabItem("Sprites"))
        {
            if (ImGui::Button("Open Sprite Sheet"))
            {
                //TODO add all the sprites from a sprite sheet
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove Sprites"))
            {
                //TODO warn this will remove all sprites from the
                //selected sprite's sprite sheet
            }
            uiConst::showTipMessage("This will remove all sprites which share the selected sprite sheet");

            //thumbnail window
            ImGui::BeginChild("##spriteChild");

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Fonts"))
        {
            if (ImGui::Button("Add"))
            {
                //load a font
                auto path = cro::FileSystem::openFileDialogue(m_sharedData.workingDirectory + "/untitled.ttf", "ttf,otf", true);
                if (!path.empty())
                {
                    auto files = cro::Util::String::tokenize(path, '|');
                    for (const auto& f : files)
                    {
                        loadFont(f);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove"))
            {
                //note this will only remove the thumbnail
                //until the app is reloaded
                if (m_selectedFont != 0
                    && cro::FileSystem::showMessageBox("Delete?", "Remove this font?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                {
                    m_fontThumbs.erase(m_selectedFont);
                    m_selectedFont = 0;
                }
            }

            //thumbnail window
            ImGui::BeginChild("##fontChild");

            static std::size_t lastSelected = 0;

            auto thumbSize = size;
            thumbSize.y *= uiConst::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            auto frameSize = thumbSize;
            frameSize.x += ImGui::GetStyle().FramePadding.x * uiConst::FramePadding.x;
            frameSize.y += ImGui::GetStyle().FramePadding.y * uiConst::FramePadding.y;

            std::int32_t thumbsPerRow = static_cast<std::int32_t>(std::floor(size.x / frameSize.x) - 1);
            std::int32_t count = 0;
            std::uint32_t removeID = 0;

            for (const auto& [id, thumb] : m_fontThumbs)
            {
                ImVec4 colour(0.f, 0.f, 0.f, 1.f);
                if (id == m_selectedFont)
                {
                    colour = { 1.f, 1.f, 0.f, 1.f };
                    if (lastSelected != m_selectedFont)
                    {
                        ImGui::SetScrollHereY();
                    }
                }

                ImGui::BeginChildFrame(83854 + id, { frameSize.x, frameSize.y }, ImGuiWindowFlags_NoScrollbar);

                ImGui::PushStyleColor(ImGuiCol_Border, colour);
                if (ImGui::ImageButton(thumb.texture.getTexture(), { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
                {
                    m_selectedFont = id;
                    ImGui::SetScrollHereY();
                }
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    //set payload to carry the GL id of the texture
                    ImGui::SetDragDropPayload("FONT_SRC", &id, sizeof(std::uint32_t));

                    //display preview
                    ImGui::Image(thumb.texture.getTexture(), { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f });
                    ImGui::Text("%s", thumb.name.c_str());
                    ImGui::EndDragDropSource();

                    m_selectedFont = id;
                    ImGui::SetScrollHereY();
                }

                ImGui::Text("%s", thumb.name.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", thumb.name.c_str());
                    ImGui::EndTooltip();
                }

                ImGui::EndChildFrame();

                //put on same line if we fit
                count++;
                if (count % thumbsPerRow != 0)
                {
                    ImGui::SameLine();
                }
            }

            lastSelected = m_selectedFont;

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void LayoutState::drawInfo()
{
    auto [pos, size] = WindowLayouts[WindowID::Info];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("InfoBar##Layout", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, m_messageColour);
        ImGui::Text("%s", cro::Console::getLastOutput().c_str());
        ImGui::PopStyleColor();

        ImGui::SameLine();
        auto cursor = ImGui::GetCursorPos();
        cursor.y -= 4.f;
        ImGui::SetCursorPos(cursor);

        if (ImGui::Button("More...", ImVec2(56.f, 20.f)))
        {
            cro::Console::show();
        }
    }
    ImGui::End();
}

void LayoutState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);
    WindowLayouts[WindowID::Inspector] =
        std::make_pair(glm::vec2(0.f, uiConst::TitleHeight),
            glm::vec2(width * uiConst::InspectorWidth, height - (uiConst::TitleHeight + uiConst::InfoBarHeight)));

    WindowLayouts[WindowID::Browser] =
        std::make_pair(glm::vec2(width * uiConst::InspectorWidth, height - (height * uiConst::BrowserHeight) - uiConst::InfoBarHeight),
            glm::vec2(width - (width * uiConst::InspectorWidth), height * uiConst::BrowserHeight));

    WindowLayouts[WindowID::Info] =
        std::make_pair(glm::vec2(0.f, height - uiConst::InfoBarHeight), glm::vec2(width, uiConst::InfoBarHeight));
}

void LayoutState::loadFont(const std::string& path)
{
    if (m_resources.fonts.load(m_nextResourceID, path))
    {
        m_fontThumbs.insert(std::make_pair(m_nextResourceID, FontThumb()));

        //render the preview
        auto& [texture, relPath, name] = m_fontThumbs.at(m_nextResourceID);
        texture.create(64, 64, false);
        texture.clear(cro::Colour::Cyan);
        texture.display();

        name = cro::FileSystem::getFileName(path);

        if (!m_sharedData.workingDirectory.empty() &&
            path.find(m_sharedData.workingDirectory) != std::string::npos)
        {
            relPath = path.substr(m_sharedData.workingDirectory.size() + 1);
        }
        else if (cro::FileSystem::fileExists(m_sharedData.workingDirectory + path))
        {
            relPath = path;
        }
        else
        {
            relPath = name;
        }

        m_nextResourceID++;
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed to load font.");
    }
}