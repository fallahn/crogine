/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2026
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

#include "BushState.hpp"
#include "BushStateConsts.hpp"

#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/detail/OpenGL.hpp>

namespace
{
    constexpr float MenuBarHeight = 18.f; //TODO this is probably useful in a consts file
    const std::filesystem::path ConfigFile = "bush_window_states.cfg";
}

void BushState::drawMenuBar(bool*)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load Model"))
            {
                //track path of last opened model
                if (!m_openFilePaths[OpenDialogueState::Model].empty())
                {
                    m_openFileBrowser.SetPwd(m_openFilePaths[OpenDialogueState::Model]);
                }
                m_openFileBrowser.SetTypeFilters({".cmt"});
                m_openFileBrowser.SetTitle("Load Model");

                m_openFileBrowser.Open();
                m_openDialogueState = OpenDialogueState::Model;
            }
            if (ImGui::MenuItem("Open Preset"))
            {
                if (!m_openFilePaths[OpenDialogueState::Treeset].empty())
                {
                    m_openFileBrowser.SetPwd(m_openFilePaths[OpenDialogueState::Treeset]);
                }
                m_openFileBrowser.SetTypeFilters({ ".tst" });
                m_openFileBrowser.SetTitle("Open Preset");

                m_openFileBrowser.Open();
                m_openDialogueState = OpenDialogueState::Treeset;
            }
            if (ImGui::MenuItem("Save Preset"))
            {
                if (!m_saveFilePaths[SaveDialogueState::Treeset].empty())
                {
                    m_saveFileBrowser.SetPwd(m_saveFilePaths[SaveDialogueState::Treeset]);
                }
                m_saveFileBrowser.SetTypeFilters({ ".tst" });
                m_saveFileBrowser.SetTitle("Save Treeset");
                m_saveFileBrowser.SetInputName(m_lastTreeset.string());


                m_saveFileBrowser.Open();
                m_saveDialogueState = SaveDialogueState::Treeset;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                if (getStateCount() == 1)
                {
                    cro::App::quit();
                }
                else
                {
                    requestStackPop();
                    Social::setStatus(Social::InfoID::Menu, { "Main Menu" });
                }
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Tools"))
        {
            if (ImGui::MenuItem("Create Billboard", nullptr, &m_windows[m_windowIndices[WindowIndex::Billboard]].visible))
            {
                saveWindowStates();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    //do this here as we assume menu bar is always updated
    handleFileDialogues();
}

void BushState::drawProperties(bool*)
{
    const glm::vec2 WindowSize = cro::App::getWindow().getSize();
    static constexpr float PanelWidth = 344.f;
    const float PanelHeight = WindowSize.y - MenuBarHeight;

    ImGui::SetNextWindowSize({ PanelWidth, PanelHeight });
    ImGui::SetNextWindowPos({ 0.f, MenuBarHeight });
    if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration))
    {
        ImGui::BeginChild("##properties");
        ImGui::Text("Properties");
        if (ImGui::SliderFloat("Leaf Size", &m_treeset.leafSize, 0.01f, 1.f))
        {
            glUseProgram(m_shaderUniform.shaderID);
            glUniform1f(m_shaderUniform.size, m_treeset.leafSize);
        }

        if (ImGui::SliderFloat("Randomness", &m_treeset.randomAmount, 0.f, 1.f))
        {
            glUseProgram(m_shaderUniform.shaderID);
            glUniform1f(m_shaderUniform.randomness, m_treeset.randomAmount);
        }

        if (ImGui::SliderFloat("Scale", &m_treeset.scale, MinModelScale, 3.f))
        {
            m_treeset.scale = std::max(MinModelScale, m_treeset.scale);
            for (auto e : m_models)
            {
                if (e.isValid())
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec3(m_treeset.scale));
                }
            }
        }

        if (ImGui::SliderFloat("Colour Rotation", &m_treeset.colourRotation, 0.f, 1.f))
        {
            glUseProgram(m_shaderUniform.shaderID);
            glUniform1f(m_shaderUniform.colourRotation, m_treeset.colourRotation);
        }

        if (ImGui::ColorEdit3("Colour", &m_treeset.colour[0]))
        {
            glUseProgram(m_shaderUniform.shaderID);
            glUniform3f(m_shaderUniform.colour, m_treeset.colour.r, m_treeset.colour.g, m_treeset.colour.b);
        }

        if (ImGui::Button("Load Palette"))
        {
            auto path = cro::FileSystem::openFileDialogue("", "ase");
            if (!path.empty())
            {
                m_palette.loadFromFile(path);
            }
        }
        auto i = 0;
        for (const auto& swatch : m_palette.getSwatches())
        {
            for (const auto& colour : swatch.colours)
            {
                ImVec4 c(colour.colour.getVec4());
                std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);
                if (ImGui::ColorButton(label.c_str(), c))
                {
                    m_treeset.colour = { c.x, c.y, c.z };
                    glUseProgram(m_shaderUniform.shaderID);
                    glUniform3f(m_shaderUniform.colour, c.x, c.y, c.z);
                }

                if ((i++ % 12) != 11)
                {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::NewLine();
        ImGui::Separator();
        ImGui::SliderFloat("Wind Strength", &m_windData.direction[1], 0.1f, 1.f);
        if (ImGui::SliderFloat("Wind Direction", &m_windRotation, 0.f, cro::Util::Const::PI))
        {
            auto dir = glm::rotate(glm::quat(0.f, 0.f, 0.f, 1.f), m_windRotation, cro::Transform::Y_AXIS) * glm::vec3(-1.f, 0.f, 0.f);
            m_windData.direction[0] = dir.x;
            m_windData.direction[2] = dir.z;
        }

        if (ImGui::Button("Load Texture"))
        {
            auto path = cro::FileSystem::openFileDialogue("", "png");
            if (!path.empty())
            {
                m_leafTexture->loadFromFile(path);
                m_treeset.texturePath = U8PATH_CAST(cro::FileSystem::getFileName(path));
            }
        }
        ImGui::SameLine();
        ImGui::Image(*m_leafTexture, { 32.f, 32.f }, { 0.f, 0.f }, { 1.f, 1.f });


        ImGui::NewLine();
        if (m_models[0].isValid())
        {
            if (ImGui::SliderFloat("Rotation", &m_rotation, 0.f, cro::Util::Const::TAU))
            {
                for (auto model : m_models)
                {
                    model.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, m_rotation);
                }
            }

            static bool showInstanced = false;
            if (ImGui::Checkbox("Instanced", &showInstanced))
            {
                m_models[1].getComponent<cro::Model>().setHidden(showInstanced);
                m_models[2].getComponent<cro::Model>().setHidden(!showInstanced);
            }

            for (i = 0u; i < m_materials.size(); ++i)
            {
                auto label = "Material##" + std::to_string(i);
                if (ImGui::InputInt(label.c_str(), &m_materials[i].activeMaterial))
                {
                    m_materials[i].activeMaterial = std::min(1, std::max(0, m_materials[i].activeMaterial));

                    for (auto j = 1u; j < m_models.size(); ++j)
                    {
                        m_models[j].getComponent<cro::Model>().setMaterial(i, m_materials[i].materials[m_materials[i].activeMaterial]);
                        m_models[j].getComponent<cro::Model>().getMeshData().indexData[i].primitiveType =
                            m_materials[i].activeMaterial ? GL_POINTS : GL_TRIANGLES;
                    }
                }
            }
        }
        ImGui::EndChild();

    }
    ImGui::End();
}

