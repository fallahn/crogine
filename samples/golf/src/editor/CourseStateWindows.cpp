/*-----------------------------------------------------------------------

Matt Marchant 2026
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

#include "CourseState.hpp"
#include "EditorConsts.hpp"

#include <crogine/audio/sound_system/MusicPlayer.hpp>

namespace
{

}

void CourseState::drawMenuBar()
{

    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project", "Ctrl+N"))
            {
                menuNewProject();
            }

            if (ImGui::MenuItem("Open Project"))
            {
                menuOpenProject();
            }

            if (ImGui::MenuItem("Save Project", nullptr, false, !m_projectSettings.workingDirectory.empty()))
            {
                saveProject(m_projectSettings.workingDirectory);
            }


            //if not empty history display recent projects
            if (!m_history.empty())
            {
                if (ImGui::BeginMenu("Recent"))
                {
                    for (const auto& item : m_history)
                    {
                        if (ImGui::MenuItem(item.filename().string().c_str()))
                        {
                            menuConfirmThenOpen(item);
                        }
                    }
                    ImGui::EndMenu();
                }
            }

            if (ImGui::MenuItem("Quit", "Ctrl+Q"))
            {
                confirmQuit();
            }

            ImGui::EndMenu();
        }

        const auto enabled = !m_projectSettings.id.empty();
        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Add Hole", nullptr, false, enabled && m_courseData.holePaths.back().empty()))
            {
                requestStackPush(StateID::EditorHole);
            }

            if (ImGui::MenuItem("Treeset Editor", nullptr, false, enabled))
            {
                requestStackPush(StateID::EditorBush);
            }

            if (ImGui::MenuItem("Audio Editor", "Shift+A", false, enabled))
            {
                m_windowStates[WindowID::AudioEditor].first = true;
            }

            ImGui::EndMenu();
        }


        if (ImGui::BeginMenu("Tools"))
        {
            ImGui::MenuItem("Options");

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Documentation");
            ImGui::MenuItem("About");

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    //handleFileDialogues();
}

void CourseState::drawNewProject()
{
    ImGui::OpenPopup("New Project##new_project");

    setModalSizeAndPos();
    if (ImGui::BeginPopupModal("New Project##new_project", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Project Directory: %s", m_newProjectBuffer.workingDirectory.empty() ? "None Set" : U8PATH_CAST(m_newProjectBuffer.workingDirectory));
        ImGui::SameLine();
        if (ImGui::Button("Choose##new_project"))
        {
            //browse folder
            m_newProjectBuffer.workingDirectory = cro::FileSystem::openFolderDialogue(
                m_newProjectBuffer.workingDirectory.empty() ? 
                m_editorSettings.lastLoadDirectory.empty() ? std::filesystem::current_path() : m_editorSettings.lastLoadDirectory
                : m_newProjectBuffer.workingDirectory);
        }

        //project name
        ImGui::Text("Name:");
        ImGui::SameLine();
        static cro::String conversionBuffer; //hack to convert to proper characters where we can check limit
        if (ImGui::InputText("##Name:new_project", &m_newProjectBuffer.name))
        {
            conversionBuffer = m_newProjectBuffer.name;
            if (conversionBuffer.size() > MaxProjectNameChars)
            {
                conversionBuffer = conversionBuffer.substr(0, MaxProjectNameChars);
                m_newProjectBuffer.name = conversionBuffer.toUtf8Char();
            }
        }

        if (ImGui::Button("Create##new_project"))
        {
            if (m_newProjectBuffer.workingDirectory.empty())
            {
                cro::FileSystem::showMessageBox("Error", "Project directory not set");
            }
            else if (m_newProjectBuffer.name.empty())
            {
                cro::FileSystem::showMessageBox("Error", "Project name not set");
            }
            else
            {
                //check folder is empty
                const auto files = cro::FileSystem::listFiles(m_newProjectBuffer.workingDirectory);
                const auto dirs = cro::FileSystem::listDirectories(m_newProjectBuffer.workingDirectory);

                const auto projectDir = std::to_string(std::hash<std::string>{}(m_newProjectBuffer.name));
                m_newProjectBuffer.id = projectDir;

                if (files.empty() && dirs.empty())
                {
                    const std::array Paths = getProjectDirectories(projectDir);                    

                    //create directory tree
                    std::error_code ec;
                    for (const auto& path : Paths)
                    {
                        std::filesystem::create_directory(m_newProjectBuffer.workingDirectory / path, ec);
                        if (ec)
                        {
                            break;
                        }
                    }

                    if (ec)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed creating project directory. Make sure you have permissions to do so.");
                        LogW << "Failed creating project at " << m_newProjectBuffer.workingDirectory << " - reason: " << ec.message() << std::endl;
                    }
                    else
                    {
                        m_courseData = {};

                        //save project to create the settings file
                        m_projectSettings = m_newProjectBuffer;
                        saveProject(m_newProjectBuffer.workingDirectory);

                        //call loadProject() which treats
                        //as if loading an existing project
                        //eg resets existing settings
                        loadProject(m_newProjectBuffer.workingDirectory);

                        cro::FileSystem::showMessageBox("Success", "Project created successfully");
                        ImGui::CloseCurrentPopup();
                        m_windowStates[WindowID::NewProject].first = false;
                    }
                }
                else
                {
                    cro::FileSystem::showMessageBox("Error", "Project directory is not empty");
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
            m_windowStates[WindowID::NewProject].first = false;
        }

        ImGui::EndPopup();
    }
}

void CourseState::drawProjectProperties()
{
    const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
    ImGui::SetNextWindowPos({ 0.f, ToolBarSize + InfoBarHeight });
    ImGui::SetNextWindowSize({ ProjectPanelWidth, windowSize.y - ToolBarSize - InfoBarHeight });
    ImGui::Begin("Project Properties", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    //display title
    ImGui::Text("Display Title");
    helpMarker("The name of the course to display in game. Max 34 chars");
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::InputText("##dName", &m_courseData.displayTitleBuffer))
    {
        //urrrg such string conversion. If we weren't clamping the length here
        //it might be better to just set the cro::String once when saving the cfg
        m_courseData.displayTitle = cro::String::fromUtf8(m_courseData.displayTitleBuffer.begin(), m_courseData.displayTitleBuffer.end());
        if (m_courseData.displayTitle.size() > MaxDescChar)
        {
            m_courseData.displayTitle = m_courseData.displayTitle.substr(0, MaxDescChar);
            m_courseData.displayTitleBuffer = m_courseData.displayTitle.toUtf8Char();
        }
    }

    //display description
    ImGui::NewLine();
    ImGui::Text("Display Description");
    helpMarker("The description of the course to display in game. Max 34 chars");
    ImGui::SetNextItemWidth(-1.f);
    if (ImGui::InputText("##dDesc", &m_courseData.displayDescriptionBuffer))
    {
        m_courseData.displayDescription = cro::String::fromUtf8(m_courseData.displayDescriptionBuffer.begin(), m_courseData.displayDescriptionBuffer.end());
        if (m_courseData.displayDescription.size() > MaxDescChar)
        {
            m_courseData.displayDescription = m_courseData.displayDescription.substr(0, MaxDescChar);
            m_courseData.displayDescriptionBuffer = m_courseData.displayDescription.toUtf8Char();
        }
    }

    ImGui::NewLine();
    if (ImGui::CollapsingHeader("Theme", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
    {
        //skybox
        if (!m_skyboxFiles.empty())
        {
            ImGui::NewLine();
            ImGui::Text("Skybox");
            helpMarker("Select a skybox to use with this course. The selection of skyboxes includes the default ones provided with the game as well as any authoured for this project using the skybox editor");

            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::BeginCombo("##skybox", m_skyboxFiles[m_skyboxIndex].filename().string().c_str()))
            {
                for (auto i = 0u; i < m_skyboxFiles.size(); ++i)
                {
                    if (ImGui::Selectable(m_skyboxFiles[i].filename().string().c_str(), i == m_skyboxIndex))
                    {
                        m_skyboxIndex = i;
                        m_courseData.skybox = m_skyboxFiles[i];

                        LogI << FILE_LINE << ": update 3D preview!!" << std::endl;
                    }
                }
                ImGui::EndCombo();
            }
        }

        //shrubs
        if (!m_shrubFiles.empty())
        {
            ImGui::NewLine();
            ImGui::Text("Shrubs and Trees");
            helpMarker("Select the treeset used for the shrubbery in the game. Treesets can be created in the treeset editor.");

            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::BeginCombo("##shrubs", m_shrubFiles[m_shrubIndex].filename().string().c_str()))
            {
                for (auto i = 0u; i < m_shrubFiles.size(); ++i)
                {
                    if (ImGui::Selectable(m_shrubFiles[i].filename().string().c_str(), i == m_shrubIndex))
                    {
                        m_shrubIndex = i;
                        m_courseData.shrubs = m_shrubFiles[i];

                        LogI << FILE_LINE << ": update 3D preview!!" << std::endl;
                    }
                }

                ImGui::EndCombo();
            }
        }

        //TODO instance model

        //audio
        if (!m_audioFiles.empty())
        {
            ImGui::NewLine();
            ImGui::Text("Environment Audio");
            helpMarker("Select the environment audio played in the background. The selection is available from the default game audio as well as any created in the Audio Editor. See the Edit menu");

            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::BeginCombo("##audio", m_audioFiles[m_audioIndex].filename().string().c_str()))
            {
                for (auto i = 0u; i < m_audioFiles.size(); ++i)
                {
                    if (ImGui::Selectable(m_audioFiles[i].filename().string().c_str(), i == m_audioIndex))
                    {
                        m_audioIndex = i;
                        m_courseData.audio = m_audioFiles[i];

                        pauseAudioPreview();
                        m_editorSettings.audioscapeChanged = true;
                    }
                }

                ImGui::EndCombo();
            }
        }

    }

    //list of holes
    if (!m_courseData.holePaths[0].empty())
    {
        ImGui::NewLine();

        if (ImGui::CollapsingHeader("Hole Selection", ImGuiTreeNodeFlags_Framed))
        {
            ImGui::NewLine();

            static std::uint32_t selectedIndex = 0;

            ImGui::Text("Holes");
            helpMarker("Double Click a hole to edit it, or use the up and down buttons to change the hole order.");
            if (ImGui::BeginListBox("##Holes", { -1.f, 0.f }))
            {
                for (auto i = 0u; i < m_courseData.holePaths.size(); ++i)
                {
                    if (!m_courseData.holePaths[i].empty())
                    {
                        if (ImGui::Selectable(m_courseData.holePaths[i].filename().string().c_str(), i == selectedIndex, ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            selectedIndex = i;

                            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                            {
                                //TODO edit hole
                            }
                        }
                    }
                }
                ImGui::EndListBox();
            }

            const auto ButtonWidth = (ProjectPanelWidth / 2.f) - 13.f;
            if (ImGui::Button("Up", { ButtonWidth, 0.f }))
            {
                if (selectedIndex > 0)
                {
                    selectedIndex--;
                    std::swap(m_courseData.holePaths[selectedIndex], m_courseData.holePaths[selectedIndex + 1]);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Down", { ButtonWidth, 0.f }))
            {
                if (selectedIndex < m_courseData.holePaths.size() - 1
                    && !m_courseData.holePaths[selectedIndex + 1].empty())
                {
                    std::swap(m_courseData.holePaths[selectedIndex], m_courseData.holePaths[selectedIndex + 1]);
                    selectedIndex++;
                }
            }
            if (ImGui::Button("Edit Selected", { -1.f, 0.f }))
            {
                //TODO launch editor
            }
        }
    }
    else
    {
        ImGui::NewLine();
        ImGui::Separator();
        ImGui::NewLine();
        if (ImGui::Button("New Hole", { -1.f, 0.f }))
        {
            //TODO launch new hole editor
        }
    }

    //preview image
    if (m_previewTexture.getSize().x != 0)
    {
        ImGui::NewLine();
        ImGui::Image(m_previewTexture, { 280.f, 210.f }, { 0.f, 1.f }, { 1.f, 0.f });
    }

    ImGui::NewLine();
    if (ImGui::Button("Set Preview Image", {-1.f, 0.f}))
    {
        const auto path = cro::FileSystem::openFileDialogue(m_editorSettings.lastLoadDirectory / "untitled.png", "png");
        if (!path.empty())
        {
            loadPreviewTexture(path);
        }
    }
    toolTip("Select an image to be used as the in-game preview. This will automatically be resized to 640 x 480 so images should be 4:3 ratio");

    //preview settings eg day/night or audio
    ImGui::NewLine();
    if (ImGui::CollapsingHeader("Scene Preview"))
    {
        if (ImGui::Checkbox("Night Time", &m_editorSettings.previewNightMode))
        {
            saveSettings();

            //TODO update lighting and switch audioscape
        }

        ImGui::NewLine();
        ImGui::Text("Audioscape");
        if (/*isPlaying*/true)
        {
            if (ImGui::Button("Stop##audio", { -1.f, 0.f }))
            {
                pauseAudioPreview();
            }
        }
        else
        {
            if (ImGui::Button("Play##audio", { -1.f, 0.f }))
            {
                playAudioPreview();
            }
        }

        if (ImGui::Button("Show Volume", { -1.f, 0.f }))
        {
            cro::Console::show(cro::Console::TabIndex::Audio);
        }
    }


    ImGui::End();
}

