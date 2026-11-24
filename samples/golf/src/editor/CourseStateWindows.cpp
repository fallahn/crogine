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

#include <crogine/gui/Gui.hpp>

void CourseState::drawMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project"))
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

            if (ImGui::MenuItem("Open Project"))
            {
                const auto path = cro::FileSystem::openFolderDialogue(m_editorSettings.lastLoadDirectory);
                if (!path.empty())
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
            }

            if (!m_projectSettings.workingDirectory.empty())
            {
                if (ImGui::MenuItem("Save Project"))
                {
                    saveProject(m_projectSettings.workingDirectory);
                }
            }

            //if not empty history display recent projects

            if (ImGui::MenuItem("Quit"))
            {
                confirmQuit();
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Treeset Editor"))
            {
                requestStackPush(StateID::EditorBush);
            }

            if (ImGui::MenuItem("Hole Editor"))
            {
                requestStackPush(StateID::EditorHole);
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

    const auto winSize = glm::vec2(cro::App::getWindow().getSize());
    constexpr auto displaySize = glm::vec2(380.f, 160.f);
    const auto displayPos = (winSize - displaySize) / 2.f;

    ImGui::SetNextWindowSize({ displaySize.x, displaySize.y });
    ImGui::SetNextWindowPos({ displayPos.x, displayPos.y });
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
        static const std::size_t MaxNameChars = 128;
        static cro::String conversionBuffer; //hack to convert to proper characters where we can check limit
        if (ImGui::InputText("##Name:new_project", &m_newProjectBuffer.name))
        {
            conversionBuffer = m_newProjectBuffer.name;
            if (conversionBuffer.size() > MaxNameChars)
            {
                conversionBuffer = conversionBuffer.substr(0, MaxNameChars);
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