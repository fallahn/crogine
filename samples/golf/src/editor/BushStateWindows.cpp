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

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/detail/OpenGL.hpp>

namespace
{
    constexpr float MenuBarHeight = 18.f; //TODO this is probably useful in a consts file
    const std::filesystem::path ConfigFile = "bush_window_states.cfg";

    //TODO move to consts file
    static constexpr float MinRadius = 0.1f;
    static constexpr float MaxRadius = 3.f;
    static constexpr float MaxHeight = 3.5f;
    static constexpr float MaxPos = 6.f;
}

void BushState::drawMenuBar(bool*)
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Load Model"))
            {
                if (m_models[1].isValid()
                    && cro::FileSystem::showMessageBox("Save Settings", "Do you want to save the current treeset first?", cro::FileSystem::YesNo))
                {
                    //shows save option first
                    showPresetSaveDialogue(OpenDialogueState::Model);
                }
                else
                {
                    showModelOpenDialogue();
                }
            }
            if (ImGui::MenuItem("Open Preset"))
            {
                if (m_models[1].isValid()
                    && cro::FileSystem::showMessageBox("Save Settings", "Do you want to save the current treeset first?", cro::FileSystem::YesNo))
                {
                    //shows save option first
                    showPresetSaveDialogue(OpenDialogueState::Treeset);
                }
                else
                {
                    showPresetOpenDialogue();
                }
            }
            if (ImGui::MenuItem("Save Preset"))
            {
                showPresetSaveDialogue();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
            {
                if (getStateCount() == 1)
                {
                    if (!cro::FileSystem::showMessageBox("Save Settings", "Do you want to save the current treeset first?", cro::FileSystem::YesNo))
                    {
                        cro::App::quit();
                    }
                    else
                    {
                        showPresetSaveDialogue(OpenDialogueState::Quit);
                    }
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

        if (ImGui::BeginMenu("Help"))
        {
            ImGui::MenuItem("Controls", nullptr, &m_windows[m_windowIndices[WindowIndex::Help]].visible);
            ImGui::MenuItem("About");

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
    const float PanelHeight = WindowSize.y - MenuBarHeight;

    ImGui::SetNextWindowSize({ PropertiesPanelWidth, PanelHeight });
    ImGui::SetNextWindowPos({ 0.f, MenuBarHeight });
    if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus))
    {
        ImGui::BeginChild("##properties");
        if (m_models[0].isValid())
        {
            ImGui::Text("Properties");
            if (ImGui::SliderFloat("Leaf Size", &m_treeset.leafSize, 0.01f, 1.f))
            {
                glUseProgram(m_shaderUniforms[BushShaderID::Bush].shaderID);
                glUniform1f(m_shaderUniforms[BushShaderID::Bush].size, m_treeset.leafSize);
            }

            if (ImGui::SliderFloat("Randomness", &m_treeset.randomAmount, 0.f, 1.f))
            {
                glUseProgram(m_shaderUniforms[BushShaderID::Bush].shaderID);
                glUniform1f(m_shaderUniforms[BushShaderID::Bush].randomness, m_treeset.randomAmount);
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
                glUseProgram(m_shaderUniforms[BushShaderID::Bush].shaderID);
                glUniform1f(m_shaderUniforms[BushShaderID::Bush].colourRotation, m_treeset.colourRotation);
            }

            if (ImGui::ColorEdit3("Colour", &m_treeset.colour[0]))
            {
                glUseProgram(m_shaderUniforms[BushShaderID::Bush].shaderID);
                glUniform3f(m_shaderUniforms[BushShaderID::Bush].colour, m_treeset.colour.r, m_treeset.colour.g, m_treeset.colour.b);
            }

            ImGui::NewLine();
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
                        glUseProgram(m_shaderUniforms[BushShaderID::Bush].shaderID);
                        glUniform3f(m_shaderUniforms[BushShaderID::Bush].colour, c.x, c.y, c.z);
                    }

                    if ((i++ % 12) != 11)
                    {
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::NewLine();
            ImGui::NewLine();
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
            ImGui::SeparatorText("Materials");
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



            ImGui::NewLine();
            ImGui::SeparatorText("Preview");
            ImGui::SliderFloat("Wind Strength", &m_windData.direction[1], 0.1f, 1.f);
            if (ImGui::SliderFloat("Wind Direction", &m_windRotation, 0.f, cro::Util::Const::PI))
            {
                auto dir = glm::rotate(glm::quat(0.f, 0.f, 0.f, 1.f), m_windRotation, cro::Transform::Y_AXIS) * glm::vec3(-1.f, 0.f, 0.f);
                m_windData.direction[0] = dir.x;
                m_windData.direction[2] = dir.z;
            }

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
            if (showInstanced)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, { 1.f, 0.f, 0.f, 1.f });
                ImGui::TextWrapped("%s", "Collision object preview is not available in instanced view.");
                ImGui::PopStyleColor();
            }

            drawCollisionPanel();            
        }
        else
        {
            //this should never show if we have a model
            //loaded so there's no reason to as if we want
            //to save the current preset first.
            if (ImGui::Button("Load Model"))
            {
                showModelOpenDialogue();
            }
            ImGui::SameLine();
            if (ImGui::Button("Open Preset"))
            {
                showPresetOpenDialogue();
            }
        }
        ImGui::EndChild();

    }
    ImGui::End();
}