void CourseState::drawAudioEditor()
{
    if (ImGui::OpenPopup("Audio Editor"))
    {
        //for (auto& p : m_audioPreviewsDay)
        //{
        //    //TODO don't do this if it's a project path
        //    p.inputFile = std::filesystem::path();
        //}
    }

    static constexpr auto DisplaySize = glm::vec2(580.f, 360.f);

    const auto drawInputs = 
        [this](std::array<AudioPreview, AudioID::Count>& previews)
        {
            for (auto i = 0u; i < previews.size(); ++i)
            {
                const auto id = "##" + std::to_string(i);
                auto str = previews[i].inputFile.empty() ? "No File" : previews[i].inputFile.filename().string();
                ImGui::Text("%s", previews[i].label.c_str());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(160.f);
                ImGui::InputText(id.c_str(), &str, ImGuiInputTextFlags_ReadOnly);
                ImGui::SameLine();

                if (ImGui::Button(("Browse" + id).c_str()))
                {
                    const auto path = cro::FileSystem::openFileDialogue(m_editorSettings.lastLoadDirectory / "untitled.ogg");
                    if (!path.empty())
                    {
                        m_editorSettings.lastLoadDirectory = path.parent_path();

                        if (!path.has_extension() || path.extension() != ".ogg")
                        {
                            cro::FileSystem::showMessageBox("Error", "File must be ogg encoded");
                        }
                        else if (std::filesystem::file_size(path) > MaxAudioFileSize)
                        {
                            cro::FileSystem::showMessageBox("Error", "Files must be less than 1MB");
                        }
                        else
                        {
                            cro::MusicPlayer music;
                            if (!music.loadFromFile(path))
                            {
                                cro::FileSystem::showMessageBox("Error", "Unable to open audio file");
                            }
                            else if (music.getChannelCount() != previews[i].channels)
                            {
                                cro::FileSystem::showMessageBox("Error", "Audio must have " + std::to_string(previews[i].channels) + " channels");
                            }
                            else
                            {
                                previews[i].inputFile = path;
                            }
                        }
                    }
                }
                helpMarker(previews[i].toolTip);
                
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150.f);
                if (ImGui::SliderFloat(("Volume" + id).c_str(), &previews[i].volume, 0.f, 2.f))
                {
                    previews[i].volume = std::clamp(previews[i].volume, 0.f, 2.f);
                }
                if (i == (previews.size() - 3))
                {
                    ImGui::NewLine();
                }
            }
        };

    setModalSizeAndPos(DisplaySize);
    if (ImGui::BeginPopupModal("Audio Editor", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::BeginTabBar("##audio");
        if (ImGui::BeginTabItem("Day Audio"))
        {
            ImGui::BeginChild("##day_child", { -1.f, DisplaySize.y - 120.f });

            drawInputs(m_audioPreviewsDay);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Night Audio"))
        {
            ImGui::BeginChild("##night_child", { -1.f, DisplaySize.y - 120.f });
            drawInputs(m_audioPreviewsNight);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        ImGui::Text("Audio files should be encoded as ogg and under 1MB in size.");
        helpMarker("Audio can be previewed from the collapsible \'Scene Preview\' region in the Project Properties window on the left.");
        ImGui::NewLine();
        ImGui::BeginChild("##button_left", {DisplaySize.x / 1.25f, 0.f});
        //if (ImGui::Button("Preview"))
        //{
        //    //TODO play audio preview and allow setting volume
        //}
        ImGui::EndChild();//button_left
        ImGui::SameLine();

        if (ImGui::Button("Save##audio"))
        {
            exportAudioFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Close##audio"))
        {
            //TODO stop audio? Or will we always
            //play audio based on current selection?

            if (cro::FileSystem::showMessageBox("Confirm", "Save audio configuration?", cro::FileSystem::YesNo))
            {
                //save file
                exportAudioFile();
            }

            m_windowStates[WindowID::AudioEditor].first = false;
            ImGui::CloseCurrentPopup();
            refreshAudioFiles();
        }
        ImGui::EndPopup();
    }
}

//menu functions which are also invoked with keyboard shortcuts
void CourseState::menuConfirmThenOpen(const std::filesystem::path& path)
{
    if (!m_projectSettings.name.empty())
    {
        if (cro::FileSystem::showMessageBox("Confirm", "Save current project?", cro::FileSystem::ButtonType::YesNo))
        {
            saveProject(m_projectSettings.workingDirectory);
        }
    }
    loadProject(path);
    saveSettings();
}

void CourseState::menuNewProject()
{
    if (!m_projectSettings.name.empty())
    {
        if (cro::FileSystem::showMessageBox("Confirm", "Save current project?", cro::FileSystem::ButtonType::YesNo))
        {
            saveProject(m_projectSettings.workingDirectory);
        }
    }

    m_newProjectBuffer.name = {};
    m_windowStates[WindowID::NewProject].first = true;
}

void CourseState::menuOpenProject()
{
    const auto path = cro::FileSystem::openFileDialogue(m_editorSettings.lastLoadDirectory / "settings.svgproj", "svgproj");
    if (!path.empty())
    {
        menuConfirmThenOpen(path.parent_path());
    }
}