void BushState::drawBillboard(bool* visible)
{
    const glm::vec2 WindowSize = cro::App::getWindow().getSize();
    static constexpr float PanelWidth = 396.f;
    const float PanelHeight = WindowSize.y - MenuBarHeight;

    ImGui::SetNextWindowSize({ PanelWidth, PanelHeight });
    ImGui::SetNextWindowPos({ std::floor(WindowSize.x - PanelWidth), MenuBarHeight});
    if (ImGui::Begin("Create Billboard", visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        //check if window was closed
        if (!m_windows[m_windowIndices[WindowIndex::Billboard]].visible)
        {
            saveWindowStates();
        }

        glm::vec2 size(BillboardTargetSize);
        ImGui::Image(m_billboardTexture.getTexture(), { size.x, size.y }, { 0.f, 1.f }, { 1.f, 0.f });

        ImGui::SliderFloat("Leaf Scale", &m_billboardScaleMultiplier, 0.1f, 1.f);

        if (ImGui::Button("Reset Rotation"))
        {
            m_rootRotation = glm::vec2(0.f);
            m_root.getComponent<cro::Transform>().setRotation(glm::mat4(1.f));
        }

        ImGui::SameLine();

        if (ImGui::Button("Save Image"))
        {
            if (!m_saveFilePaths[SaveDialogueState::Billboard].empty())
            {
                m_saveFileBrowser.SetPwd(m_saveFilePaths[SaveDialogueState::Billboard]);
            }
            m_saveFileBrowser.SetTypeFilters({ ".png" });
            m_saveFileBrowser.SetTitle("Save Billboard");
            m_saveFileBrowser.SetInputName("untitled.png");

            m_saveFileBrowser.Open();
            m_saveDialogueState = SaveDialogueState::Billboard;
        }
    }
    ImGui::End();
}

void BushState::drawThumbnail(bool* visible)
{
    if (ImGui::Begin("Thumbnail", visible))
    {
        ImGui::Image(m_thumbnailTexture.getTexture(), { static_cast<float>(ThumbnailSize.x), static_cast<float>(ThumbnailSize.y) }, { 0.f, 1.f }, { 1.f, 0.f });
    }
    ImGui::End();
}

void BushState::drawSkyboxEditor(bool* visible)
{
    if (ImGui::Begin("Skybox", visible))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, { 1.f, 0.f, 0.f, 1.f });
        ImGui::Text("Move this to elsewhere in the editor");
        ImGui::PopStyleColor();
        if (ImGui::Button("Load Skybox"))
        {
            loadSkyboxFile();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Skybox"))
        {
            saveSkyboxFile();
        }

        //hacky but should be ok if there are not too many models in
        //the skybox scene.
        std::vector<cro::Entity> items;
        for (auto e : m_skyScene.getSystem<cro::ModelRenderer>()->getEntities())
        {
            if (!e.getLabel().empty())
            {
                items.push_back(e);
            }
        }
        int k = 0;
        for (auto item : items)
        {
            ImGui::Text("%s", item.getLabel().c_str());
            ImGui::SameLine();
            std::string label = "Remove##" + std::to_string(k++);
            if (ImGui::Button(label.c_str()))
            {
                m_skyScene.destroyEntity(item);
            }
        }
        if (ImGui::Button("Add Model"))
        {
            addSkyboxModel();
        }
        ImGui::NewLine();

        auto i = 0;
        ImGui::Text("Sky Top");
        for (const auto& swatch : m_palette.getSwatches())
        {
            for (const auto& colour : swatch.colours)
            {
                ImVec4 c(colour.colour.getVec4());
                std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);
                if (ImGui::ColorButton(label.c_str(), c))
                {
                    m_skyTop = cro::Colour(c.x, c.y, c.z);
                    m_skyScene.setSkyboxColours(SkyBottom, m_skyMid, m_skyTop);
                }

                if ((i++ % 12) != 11)
                {
                    ImGui::SameLine();
                }
            }
        }
        ImGui::Separator();
        i = 120;
        ImGui::Text("Sky Bottom");
        for (const auto& swatch : m_palette.getSwatches())
        {
            for (const auto& colour : swatch.colours)
            {
                ImVec4 c(colour.colour.getVec4());
                std::string label = colour.name.toAnsiString() + "##" + std::to_string(i);
                if (ImGui::ColorButton(label.c_str(), c))
                {
                    m_skyMid = cro::Colour(c.x, c.y, c.z);
                    m_skyScene.setSkyboxColours(SkyBottom, m_skyMid, m_skyTop);
                }

                if ((i++ % 12) != 11)
                {
                    ImGui::SameLine();
                }
            }
        }
    }
    ImGui::End();
}