void BushState::drawCollisionPanel()
{
    ImGui::NewLine();
    ImGui::SeparatorText("Collision");
    if (ImGui::Button("Add Object##col"))
    {
        if (m_collisionObjects.size() < MaxCollisionObjects)
        {
            m_selectedCollisionObject = m_collisionObjects.size();
            auto& obj = m_collisionObjects.emplace_back();
            obj.preview = m_gameScene.createEntity();
            obj.preview.addComponent<cro::Transform>();
            m_collisionDefs[CollisionObject::Sphere]->createModel(obj.preview);
            obj.preview.getComponent<cro::Model>().setRenderFlags(~RenderFlagsBillboard);
            m_models[1].getComponent<cro::Transform>().addChild(obj.preview.getComponent<cro::Transform>());

            obj.preview.addComponent<cro::Callback>().active = true;
            obj.preview.getComponent<cro::Callback>().function =
                [this](cro::Entity e, float)
                {
                    e.getComponent<cro::Model>().setHidden(m_models[1].getComponent<cro::Model>().isHidden());
                };

            selectCollisionObject(m_selectedCollisionObject);
        }
        else
        {
            //TODO show modal saying we reached max objects
        }
    }
    if (!m_collisionObjects.empty())
    {
        ImGui::SameLine();
        if (ImGui::Button("Remove Object##col"))
        {
            //remove selected
            m_gameScene.destroyEntity(m_collisionObjects[m_selectedCollisionObject].preview);
            m_collisionObjects.erase(m_collisionObjects.begin() + m_selectedCollisionObject);

            m_selectedCollisionObject = m_collisionObjects.empty() ? 0 : std::min(m_selectedCollisionObject, m_collisionObjects.size() - 1);
            selectCollisionObject(m_selectedCollisionObject);
        }

        //we have to check this again because we may have removed all the objects with the above button
        if (!m_collisionObjects.empty())
        {
            ImGui::Text("G: Move, R: Rotate, S: Scale");

            //draw listbox
            static const std::array<std::string, CollisionObject::TypeCount> Labels =
            {
                "Sphere", "Cylinder"
            };
            ImGui::NewLine();
            ImGui::Text("Collision Objects");
            ImGui::BeginListBox("##Objects");
            for (auto i = 0u; i < m_collisionObjects.size(); ++i)
            {
                const std::string label = Labels[m_collisionObjects[i].type] + "##" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), i == m_selectedCollisionObject))
                {
                    selectCollisionObject(i);
                    recentreCamera(m_collisionObjects[i].preview.getComponent<cro::Transform>().getWorldPosition());
                }
            }
            ImGui::EndListBox();

            //draw properties of selected collision object
            ImGui::NewLine();
            ImGui::Text("Shape");
            if (ImGui::BeginCombo("##type", Labels[m_collisionObjects[m_selectedCollisionObject].type].c_str()))
            {
                for (auto i = 0u; i < Labels.size(); ++i)
                {
                    if (ImGui::Selectable(Labels[i].c_str(), i == m_collisionObjects[m_selectedCollisionObject].type))
                    {
                        m_collisionObjects[m_selectedCollisionObject].type = i;

                        auto ent = m_collisionObjects[m_selectedCollisionObject].preview;
                        auto tx = ent.getComponent<cro::Transform>().getLocalTransform();

                        m_gameScene.destroyEntity(ent);

                        ent = m_gameScene.createEntity();
                        ent.addComponent<cro::Transform>().setLocalTransform(tx);
                        m_collisionDefs[i]->createModel(ent);
                        ent.getComponent<cro::Model>().setRenderFlags(~RenderFlagsBillboard);
                        ent.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CollisionHighlightColour);
                        m_models[1].getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                        m_collisionObjects[m_selectedCollisionObject].preview = ent;

                        //TODO ideally we want to keep a separate radius property for cylinders somewhere
                        auto scale = ent.getComponent<cro::Transform>().getScale();
                        scale.y = scale.x;
                        ent.getComponent<cro::Transform>().setScale(scale);

                        //make sureto update the selection highlight
                        selectCollisionObject(m_selectedCollisionObject);
                    }
                }
                ImGui::EndCombo();
            }

            auto& tx = m_collisionObjects[m_selectedCollisionObject].preview.getComponent<cro::Transform>();

            auto pos = tx.getPosition();
            ImGui::NewLine();
            ImGui::Text("Position");
            if (ImGui::SliderFloat3("##Position", &pos[0], -MaxPos, MaxPos))
            {
                pos.x = std::clamp(pos.x, -MaxPos, MaxPos);
                pos.y = std::clamp(pos.y, -MaxPos, MaxPos);
                pos.z = std::clamp(pos.z, -MaxPos, MaxPos);
                tx.setPosition(pos);
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset##pos"))
            {
                pos = glm::vec3(0.f);
                tx.setPosition(pos);
            }
            ImGui::NewLine();

            ImGui::Text("Rotation");
            const auto rotation = tx.getRotation();
            ImGui::Text("%3.2f,%3.2f,%3.2f,%3.2f", rotation.x, rotation.y, rotation.z, rotation.w);
            ImGui::SameLine();
            if (ImGui::Button("Reset##rot"))
            {
                tx.setRotation(cro::Transform::QUAT_IDENTITY);
            }

            ImGui::NewLine();

            //scale X sets the radius, Y the half height
            auto scale = tx.getScale();
            if (m_collisionObjects[m_selectedCollisionObject].type == CollisionObject::Sphere)
            {
                ImGui::Text("Radius");
                if (ImGui::SliderFloat("##Radius", &scale[0], MinRadius, MaxRadius))
                {
                    scale.x = std::clamp(scale.x, 0.1f, MaxRadius);
                    scale.z = scale.y = scale.x;
                    tx.setScale(scale);
                }
            }
            else
            {
                ImGui::Text("Radius");
                if (ImGui::SliderFloat("##Radius", &scale[0], MinRadius, MaxRadius))
                {
                    scale.x = std::clamp(scale.x, 0.1f, MaxRadius);
                    scale.z = scale.x;
                    tx.setScale(scale);
                }
                ImGui::NewLine();
                ImGui::Text("Height");
                if (ImGui::SliderFloat("##Height", &scale[1], MinRadius, MaxHeight))
                {
                    scale.y = std::clamp(scale.y, 0.1f, MaxHeight);
                    tx.setScale(scale);
                }
            }
        }
    }
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

void BushState::drawHelp(bool* visible)
{
    constexpr glm::vec2 WindowSize = { 300.f, 200.f };
    const glm::vec2 WindowPos = (glm::vec2(cro::App::getWindow().getSize()) - WindowSize) / 2.f;

    ImGui::SetNextWindowSize({ WindowSize.x, WindowSize.y });
    ImGui::SetNextWindowPos({ WindowPos.x, WindowPos.y });
    if (ImGui::Begin("Help", visible, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        ImGui::Text("Ctrl+Click: Retarget Camera");
        ImGui::Text("Middle Mouse: Pan");
        ImGui::Text("Shift+Middle Mouse: Rotate");
        ImGui::Text("Mouse Wheel: Zoom");
        ImGui::NewLine();
        ImGui::Text("G: Translate Collision Object");
        ImGui::Text("R: Rotate Collision Object");
        ImGui::Text("S: Scale Collision Object");
    }
    ImGui::End();
}

void BushState::selectCollisionObject(std::size_t idx)
{
    idx = std::min(idx, m_collisionObjects.size() - 1);
    m_selectedCollisionObject = idx;

    if (!m_collisionObjects.empty())
    {
        for (auto& o : m_collisionObjects)
        {
            o.preview.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", cro::Colour::White);
        }
        m_collisionObjects[idx].preview.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CollisionHighlightColour);
    }
}