void BushState::handleFileDialogues()
{
    //updates the drawlist - has to be called first else IsOpened() is always false
    m_openFileBrowser.Display();

    if (m_openFileBrowser.IsOpened())
    {
        if (m_openFileBrowser.HasSelected())
        {
            const auto path = m_openFileBrowser.GetSelected();

            switch (m_openDialogueState)
            {
            default: break;
            case OpenDialogueState::Model:
                loadModel(path);
                break;
            case OpenDialogueState::Treeset:
                loadPreset(path);
                break;
            }
            m_openFilePaths[m_openDialogueState] = m_openFileBrowser.GetPwd();
            saveWindowStates();

            m_openDialogueState = OpenDialogueState::None;
            m_openFileBrowser.ClearSelected();
        }
    }


    m_saveFileBrowser.Display();

    if (m_saveFileBrowser.IsOpened())
    {
        if (m_saveFileBrowser.HasSelected())
        {
            const auto path = m_saveFileBrowser.GetSelected();

            switch (m_saveDialogueState)
            {
            default: break;
            case SaveDialogueState::Treeset:
                savePreset(path);
                break;
            case SaveDialogueState::Billboard:
                m_billboardTexture.saveToFile(path);
                break;
            }

            m_saveFilePaths[m_saveDialogueState] = m_saveFileBrowser.GetPwd();
            saveWindowStates();

            m_saveDialogueState = SaveDialogueState::None;
            m_saveFileBrowser.ClearSelected();
        }
    }
}