void BushState::drawCollisionManipulator()
{
    //DEAR FUTURE ME: if this isn't matching the current viewport make sure
    //to update ImGuizmo::SetRect() in drawUI()
    if (!m_collisionObjects.empty())
    {
        auto mode = m_gizmoMode;
        //if we know we only adjust one axis we cam make sure
        //the other 3 always match to maintain radius
        if(mode == ImGuizmo::OPERATION::SCALE
            && m_collisionObjects[m_selectedCollisionObject].type == CollisionObject::Sphere)
        { 
            mode = ImGuizmo::OPERATION::SCALE_Y;
        }

        auto selectedEnt = m_collisionObjects[m_selectedCollisionObject].preview;

        const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
        const auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
        auto tx = selectedEnt.getComponent<cro::Transform>().getWorldTransform();
        ImGuizmo::Manipulate(&cam.getActivePass().viewMatrix[0][0], &cam.getProjectionMatrix()[0][0], mode, ImGuizmo::MODE::LOCAL, &tx[0][0]);
        selectedEnt.getComponent<cro::Transform>().setLocalTransform(cro::Util::Matrix::getRelativeTransform(selectedEnt.getComponent<cro::Transform>().getParentTransform(), tx));

        //clamp translation and scale
        auto& txComponent = selectedEnt.getComponent<cro::Transform>();
        switch (m_gizmoMode)
        {
        default: break;
        case ImGuizmo::OPERATION::SCALE:
        {
            auto scale = txComponent.getScale();
            if (m_collisionObjects[m_selectedCollisionObject].type == CollisionObject::Sphere)
            {
                scale.x = scale.z = std::clamp(scale.y, MinRadius, MaxRadius);
            }
            else
            {
                scale.x = std::clamp(scale.x, MinRadius, MaxRadius);
                scale.z = scale.x;
                scale.y = std::clamp(scale.y, MinRadius, MaxHeight);
            }
            txComponent.setScale(scale);
        }
            break;
        case ImGuizmo::OPERATION::TRANSLATE:
        {
            auto pos = txComponent.getPosition();
            pos.x = std::clamp(pos.x, -MaxPos, MaxPos);
            pos.y = std::clamp(pos.y, -MaxPos, MaxPos);
            pos.z = std::clamp(pos.z, -MaxPos, MaxPos);
            txComponent.setPosition(pos);
        }
            break;
        }
    }
}

void BushState::drawViewMainpulator()
{
    static constexpr float ManipSize = 96.f;

    //auto tx = /*glm::inverse*/(m_arcBall.getComponent<cro::Transform>().getLocalTransform());
    //ImGuizmo::ViewManipulate(&tx[0][0], 10.f, ImVec2(ImGui::GetIO().DisplaySize.x - ManipSize, 22.f), {ManipSize, ManipSize}, 0/*x10101010*/);
    //m_arcBall.getComponent<cro::Transform>().setRotation(/*glm::inverse*/(tx));
}

void BushState::showModelOpenDialogue()
{
    //track path of last opened model
    if (!m_openFilePaths[OpenDialogueState::Model].empty())
    {
        m_openFileBrowser.SetPwd(m_openFilePaths[OpenDialogueState::Model]);
    }
    m_openFileBrowser.SetTypeFilters({ ".cmt" });
    m_openFileBrowser.SetTitle("Load Model");

    m_openFileBrowser.Open();
    m_openDialogueState = OpenDialogueState::Model;
}

void BushState::showPresetOpenDialogue()
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

void BushState::showPresetSaveDialogue(std::int32_t dialogueToOpen)
{
    if (!m_saveFilePaths[SaveDialogueState::Treeset].empty())
    {
        m_saveFileBrowser.SetPwd(m_saveFilePaths[SaveDialogueState::Treeset]);
    }
    m_saveFileBrowser.SetTypeFilters({ ".tst" });
    m_saveFileBrowser.SetTitle("Save Treeset");
    //TODO find out why this doesn't work
    m_saveFileBrowser.SetInputName(m_lastTreeset.string());


    m_saveFileBrowser.Open();
    m_saveDialogueState = SaveDialogueState::Treeset;
    m_pendingDialogueState = dialogueToOpen;
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
                m_saveFilePaths[SaveDialogueState::Treeset] = m_openFileBrowser.GetPwd();
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
            auto path = m_saveFileBrowser.GetSelected();
            if (!path.has_extension() || path.extension() != "tst")
            {
                path.replace_extension("tst");
            }
            m_saveFileBrowser.ClearSelected();

            //TODO this needs to be a modal
            if (cro::FileSystem::fileExists(path)
                && !cro::FileSystem::showMessageBox("Confirm", "File Exists, Overwrite?", cro::FileSystem::YesNo))
            {
                m_saveDialogueState = SaveDialogueState::None;
                return;
            }

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

            switch (m_pendingDialogueState)
            {
            default: break;
            case OpenDialogueState::Model:
                showModelOpenDialogue();
                break;
            case OpenDialogueState::Treeset:
                showPresetOpenDialogue();
                break;
            case OpenDialogueState::Quit:
                cro::App::quit();
                break;
            }
            m_pendingDialogueState = OpenDialogueState::None;
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