void BushState::loadWindowStates()
{
    //this must happen after registerWindows() has been called!!
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(cro::App::getPreferencePath() / ConfigFile))
    {
        const auto& props = cfg.getProperties();
        for (const auto& prop : props)
        {
            const auto& name = prop.getName();
            if (name == "billboard_state")
            {
                m_windows[m_windowIndices[WindowIndex::Billboard]].visible = prop.getValue<bool>();
            }
            else if (name == "open_model")
            {
                m_openFilePaths[OpenDialogueState::Model] = prop.getValue<std::string>();
            }
            else if (name == "open_treeset")
            {
                m_openFilePaths[OpenDialogueState::Treeset] = prop.getValue<std::string>();
            }
            else if (name == "save_treeset")
            {
                m_saveFilePaths[SaveDialogueState::Treeset] = prop.getValue<std::string>();
            }
            else if (name == "save_billboard")
            {
                m_saveFilePaths[SaveDialogueState::Billboard] = prop.getValue<std::string>();
            }
        }
    }
}

void BushState::saveWindowStates()
{
    cro::ConfigFile cfg;
    cfg.addProperty("billboard_state").setValue(m_windows[m_windowIndices[WindowIndex::Billboard]].visible);

    if (!m_openFilePaths[OpenDialogueState::Model].empty())
    {
        cfg.addProperty("open_model").setValue(U8PATH_CAST(m_openFilePaths[OpenDialogueState::Model]));
    }

    if (!m_openFilePaths[OpenDialogueState::Treeset].empty())
    {
        cfg.addProperty("open_treeset").setValue(U8PATH_CAST(m_openFilePaths[OpenDialogueState::Treeset]));
    }

    if (!m_saveFilePaths[SaveDialogueState::Treeset].empty())
    {
        cfg.addProperty("save_treeset").setValue(U8PATH_CAST(m_saveFilePaths[SaveDialogueState::Treeset]));
    }

    if (!m_saveFilePaths[SaveDialogueState::Billboard].empty())
    {
        cfg.addProperty("save_billboard").setValue(U8PATH_CAST(m_saveFilePaths[SaveDialogueState::Billboard]));
    }

    cfg.save(cro::App::getPreferencePath() / ConfigFile);
}