/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include "ModelState.hpp"
#include "SharedStateData.hpp"
#include "ModelViewerConsts.inl"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>

#include <crogine/graphics/MeshBuilder.hpp>

#include <crogine/util/String.hpp>

namespace
{
    const std::array ShaderStrings =
    {
        "Unlit", "VertexLit", "PBR"
    };

    const std::array BlendStrings =
    {
        "None", "Alpha", "Multiply", "Additive"
    };

    const std::array AttribStrings =
    {
        "  Position",
        "  Colour",
        "  Normal",
        "  Tangent",
        "  Bitangent",
        "  UV0",
        "  UV1",
        "  BlendIndices",
        "  BlendWeights"
    };

    enum WindowID
    {
        Inspector,
        Browser,
        MaterialSlot,
        ViewGizmo,

        Count
    };
    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f)),
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f)),
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f))
    };

    void helpMarker(const char* desc)
    {
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    void toolTip(const char* desc)
    {
        if (ImGui::IsItemHovered())
        {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(desc);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    //returns true if texture was changed
    bool drawTextureSlot(const std::string label, std::uint32_t& dest, std::uint32_t thumbnail)
    {
        bool retVal = false;

        glm::vec2 imgSize = WindowLayouts[WindowID::MaterialSlot].second;
        if (ImGui::ImageButton((void*)(std::size_t)thumbnail, { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
        {
            if (cro::FileSystem::showMessageBox("Confirm", "Clear this slot? Drag a texture from the browser to set it", cro::FileSystem::YesNo, cro::FileSystem::Question))
            {
                dest = 0;
                retVal = true;
            }
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_SRC"))
            {
                auto old = dest;
                CRO_ASSERT(payload->DataSize == sizeof(std::uint32_t), "");
                dest = *(const std::uint32_t*)payload->Data;
                retVal = (old != dest);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();

        //seems a waste to keep calling this but hey
        auto textSize = ImGui::CalcTextSize("Aty");

        auto pos = ImGui::GetCursorPosY();
        pos += (imgSize.y - textSize.y) / 2.f;
        ImGui::SetCursorPosY(pos);
        ImGui::Text("%s", label.c_str());

        ImGui::SameLine();
        helpMarker("Drag a texture from the Texture Browser to fill the slot, or click the icon to clear it.");
        return retVal;
    }

    bool drawMaterialSlot(const std::string label, std::int32_t& dest, std::uint32_t thumbnail)
    {
        bool retVal = false;

        glm::vec2 imgSize = WindowLayouts[WindowID::MaterialSlot].second;
        if (ImGui::ImageButton((void*)(std::size_t)thumbnail, { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
        {
            if (cro::FileSystem::showMessageBox("Confirm", "Clear this slot? Drag a material from the browser to set it", cro::FileSystem::YesNo, cro::FileSystem::Question))
            {
                dest = -1;
                retVal = true;
            }
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MATERIAL_SRC"))
            {
                auto old = dest;
                CRO_ASSERT(payload->DataSize == sizeof(std::int32_t), "");
                dest = *(const std::uint32_t*)payload->Data;
                retVal = (dest != old);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::SameLine();

        //seems a waste to keep calling this but hey
        auto textSize = ImGui::CalcTextSize("Aty");

        auto pos = ImGui::GetCursorPosY();
        pos += (imgSize.y - textSize.y) / 2.f;
        ImGui::SetCursorPosY(pos);
        ImGui::Text("%s", label.c_str());

        ImGui::SameLine();
        helpMarker("Drag a texture from the Material Browser to fill the slot, or click the icon to clear it.");
        return retVal;
    }

    bool setInspectorTab = false;
    std::int32_t inspectorTabIndex = 0;
}

float updateView(cro::Entity entity, float farPlane, float fov)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.x -= (size.x * ui::InspectorWidth);
    size.y -= (size.y * ui::BrowserHeight);

    auto& cam3D = entity.getComponent<cro::Camera>();
    cam3D.setPerspective(fov, size.x / size.y, 0.1f, farPlane);
    cam3D.viewport.left = ui::InspectorWidth;
    cam3D.viewport.width = 1.f - ui::InspectorWidth;
    cam3D.viewport.bottom = ui::BrowserHeight;
    cam3D.viewport.height = 1.f - ui::BrowserHeight;

    return size.x / size.y;
}

void ModelState::showSaveMessage()
{
    if (cro::FileSystem::showMessageBox("", "Do you want to save the model first?", cro::FileSystem::YesNo, cro::FileSystem::Question))
    {
        if (m_currentFilePath.empty())
        {
            if (m_importedVBO.empty())
            {
                auto path = cro::FileSystem::saveFileDialogue(m_sharedData.workingDirectory + "/untitled", "cmt");
                if (!path.empty())
                {
                    saveModel(path);
                }
            }
            else
            {
                //export the model
                //TODO check for animated model
                exportStaticModel(false, false);
            }
        }
        else
        {
            saveModel(m_currentFilePath);
        }
    }
}

void ModelState::buildUI()
{
    loadPrefs();
    registerWindow([&]()
        {
            if (ImGui::BeginMainMenuBar())
            {
                //file menu
                if (ImGui::BeginMenu("File##Model"))
                {
                    if (ImGui::MenuItem("Open Model", nullptr, nullptr))
                    {
                        if (m_sharedData.workingDirectory.empty())
                        {
                            if (cro::FileSystem::showMessageBox("", "Working directory currently not set. Would you like to set one now?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                            {
                                auto path = cro::FileSystem::openFolderDialogue(m_sharedData.workingDirectory);
                                if (!path.empty())
                                {
                                    m_sharedData.workingDirectory = path;
                                    std::replace(m_sharedData.workingDirectory.begin(), m_sharedData.workingDirectory.end(), '\\', '/');
                                }
                            }
                        }

                        openModel();
                    }
                    if (ImGui::MenuItem("Save", nullptr, nullptr, !m_currentFilePath.empty()))
                    {
                        //if a model is open overwrite the model def with current materials
                        saveModel(m_currentFilePath);
                    }
                    if (ImGui::MenuItem("Save As...", nullptr, nullptr, !m_currentFilePath.empty()))
                    {
                        //if a model is open create a new model def with current materials
                        auto path = cro::FileSystem::saveFileDialogue(m_sharedData.workingDirectory + "/untitled", "cmt");
                        if (!path.empty())
                        {
                            saveModel(path);
                        }
                    }

                    if (ImGui::MenuItem("Close Model", nullptr, nullptr, m_entities[EntityID::ActiveModel].isValid()))
                    {
                        closeModel();
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Import Model", nullptr, nullptr))
                    {
                        importModel();
                    }
                    if (ImGui::MenuItem("Export Model", nullptr, nullptr, !m_importedVBO.empty()))
                    {
                        //TODO check for animations
                        exportStaticModel();
                    }

                    ImGui::Separator();

                    if (getStateCount() > 1)
                    {
                        if (ImGui::MenuItem("Return To World Editor"))
                        {
                            if (m_entities[EntityID::ActiveModel].isValid())
                            {
                                showSaveMessage();
                            }

                            getContext().mainWindow.setTitle("Crogine Editor");
                            requestStackPop();
                        }
                    }

                    if (ImGui::MenuItem("Quit", nullptr, nullptr))
                    {
                        if (m_entities[EntityID::ActiveModel].isValid())
                        {
                            showSaveMessage();
                        }

                        cro::App::quit();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Create"))
                {
                    if (ImGui::MenuItem("Quad", nullptr, nullptr))
                    {

                    }

                    if (ImGui::MenuItem("Sphere", nullptr, nullptr))
                    {

                    }

                    if (ImGui::MenuItem("Cube", nullptr, nullptr))
                    {

                    }

                    if (ImGui::MenuItem("Billboard", nullptr, nullptr))
                    {

                    }
                    ImGui::EndMenu();
                }

                //view menu
                if (ImGui::BeginMenu("View"))
                {
                    if (ImGui::MenuItem("Options", nullptr, nullptr))
                    {
                        m_showPreferences = true;
                    }
                    if (ImGui::MenuItem("Choose Skybox", nullptr, nullptr))
                    {
                        auto lastPath = m_sharedData.skymapTexture.empty() ? m_sharedData.workingDirectory + "/untitled.hdr" : m_sharedData.skymapTexture;

                        auto path = cro::FileSystem::openFileDialogue(lastPath, "hdr");
                        if (!path.empty())
                        {
                            if (m_environmentMap.loadFromFile(path))
                            {
                                m_sharedData.skymapTexture = path;
                            }
                        }
                    }
                    if (ImGui::MenuItem("Show Skybox", nullptr, &m_showSkybox))
                    {
                        if (m_showSkybox)
                        {
                            m_scene.enableSkybox();
                        }
                        else
                        {
                            m_scene.disableSkybox();
                        }
                        savePrefs();
                    }
                    if (ImGui::MenuItem("Ground Plane", nullptr, &m_showGroundPlane))
                    {
                        if (m_showGroundPlane)
                        {
                            //set this to whichever world scale we're currently using
                            //updateWorldScale();
                            m_entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
                        }
                        else
                        {
                            m_entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
                        }
                        savePrefs();
                    }
                    if (ImGui::MenuItem("Material Preview", nullptr, &m_showMaterialWindow))
                    {
                        savePrefs();
                    }
                    if (ImGui::MenuItem("AO Baker", nullptr, &m_showBakingWindow, m_entities[EntityID::ActiveModel].isValid() && m_importedVBO.empty()))
                    {

                    }
                    if (ImGui::MenuItem("Reset Camera"))
                    {
                        m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setLocalTransform(glm::mat4(1.f));
                        m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setPosition(DefaultArcballPosition);
                        m_fov = DefaultFOV;
                        updateView(m_scene.getActiveCamera(), DefaultFarPlane, m_fov);
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            //options window
            if (m_showPreferences)
            {
                ImGui::SetNextWindowSize({ 400.f, 260.f });
                if (ImGui::Begin("Preferences", &m_showPreferences))
                {
                    ImGui::Text("%s", "Working Directory:");
                    if (m_sharedData.workingDirectory.empty())
                    {
                        ImGui::Text("%s", "Not Set");
                    }
                    else
                    {
                        auto dir = m_sharedData.workingDirectory.substr(0, 30) + "...";
                        ImGui::Text("%s", dir.c_str());
                        ImGui::SameLine();
                        helpMarker(m_sharedData.workingDirectory.c_str());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse"))
                    {
                        auto path = cro::FileSystem::openFolderDialogue(m_sharedData.workingDirectory);
                        if (!path.empty())
                        {
                            m_sharedData.workingDirectory = path;
                            std::replace(m_sharedData.workingDirectory.begin(), m_sharedData.workingDirectory.end(), '\\', '/');
                        }
                    }

                    //ImGui::PushItemWidth(100.f);
                    ////world scale selection
                    //const char* items[] = { "0.01", "0.1", "1", "10", "100", "1000" };
                    //static const char* currentItem = items[m_preferences.unitsPerMetre];
                    //if (ImGui::BeginCombo("World Scale (units per metre)", currentItem))
                    //{
                    //    for (auto i = 0u; i < worldScales.size(); ++i)
                    //    {
                    //        bool selected = (currentItem == items[i]);
                    //        if (ImGui::Selectable(items[i], selected))
                    //        {
                    //            currentItem = items[i];
                    //            m_preferences.unitsPerMetre = i;
                    //            updateWorldScale();
                    //        }

                    //        if (selected)
                    //        {
                    //            ImGui::SetItemDefaultFocus();
                    //        }
                    //    }
                    //    ImGui::EndCombo();
                    //}
                    //ImGui::PopItemWidth();

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();


                    if (ImGui::ColorEdit3("Sky Top", m_preferences.skyTop.asArray()))
                    {
                        m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
                    }
                    if (ImGui::ColorEdit3("Sky Bottom", m_preferences.skyBottom.asArray()))
                    {
                        m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
                    }

                    ImGui::NewLine();
                    ImGui::NewLine();
                    if (!m_showPreferences ||
                        ImGui::Button("Close"))
                    {
                        savePrefs();
                        m_showPreferences = false;
                    }
                }
                ImGui::End();
            }

            //material window
            if (m_showMaterialWindow)
            {
                ImGui::SetNextWindowSize({ 528.f, 554.f });
                ImGui::Begin("Material Preview", &m_showMaterialWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
                if (!m_materialDefs.empty())
                {
                    ImGui::Image(m_materialDefs[m_selectedMaterial].previewTexture.getTexture(), { ui::PreviewTextureSize, ui::PreviewTextureSize }, { 0.f, 1.f }, { 1.f, 0.f });
                }
                ImGui::End();

                if (!m_showMaterialWindow)
                {
                    savePrefs();
                }
            }

            if (m_showBakingWindow && m_entities[EntityID::ActiveModel].isValid() && m_importedVBO.empty())
            {
                ImGui::SetNextWindowSize({ 528.f, 554.f });
                ImGui::Begin("AO Baker##0", &m_showBakingWindow, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

                const auto& meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
                if (meshData.attributes[cro::Mesh::UV1] > 0)
                {
                    //TODO show combo to select which UV coords to use
                }

                if (ImGui::Button("Bake"))
                {
                    bakeLightmap(/*TODO pass UV channel*/);
                }

                ImGui::Separator();

                for (auto i = 0u; i < meshData.submeshCount && i < m_lightmapTextures.size(); ++i) //only show tetures for the current model (there may be more from a previous bake)
                {
                    ImGui::Image(*m_lightmapTextures[i], { ui::PreviewTextureSize / 4.f, ui::PreviewTextureSize / 4.f }, { 0.f, 1.f }, { 1.f, 0.f });

                    if (i > 0 && (i % 3) != 0)
                    {
                        ImGui::SameLine();
                    }
                }

                if (!m_lightmapTextures.empty() && ImGui::Button("Save All"))
                {
                    auto path = cro::FileSystem::saveFileDialogue(m_sharedData.workingDirectory + "/" + "ao_untitled", "png");
                    if (!path.empty())
                    {
                        if (meshData.submeshCount > 1)
                        {
                            auto preString = path.substr(0, path.find_last_of('.'));
                            for (auto i = 0u; i < meshData.submeshCount; ++i)
                            {
                                m_lightmapTextures[i]->saveToFile(preString + "0" + std::to_string(i) + ".png");
                            }
                        }
                        else
                        {
                            m_lightmapTextures[0]->saveToFile(path);
                        }
                    }
                }

                ImGui::End();
            }

            drawInspector();
            drawBrowser();

            //ImGui::SetNextWindowSize({ 528.f, 554.f });
            //ImGui::Begin("Shadow Map", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

            ///*ImGui::Image(m_scene.getSystem<cro::ShadowMapRenderer>().getDepthMapTexture(),
            //    { ui::PreviewTextureSize, ui::PreviewTextureSize }, { 0.f, 1.f }, { 1.f, 0.f });*/

            //ImGui::End();

            ImGuiIO& io = ImGui::GetIO();
            ImGuizmo::SetRect(io.DisplaySize.x * ui::InspectorWidth, 0, io.DisplaySize.x - (ui::InspectorWidth * io.DisplaySize.x),
                io.DisplaySize.y - (ui::BrowserHeight * io.DisplaySize.y));


            auto [pos, size] = WindowLayouts[WindowID::ViewGizmo];

            //view cube doohickey
            auto tx = glm::inverse(m_entities[EntityID::ArcBall].getComponent<cro::Transform>().getLocalTransform());
            ImGuizmo::ViewManipulate(&tx[0][0], 10.f, ImVec2(pos.x, pos.y), ImVec2(size.x, size.y), 0/*x10101010*/);
            m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setRotation(glm::inverse(tx));

            //tooltip
            if (io.MousePos.x > pos.x && io.MousePos.x < pos.x + size.x
                && io.MousePos.y > pos.y && io.MousePos.y < pos.y + size.y)
            {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, 0);
                ImGui::PushStyleColor(ImGuiCol_Border, 0);
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);

                auto p = ImGui::GetCursorPos();
                ImGui::PushStyleColor(ImGuiCol_Text, 0xff000000);
                ImGui::TextUnformatted("Left Click Rotate\nMiddle Mouse Pan\nScroll Zoom");
                ImGui::PopStyleColor();

                p.x -= 1.f;
                p.y -= 1.f;
                ImGui::SetCursorPos(p);
                ImGui::TextUnformatted("Left Click Rotate\nMiddle Mouse Pan\nScroll Zoom");

                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
                ImGui::PopStyleColor(2);
            }
            //rotate the model if it's loaded
            if (m_entities[EntityID::ActiveModel].isValid()
                && m_importedVBO.empty())
            {
                const auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
                tx = m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().getLocalTransform();
                ImGuizmo::Manipulate(&cam.getActivePass().viewMatrix[0][0], &cam.getProjectionMatrix()[0][0], ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, &tx[0][0]);
                m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setLocalTransform(tx);
            }


            if (m_browseGLTF)
            {
                showGLTFBrowser();
            }
        });

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void ModelState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::BeginTabBar("##a");

        auto idx = 0;
        auto flags = (setInspectorTab && inspectorTabIndex == idx) ? ImGuiTabItemFlags_SetSelected : 0;

        if (ImGui::BeginTabItem("Model", nullptr, flags))
        {
            //model details
            if (m_entities[EntityID::ActiveModel].isValid())
            {
                if (!m_importedVBO.empty())
                {
                    ImGui::Text("Material / Submesh Count: %d", m_importedHeader.arrayCount);
                    ImGui::Separator();

                    std::string flags = "Vertex Attributes:\n";
                    if (m_importedHeader.flags & cro::VertexProperty::Position)
                    {
                        flags += "  Position\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::Colour)
                    {
                        flags += "  Colour\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::Normal)
                    {
                        flags += "  Normal\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::Tangent)
                    {
                        flags += "  Tan/Bitan\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::UV0)
                    {
                        flags += "  Texture Coords\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::UV1)
                    {
                        flags += "  Shadowmap Coords\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::BlendIndices)
                    {
                        flags += "  Blend Indices\n";
                    }
                    if (m_importedHeader.flags & cro::VertexProperty::BlendWeights)
                    {
                        flags += "  Blend Weights\n";
                    }
                    ImGui::Text("%s", flags.c_str());

                    ImGui::NewLine();
                    ImGui::Separator();

                    if (m_importedHeader.animated)
                    {
                        auto& skel = m_entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();
                        ImGui::Text("%lu Animation(s)", skel.animations.size());
                        int buns = 232131;
                        int animID = 0;
                        for (const auto& anim : skel.animations)
                        {
                            auto name = anim.name.substr(0, 16);
                            ImGui::Text("%s\n Frames: %lu", name.c_str(), anim.frameCount);

                            ImGui::SameLine();
                            std::string label = "<##" + std::to_string(buns++);
                            if (ImGui::Button(label.c_str())
                                && animID == skel.getCurrentAnimation()
                                && skel.animations[skel.getCurrentAnimation()].playbackRate == 0)
                            {
                                skel.prevFrame();
                            }
                            toolTip("Previous Frame");
                            ImGui::SameLine();

                            if (skel.animations[skel.getCurrentAnimation()].playbackRate != 0)
                            {
                                //pause button
                                label = "Pause##" + std::to_string(buns++);
                                if (ImGui::Button(label.c_str(), ImVec2(50.f, 22.f)))
                                {
                                    skel.stop();
                                }
                            }
                            else
                            {
                                //play button
                                label = "Play##" + std::to_string(buns++);
                                if (ImGui::Button(label.c_str(), ImVec2(50.f, 22.f)))
                                {
                                    skel.play(animID);
                                }
                            }

                            ImGui::SameLine();
                            label = ">##" + std::to_string(buns++);
                            if (ImGui::Button(label.c_str())
                                && animID == skel.getCurrentAnimation()
                                && skel.animations[skel.getCurrentAnimation()].playbackRate == 0)
                            {
                                skel.nextFrame();
                            }
                            toolTip("Next Frame");
                            animID++;

                        }
                        ImGui::Text("Current Frame: %lu", skel.animations[skel.getCurrentAnimation()].currentFrame - skel.animations[skel.getCurrentAnimation()].startFrame);

                        ImGui::NewLine();
                        ImGui::Separator();
                    }


                    ImGui::NewLine();
                    ImGui::Text("Transform"); ImGui::SameLine(); helpMarker("Double Click to change Values");
                    if (ImGui::DragFloat3("Rotation", &m_importedTransform.rotation[0], 1.f, -180.f, 180.f))
                    {
                        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setRotation(cro::Transform::Z_AXIS, m_importedTransform.rotation.z * cro::Util::Const::degToRad);
                        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, m_importedTransform.rotation.y * cro::Util::Const::degToRad);
                        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, m_importedTransform.rotation.x * cro::Util::Const::degToRad);
                    }
                    if (ImGui::DragFloat("Scale", &m_importedTransform.scale, 0.01f, 0.1f, 10.f))
                    {
                        //scale needs to be uniform, else we'd have to recalc all the normal data
                        m_importedTransform.scale = std::min(10.f, std::max(0.1f, m_importedTransform.scale));
                        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }
                    ImGui::Text("Quick Scale:"); ImGui::SameLine();
                    if (ImGui::Button("0.5"))
                    {
                        m_importedTransform.scale = std::max(0.1f, m_importedTransform.scale / 2.f);
                        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }ImGui::SameLine();
                    if (ImGui::Button("2.0"))
                    {
                        m_importedTransform.scale = std::min(10.f, m_importedTransform.scale * 2.f);
                        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }
                    if (ImGui::Button("Apply Transform"))
                    {
                        applyImportTransform();
                    }
                    ImGui::SameLine();
                    helpMarker("Applies this transform directly to the vertex data, before exporting the model.\nUseful if an imported model uses z-up coordinates, or is much\nlarger or smaller than other models in the scene.\nTIP: if a model doesn't scale enough in either direction try applying the current scale first before rescaling");

                    ImGui::NewLine();
                    static bool modelOnly = false;
                    static bool exportAnimation = true;
                    ImGui::Checkbox("Export Model Only", &modelOnly);
                    ImGui::SameLine();
                    helpMarker("If this is checked then only the model data will exported to the crogine file, leaving any existing material data in tact.");
                    if (m_importedHeader.animated)
                    {
                        ImGui::Checkbox("Export Animations", &exportAnimation);
                    }
                    if (ImGui::Button("Convert##01"))
                    {
                        if (m_importedHeader.animated
                            && exportAnimation)
                        {

                        }
                        else
                        {
                            //TODO we can't do this if the vertex data
                            //contains blend information.
                            exportStaticModel(modelOnly);
                        }
                    }
                    ImGui::SameLine();
                    helpMarker("Export this model to Crogine format, and create a model definition file.\nThe model will then be automatically re-opened for material editing.");
                }
                else
                {
                    ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                    ImGui::InputText("##name_model", &m_modelProperties.name);
                    ImGui::PopItemWidth();

                    switch (m_modelProperties.type)
                    {
                    default:

                        break;
                    case ModelProperties::Sphere:
                        ImGui::Text("Radius: %2.2f", m_modelProperties.radius);
                        break;
                    case ModelProperties::Quad:
                        ImGui::Text("Size: %2.2f, %2.2f", m_modelProperties.size.x, m_modelProperties.size.y);
                        ImGui::Text("UV: %2.2f, %2.2f, %2.2f, %2.2f", m_modelProperties.uv.x, m_modelProperties.uv.y, m_modelProperties.uv.z, m_modelProperties.uv.w);
                        break;
                    case ModelProperties::Cube:
                        ImGui::Text("Size: %2.2f, %2.2f, %2.2f", m_modelProperties.size.x, m_modelProperties.size.y, m_modelProperties.size.z);
                        break;
                    case ModelProperties::Billboard:
                        if (ImGui::Checkbox("Lock Rotation", &m_modelProperties.lockRotation))
                        {
                            auto& matDef = m_materialDefs[m_selectedMaterial];
                            if (m_modelProperties.lockRotation)
                            {
                                matDef.shaderFlags |= cro::ShaderResource::LockRotation;
                            }
                            else
                            {
                                matDef.shaderFlags &= ~cro::ShaderResource::LockRotation;
                            }
                            refreshMaterialThumbnail(matDef);
                        }
                        if (ImGui::Checkbox("Lock Scale", &m_modelProperties.lockScale))
                        {
                            auto& matDef = m_materialDefs[m_selectedMaterial];
                            if (m_modelProperties.lockScale)
                            {
                                matDef.shaderFlags |= cro::ShaderResource::LockScale;
                            }
                            else
                            {
                                matDef.shaderFlags &= ~cro::ShaderResource::LockScale;
                            }
                            refreshMaterialThumbnail(matDef);
                        }
                        break;
                    }

                    if (ImGui::Checkbox("Cast Shadow", &m_modelProperties.castShadows))
                    {
                        m_entities[EntityID::ActiveModel].getComponent<cro::ShadowCaster>().active = m_modelProperties.castShadows;
                    }

                    ImGui::NewLine();
                    ImGui::Separator();
                    ImGui::NewLine();

                    const auto& meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
                    ImGui::Text("Materials:");
                    CRO_ASSERT(meshData.submeshCount <= m_activeMaterials.size(), "");
                    for (auto i = 0u; i < meshData.submeshCount; ++i)
                    {
                        std::uint32_t texID = m_magentaTexture.getGLHandle();
                        std::string matName = "Default";
                        if (m_activeMaterials[i] > -1)
                        {
                            texID = m_materialDefs[m_activeMaterials[i]].previewTexture.getTexture().getGLHandle();
                            matName = m_materialDefs[m_activeMaterials[i]].name;
                        }

                        auto oldIndex = m_activeMaterials[i];
                        if (drawMaterialSlot(matName, m_activeMaterials[i], texID))
                        {
                            //remove this index from the material which was unassigned
                            if (oldIndex > -1)
                            {
                                auto& oldMat = m_materialDefs[oldIndex];
                                oldMat.submeshIDs.erase(std::remove_if(oldMat.submeshIDs.begin(), oldMat.submeshIDs.end(),
                                    [i](std::int32_t a)
                                    {
                                        return a == i;
                                    }), oldMat.submeshIDs.end());
                            }

                            //add this index to the new material
                            if (m_activeMaterials[i] > -1)
                            {
                                m_materialDefs[m_activeMaterials[i]].submeshIDs.push_back(i);

                                //and update the actual model
                                m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, m_materialDefs[m_activeMaterials[i]].materialData);
                            }
                            else
                            {
                                //apply default material
                                m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, m_resources.materials.get(m_materialIDs[MaterialID::Default]));
                            }
                        }
                    }
                    ImGui::NewLine();
                    ImGui::Text("Vertex Attributes:");
                    for (auto i = 0u; i < meshData.attributes.size(); ++i)
                    {
                        auto colour = ImGui::GetStyle().Colors[ImGuiCol_TextDisabled];
                        if (meshData.attributes[i] > 0)
                        {

                            colour = ImGui::GetStyle().Colors[ImGuiCol_Text];
                        }
                        ImGui::PushStyleColor(0, colour);
                        ImGui::Text("%s", AttribStrings[i]);
                        ImGui::PopStyleColor();
                    }

                    ImGui::NewLine();
                    ImGui::Separator();
                    bool refreshBounds = false;
                    if (ImGui::Checkbox("Show AABB", &m_showAABB))
                    {
                        //toggle box
                        refreshBounds = true;
                    }

                    if (ImGui::Checkbox("Show Bounding Sphere", &m_showSphere))
                    {
                        //toggle sphere display
                        refreshBounds = true;
                    }
                    if (refreshBounds)
                    {
                        std::optional<cro::Sphere> sphere;
                        if (m_showSphere) sphere = meshData.boundingSphere;

                        std::optional<cro::Box> box;
                        if (m_showAABB) box = meshData.boundingBox;

                        updateGridMesh(m_entities[EntityID::GridMesh].getComponent<cro::Model>().getMeshData(), sphere, box);
                    }

                    if (m_entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
                    {
                        auto& skeleton = m_entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();

                        ImGui::NewLine();
                        ImGui::Separator();
                        ImGui::NewLine();

                        static bool showSkeleton = false;
                        if (ImGui::Checkbox("Show Skeleton", &showSkeleton))
                        {
                            //TODO toggle skeleton display
                        }

                        ImGui::Text("Animations: %ld", skeleton.animations.size());
                        static std::string label("Stopped");
                        if (skeleton.animations.empty())
                        {
                            label = "No Animations Found.";
                        }
                        else
                        {
                            static int currentAnim = 0;
                            auto prevAnim = currentAnim;

                            if (ImGui::InputInt("Anim", &currentAnim, 1, 1)
                                && skeleton.animations[currentAnim].playbackRate != 0)
                            {
                                currentAnim = std::min(currentAnim, static_cast<int>(skeleton.animations.size()) - 1);
                            }
                            else
                            {
                                currentAnim = prevAnim;
                            }

                            ImGui::SameLine();
                            if (skeleton.animations[currentAnim].playbackRate != 0)
                            {
                                if (ImGui::Button("Stop"))
                                {
                                    skeleton.stop();
                                    label = "Stopped";
                                }
                            }
                            else
                            {
                                if (ImGui::Button("Play"))
                                {
                                    skeleton.play(currentAnim);
                                    label = "Playing " + skeleton.animations[currentAnim].name;
                                }
                                else
                                {
                                    label = "Stopped";
                                }
                            }
                        }
                        ImGui::Text("%s", label.c_str());
                    }
                }
            }

            ImGui::EndTabItem();
        }

        idx++;
        flags = (setInspectorTab && inspectorTabIndex == idx) ? ImGuiTabItemFlags_SetSelected : 0;

        if (ImGui::BeginTabItem("Material", nullptr, flags))
        {
            if (!m_materialDefs.empty())
            {
                auto& matDef = m_materialDefs[m_selectedMaterial];

                ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                ImGui::InputText("##name", &matDef.name);
                ImGui::PopItemWidth();

                auto imgSize = size;
                imgSize.x *= ui::MaterialPreviewWidth;
                imgSize.y = imgSize.x;

                ImGui::SetCursorPos({ pos.x + ((size.x - imgSize.y) / 2.f), pos.y + 60.f });
                ImGui::Image((void*)(std::size_t)m_materialDefs[m_selectedMaterial].previewTexture.getTexture().getGLHandle(),
                    { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f });

                static float rotation = 0.f;
                ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                if (ImGui::SliderFloat("##rotation", &rotation, -180.f, 180.f))
                {
                    m_previewEntity.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation * cro::Util::Const::degToRad);
                }
                ImGui::PopItemWidth();
                static bool quad = false;
                if (quad)
                {
                    if (ImGui::Button("Sphere"))
                    {
                        m_previewEntity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.f });
                        quad = false;
                    }
                }
                else
                {
                    if (ImGui::Button("Quad"))
                    {
                        m_previewEntity.getComponent<cro::Transform>().setPosition({ 0.f, -1.5f, 0.f });
                        quad = true;
                    }
                }

                ImGui::NewLine();
                ImGui::Text("Shader Type:");
                ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                auto oldType = matDef.type;
                if (ImGui::BeginCombo("##Shader", ShaderStrings[matDef.type]))
                {
                    for (auto i = 0u; i < ShaderStrings.size(); ++i)
                    {
                        bool selected = matDef.type == i;
                        if (ImGui::Selectable(ShaderStrings[i], selected))
                        {
                            matDef.type = static_cast<MaterialDefinition::Type>(i);
                        }

                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                auto type = matDef.type;
                if (oldType != type)
                {
                    //set the corresponding default mask colour
                    if (type == MaterialDefinition::PBR)
                    {
                        //metal/rough/ao
                        matDef.maskColour = glm::vec4(0.f, 0.f, 1.f, 1.f);
                    }
                    else
                    {
                        //spec intens/spec amount/self illum
                        matDef.maskColour = glm::vec4(1.f, 1.f, 0.f, 1.f);
                    }
                }

                std::int32_t shaderFlags = 0;
                bool applyMaterial = false;

                ImGui::NewLine();
                ImGui::Text("Texture Maps:");

                std::string slotLabel("Diffuse");
                std::uint32_t thumb = m_blackTexture.getGLHandle();

                //diffuse map
                if (type == MaterialDefinition::PBR)
                {
                    slotLabel = "Albedo";
                }

                auto prevDiffuse = matDef.textureIDs[MaterialDefinition::Diffuse];
                if (matDef.textureIDs[MaterialDefinition::Diffuse] == 0)
                {
                    slotLabel += ": Empty";
                }
                else
                {
                    slotLabel += ": " + m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Diffuse]).name;
                    thumb = matDef.textureIDs[MaterialDefinition::Diffuse];
                }
                if (drawTextureSlot(slotLabel, matDef.textureIDs[MaterialDefinition::Diffuse], thumb))
                {
                    applyMaterial = true;
                }

                //drawTextureSlot() may have updated this
                if (matDef.textureIDs[MaterialDefinition::Diffuse] != 0)
                {
                    shaderFlags |= cro::ShaderResource::DiffuseMap;

                    //if this slot was previously empty we probably want to set the colour to white
                    if (prevDiffuse == 0)
                    {
                        matDef.colour = glm::vec4(1.f);
                    }
                }

                if (type != MaterialDefinition::PBR)
                {
                    //lightmap
                    slotLabel = "Light Map";
                    if (matDef.textureIDs[MaterialDefinition::Lightmap] == 0)
                    {
                        slotLabel += ": Empty";
                        thumb = m_blackTexture.getGLHandle();
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Lightmap]).name;
                        thumb = matDef.textureIDs[MaterialDefinition::Lightmap];
                    }
                    if (drawTextureSlot(slotLabel, matDef.textureIDs[MaterialDefinition::Lightmap], thumb))
                    {
                        applyMaterial = true;
                    }

                    //warn if model has no lightmap coords
                    if (m_entities[EntityID::ActiveModel].isValid())
                    {
                        const auto& meshData = m_entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
                        if (meshData.attributes[cro::Mesh::UV1] == 0)
                        {
                            ImGui::SameLine();
                            ImGui::PushStyleColor(ImGuiCol_Text, { 1.f, 0.f, 0.f, 1.f });
                            ImGui::Text("(!!)");
                            ImGui::PopStyleColor();

                            if (ImGui::IsItemHovered())
                            {
                                ImGui::BeginTooltip();
                                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                                ImGui::TextUnformatted("Current Model Has No Lightmap Texture Coordinates.");
                                ImGui::PopTextWrapPos();
                                ImGui::EndTooltip();
                            }
                        }
                    }

                    if (matDef.textureIDs[MaterialDefinition::Lightmap] != 0)
                    {
                        shaderFlags |= cro::ShaderResource::LightMap;
                    }
                }

                if (type != MaterialDefinition::Unlit)
                {
                    //mask map
                    slotLabel = "Mask Map";
                    if (matDef.textureIDs[MaterialDefinition::Mask] == 0)
                    {
                        slotLabel += ": Empty";
                        thumb = m_blackTexture.getGLHandle();
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Mask]).name;
                        thumb = matDef.textureIDs[MaterialDefinition::Mask];
                    }

                    if (drawTextureSlot(slotLabel, matDef.textureIDs[MaterialDefinition::Mask], thumb))
                    {
                        applyMaterial = true;
                    }

                    if (matDef.textureIDs[MaterialDefinition::Mask] != 0)
                    {
                        shaderFlags |= cro::ShaderResource::MaskMap;
                    }

                    //normal map
                    slotLabel = "Normal Map";
                    if (matDef.textureIDs[MaterialDefinition::Normal] == 0)
                    {
                        slotLabel += ": Empty";
                        thumb = m_blackTexture.getGLHandle();
                    }
                    else
                    {
                        slotLabel += ": " + m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Normal]).name;
                        thumb = matDef.textureIDs[MaterialDefinition::Normal];
                    }

                    if (drawTextureSlot(slotLabel, matDef.textureIDs[MaterialDefinition::Normal], thumb))
                    {
                        applyMaterial = true;
                    }

                    if (matDef.textureIDs[MaterialDefinition::Normal] != 0)
                    {
                        shaderFlags |= cro::ShaderResource::NormalMap;
                    }
                }

                ImGui::NewLine();
                /*if (type != MaterialDefinition::PBR
                    || matDef.textureIDs[MaterialDefinition::Diffuse] == 0
                    )*/
                {
                    if (ImGui::ColorEdit4("Diffuse Colour", matDef.colour.asArray()))
                    {
                        applyMaterial = true;
                    }
                    ImGui::SameLine();
                    if (type != MaterialDefinition::PBR)
                    {
                        helpMarker("Defines the diffuse colour of the material. This is multiplied with any diffuse map which may be assigned to the material.");
                    }
                    else
                    {
                        helpMarker("Defines the albedo colour or the material. This is multiplied with the albedo map if one is assigned.");
                    }

                    shaderFlags |= cro::ShaderResource::DiffuseColour;
                }
                if (type != MaterialDefinition::Unlit
                    && matDef.textureIDs[MaterialDefinition::Mask] == 0)
                {
                    if (ImGui::ColorEdit3("Mask Colour", matDef.maskColour.asArray()))
                    {
                        applyMaterial = true;
                    }
                    ImGui::SameLine();
                    if (type == MaterialDefinition::VertexLit)
                    {
                        helpMarker("Defines the mask colour of the material. Specular intensity is stored in the red channel, specular amount in the green channel, and self-illumination is stored in the blue channel");
                    }
                    else
                    {
                        helpMarker("Defines the mask colour of the material in lieu of a mask map. The metallic value is stored n the red channel, roughness in the blue channel and AO in the green channel");
                    }
                }

                if (matDef.textureIDs[MaterialDefinition::Diffuse])
                {
                    ImGui::NewLine();
                    if (ImGui::SliderFloat("Alpha Clip", &matDef.alphaClip, 0.f, 1.f))
                    {
                        applyMaterial = true;
                    }
                    ImGui::SameLine();
                    helpMarker("Alpha values of the diffuse colour below this value will cause the current fragment to be discarded");
                    if (matDef.alphaClip > 0)
                    {
                        shaderFlags |= cro::ShaderResource::AlphaClip;
                    }
                }

                ImGui::Checkbox("Receive Shadows", &matDef.receiveShadows);
                ImGui::SameLine();
                helpMarker("Check this box if the material should receive shadows from the active shadow map");
                if (matDef.receiveShadows)
                {
                    shaderFlags |= cro::ShaderResource::RxShadows;
                }

                if (type != MaterialDefinition::PBR)
                {
                    ImGui::NewLine();
                    ImGui::Checkbox("Use Vertex Colours", &matDef.vertexColoured);
                    ImGui::SameLine();
                    helpMarker("Any colour information stored in the model's vertex data will be multiplied with the diffuse colour of the material");
                    if (matDef.vertexColoured)
                    {
                        shaderFlags |= cro::ShaderResource::VertexColour;
                    }

                    ImGui::NewLine();
                    ImGui::Checkbox("Use Rimlighting", &matDef.useRimlighing);
                    ImGui::SameLine();
                    helpMarker("Enable the rim lighting effect. Not available with the PBR shader");
                    if (matDef.useRimlighing)
                    {
                        shaderFlags |= cro::ShaderResource::RimLighting;

                        if (ImGui::SliderFloat("Rim Falloff", &matDef.rimlightFalloff, 0.f, 1.f))
                        {
                            applyMaterial = true;
                        }
                        ImGui::SameLine();
                        helpMarker("Adjusts the point at which the rimlight falloff affects the fade");

                        if (ImGui::ColorEdit3("Rimlight Colour", matDef.rimlightColour.asArray()))
                        {
                            applyMaterial = true;
                        }
                        ImGui::SameLine();
                        helpMarker("Sets the colour of the rimlight effect, if it is enabled");
                    }
                }

                if (shaderFlags & (cro::ShaderResource::DiffuseMap | cro::ShaderResource::NormalMap | cro::ShaderResource::LightMap | cro::ShaderResource::MaskMap))
                {
                    ImGui::NewLine();
                    ImGui::Checkbox("Use Subrect", &matDef.useSubrect);
                    ImGui::SameLine();
                    helpMarker("Tell this material to use the given subrect in normalised coords when drawing a texture. Coords are given as Left, Bottom, Width and Height");

                    if (ImGui::InputFloat4("SubRect", &matDef.subrect[0]))
                    {
                        applyMaterial = true;
                    }
                }
                if (matDef.useSubrect)
                {
                    shaderFlags |= cro::ShaderResource::Subrects;
                }

                ImGui::NewLine();
                ImGui::Text("Blend Mode:");
                ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                auto oldMode = matDef.blendMode;
                if (ImGui::BeginCombo("##BlendMode", BlendStrings[static_cast<std::int32_t>(matDef.blendMode)]))
                {
                    for (auto i = 0u; i < BlendStrings.size(); ++i)
                    {
                        bool selected = static_cast<std::int32_t>(m_materialDefs[m_selectedMaterial].blendMode) == i;
                        if (ImGui::Selectable(BlendStrings[i], selected))
                        {
                            m_materialDefs[m_selectedMaterial].blendMode = static_cast<cro::Material::BlendMode>(i);
                        }

                        if (selected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }

                    ImGui::EndCombo();
                }
                if (oldMode != matDef.blendMode)
                {
                    applyMaterial = true;
                }
                ImGui::PopItemWidth();

                ImGui::NewLine();
                if (ImGui::Button("Export"))
                {
                    exportMaterial();
                }
                ImGui::SameLine();
                helpMarker("Export this material to a Material Definition file which can be loaded by the Material Browser");

                if (m_entities[EntityID::ActiveModel].isValid()
                    && m_entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
                {
                    shaderFlags |= cro::ShaderResource::Skinning;
                }

                if (m_modelProperties.lockRotation)
                {
                    shaderFlags |= cro::ShaderResource::LockRotation;
                }

                if (m_modelProperties.lockScale)
                {
                    shaderFlags |= cro::ShaderResource::LockScale;
                }

                if (matDef.shaderFlags != shaderFlags
                    || matDef.activeType != matDef.type)
                {
                    auto shaderType = cro::ShaderResource::Unlit;
                    if (m_modelProperties.type == ModelProperties::Billboard)
                    {
                        shaderType = cro::ShaderResource::BillboardUnlit;
                    }

                    if (matDef.type == MaterialDefinition::VertexLit)
                    {
                        shaderType = cro::ShaderResource::VertexLit;

                        if (m_modelProperties.type == ModelProperties::Billboard)
                        {
                            shaderType = cro::ShaderResource::BillboardVertexLit;
                        }
                    }
                    else if (matDef.type == MaterialDefinition::PBR)
                    {
                        shaderType = cro::ShaderResource::PBR;
                    }

                    matDef.shaderID = m_resources.shaders.loadBuiltIn(shaderType, shaderFlags);
                    matDef.shaderFlags = shaderFlags;
                    matDef.activeType = matDef.type;

                    matDef.materialData.setShader(m_resources.shaders.get(matDef.shaderID));
                    applyMaterial = true;
                }

                if (applyMaterial)
                {
                    applyPreviewSettings(m_materialDefs[m_selectedMaterial]);

                    //we have to always do this because we might have switched to a new
                    //material and they all share the  same preview mesh
                    m_previewEntity.getComponent<cro::Model>().setMaterial(0, matDef.materialData);

                    //if no model is open then this should be empty, so skip checking validity of the
                    //destination model.
                    for (auto idx : matDef.submeshIDs)
                    {
                        m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(idx, matDef.materialData);
                    }
                }
            }

            ImGui::EndTabItem();
        }

        idx++;
        flags = (setInspectorTab && inspectorTabIndex == idx) ? ImGuiTabItemFlags_SetSelected : 0;

        if (ImGui::BeginTabItem("Texture", nullptr, flags))
        {
            if (!m_materialTextures.empty())
            {
                const auto& texture = m_materialTextures.at(m_selectedTexture);
                ImGui::Text("%s", texture.name.c_str());

                auto imgSize = size;
                imgSize.x *= ui::TexturePreviewWidth;
                imgSize.y = imgSize.x;

                ImGui::SetCursorPos({ pos.x + ((size.x - imgSize.y) / 2.f), pos.y + 60.f });
                ImGui::Image((void*)(std::size_t)m_selectedTexture, { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f });

                ImGui::NewLine();

                auto texSize = texture.texture->getSize();
                ImGui::Text("Size: %u, %u", texSize.x, texSize.y);

                bool smooth = texture.texture->isSmooth();
                if (ImGui::Checkbox("Smoothed", &smooth))
                {
                    texture.texture->setSmooth(smooth);

                    //if this texture is used in the active material
                    //update the material definition
                    if (!m_materialDefs.empty())
                    {
                        for (auto t : m_materialDefs[m_selectedMaterial].textureIDs)
                        {
                            if (t == m_selectedTexture)
                            {
                                m_materialDefs[m_selectedMaterial].smoothTexture = smooth;
                            }
                        }
                    }
                }

                bool repeated = texture.texture->isRepeated();
                if (ImGui::Checkbox("Repeated", &repeated))
                {
                    texture.texture->setRepeated(repeated);

                    //if this texture is used in the active material
                    //update the material definition
                    if (!m_materialDefs.empty())
                    {
                        for (auto t : m_materialDefs[m_selectedMaterial].textureIDs)
                        {
                            if (t == m_selectedTexture)
                            {
                                m_materialDefs[m_selectedMaterial].repeatTexture = repeated;
                            }
                        }
                    }
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        setInspectorTab = false;
    }
    ImGui::End();
}

void ModelState::drawBrowser()
{
    auto [pos, size] = WindowLayouts[WindowID::Browser];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("##b");

        if (ImGui::BeginTabItem("Materials"))
        {
            if (ImGui::Button("Add##01"))
            {
                if (m_materialDefs.size() < ui::MaxMaterials)
                {
                    m_materialDefs.emplace_back();
                    m_materialDefs.back().materialData = m_resources.materials.get(m_materialIDs[MaterialID::Default]);
                    m_selectedMaterial = m_materialDefs.size() - 1;
                }
                else
                {
                    cro::FileSystem::showMessageBox("Error", "Max Materials Have Been Added.");
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Open##01"))
            {
                auto path = cro::FileSystem::openFileDialogue(m_sharedData.workingDirectory + "/untitled", "mdf,cmt");
                if (!path.empty())
                {
                    importMaterial(path);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove##01"))
            {
                if (!m_materialDefs.empty()
                    && cro::FileSystem::showMessageBox("Delete?", "Remove the selected material?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                {
                    //if the material is in use remove it from the model
                    if (!m_materialDefs[m_selectedMaterial].submeshIDs.empty())
                    {
                        auto defMat = m_resources.materials.get(m_materialIDs[MaterialID::Default]);
                        for (auto i : m_materialDefs[m_selectedMaterial].submeshIDs)
                        {
                            m_activeMaterials[i] = -1;
                            m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, defMat);
                        }
                    }

                    m_materialDefs.erase(m_materialDefs.begin() + m_selectedMaterial);
                    if (m_selectedMaterial >= m_materialDefs.size()
                        && !m_materialDefs.empty())
                    {
                        m_selectedMaterial--;
                    }

                    //update the active materials so they point to the correct slot
                    for (auto& i : m_activeMaterials)
                    {
                        if (i > m_selectedMaterial)
                        {
                            i--;
                        }
                    }

                    //update the newly selected thumbnail
                    if (!m_materialDefs.empty())
                    {
                        refreshMaterialThumbnail(m_materialDefs[m_selectedMaterial]);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Export##01"))
            {
                exportMaterial();
            }
            ImGui::SameLine();
            helpMarker("Export this material to a Material Definition file which can be loaded by the Material Browser");


            ImGui::BeginChild("##matChild");

            static std::size_t lastSelected = std::numeric_limits<std::uint32_t>::max();

            auto thumbSize = size;
            thumbSize.y *= ui::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            auto frameSize = thumbSize;
            frameSize.x += ImGui::GetStyle().FramePadding.x * ui::FramePadding.x;
            frameSize.y += ImGui::GetStyle().FramePadding.y * ui::FramePadding.y;

            std::int32_t thumbsPerRow = static_cast<std::int32_t>(std::floor(size.x / frameSize.x) - 1);

            std::int32_t count = 0;
            for (const auto& material : m_materialDefs)
            {
                ImVec4 colour(0.f, 0.f, 0.f, 1.f);
                if (count == m_selectedMaterial)
                {
                    colour = { 1.f, 1.f, 0.f, 1.f };
                    if (lastSelected != m_selectedMaterial)
                    {
                        ImGui::SetScrollHereY();
                    }
                }

                ImGui::BeginChildFrame(7767 + count, { frameSize.x, frameSize.y }, ImGuiWindowFlags_NoScrollbar);

                ImGui::PushStyleColor(ImGuiCol_Border, colour);
                ImGui::PushID(9999 + count);
                if (ImGui::ImageButton((void*)(std::size_t)material.previewTexture.getTexture().getGLHandle(), { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
                {
                    m_selectedMaterial = count;
                    ImGui::SetScrollHereY();
                }
                ImGui::PopID();
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    //set payload to carry the index of the material
                    ImGui::SetDragDropPayload("MATERIAL_SRC", &count, sizeof(cro::int32));

                    //display preview
                    ImGui::Image((void*)(std::size_t)material.previewTexture.getTexture().getGLHandle(), { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f });
                    ImGui::Text("%s", material.name.c_str());
                    ImGui::Text("%s", ShaderStrings[material.type]);
                    ImGui::EndDragDropSource();

                    m_selectedMaterial = count;
                    ImGui::SetScrollHereY();
                }

                ImGui::Text("%s", material.name.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", material.name.c_str());
                    ImGui::Text("%s", ShaderStrings[material.type]);
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

            if (lastSelected != m_selectedMaterial)
            {
                applyPreviewSettings(m_materialDefs[m_selectedMaterial]);
                m_previewEntity.getComponent<cro::Model>().setMaterial(0, m_materialDefs[m_selectedMaterial].materialData);
            }

            lastSelected = m_selectedMaterial;

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Textures"))
        {
            if (ImGui::Button("Add##00"))
            {
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp", true);
                if (!path.empty()
                    && m_materialTextures.size() < ui::MaxMaterials)
                {
                    auto files = cro::Util::String::tokenize(path, '|');
                    for (const auto& f : files)
                    {
                        addTextureToBrowser(f);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove##00"))
            {
                if (m_selectedTexture != 0
                    && cro::FileSystem::showMessageBox("Delete?", "Remove this texture?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                {
                    //remove from any materials using this
                    for (auto& mat : m_materialDefs)
                    {
                        for (auto& texID : mat.textureIDs)
                        {
                            if (texID == m_selectedTexture)
                            {
                                texID = 0;
                            }
                        }
                    }

                    m_materialTextures.erase(m_selectedTexture);
                    m_selectedTexture = m_materialTextures.empty() ? 0 : m_materialTextures.begin()->first;

                    for (auto& def : m_materialDefs)
                    {
                        refreshMaterialThumbnail(def);

                        //if no model is open then this should be empty, so skip checking validity of the
                        //destination model.
                        for (auto idx : def.submeshIDs)
                        {
                            m_entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(idx, def.materialData);
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload##00"))
            {
                if (m_selectedTexture != 0)
                {
                    auto path = m_sharedData.workingDirectory + "/" + m_materialTextures.at(m_selectedTexture).relPath + m_materialTextures.at(m_selectedTexture).name;
                    if (cro::FileSystem::fileExists(path))
                    {
                        if (!m_materialTextures.at(m_selectedTexture).texture->loadFromFile(path))
                        {
                            cro::FileSystem::showMessageBox("Error", "Could not refresh texture. Has the file been removed?");
                        }
                    }
                }
            }

            ImGui::BeginChild("##texChild");

            static std::size_t lastSelected = 0;

            auto thumbSize = size;
            thumbSize.y *= ui::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            auto frameSize = thumbSize;
            frameSize.x += ImGui::GetStyle().FramePadding.x * ui::FramePadding.x;
            frameSize.y += ImGui::GetStyle().FramePadding.y * ui::FramePadding.y;

            std::int32_t thumbsPerRow = static_cast<std::int32_t>(std::floor(size.x / frameSize.x) - 1);
            std::int32_t count = 0;
            std::uint32_t removeID = 0;
            for (const auto& [id, tex] : m_materialTextures)
            {
                ImVec4 colour(0.f, 0.f, 0.f, 1.f);
                if (id == m_selectedTexture)
                {
                    colour = { 1.f, 1.f, 0.f, 1.f };
                    if (lastSelected != m_selectedTexture)
                    {
                        ImGui::SetScrollHereY();
                    }
                }

                ImGui::BeginChildFrame(8394854 + id, { frameSize.x, frameSize.y }, ImGuiWindowFlags_NoScrollbar);

                ImGui::PushStyleColor(ImGuiCol_Border, colour);
                if (ImGui::ImageButton((void*)(std::size_t)id, { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f }))
                {
                    m_selectedTexture = id;
                    ImGui::SetScrollHereY();
                }
                ImGui::PopStyleColor();

                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                {
                    //set payload to carry the GL id of the texture
                    ImGui::SetDragDropPayload("TEXTURE_SRC", &id, sizeof(cro::uint32));

                    //display preview (could be anything, e.g. when dragging an image we could decide to display
                    //the filename and a small preview of the image, etc.)
                    ImGui::Image((void*)(std::size_t)id, { thumbSize.x, thumbSize.y }, { 0.f, 1.f }, { 1.f, 0.f });
                    ImGui::Text("%s", tex.name.c_str());
                    ImGui::EndDragDropSource();

                    m_selectedTexture = id;
                    ImGui::SetScrollHereY();
                }

                //TODO for some reason id always returns the same as the selected texture, not the current id
                /*std::string label = "##" + std::to_string(id);
                if (ImGui::BeginPopupContextWindow(label.c_str()))
                {
                    label = "Reload##" + std::to_string(id);
                    if (ImGui::MenuItem(label.c_str(), nullptr, nullptr))
                    {
                        auto path = m_preferences.workingDirectory + "/" + m_materialTextures.at(id).relPath + m_materialTextures.at(id).name;
                        if (cro::FileSystem::fileExists(path))
                        {
                            if (!m_materialTextures.at(id).texture->loadFromFile(path))
                            {
                                cro::FileSystem::showMessageBox("Error", "Could not refresh texture. Has the file been removed?");
                            }
                        }
                        LogI << m_selectedTexture << ", " << id << std::endl;
                    }
                    label = "Remove##" + std::to_string(id);
                    if (ImGui::MenuItem(label.c_str(), nullptr, nullptr))
                    {
                        if (cro::FileSystem::showMessageBox("Delete?", "Remove this texture?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                        {
                            removeID = id;
                        }
                    }
                    ImGui::EndPopup();
                }*/

                ImGui::Text("%s", tex.name.c_str());
                if (ImGui::IsItemHovered())
                {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", tex.name.c_str());
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

            if (removeID)
            {
                //remove from any materials using this
                for (auto& mat : m_materialDefs)
                {
                    for (auto& texID : mat.textureIDs)
                    {
                        if (texID == removeID)
                        {
                            texID = 0;
                        }
                    }
                }

                m_materialTextures.erase(removeID);
                m_selectedTexture = m_materialTextures.empty() ? 0 : m_materialTextures.begin()->first;
            }

            lastSelected = m_selectedTexture;

            ImGui::EndChild();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}

void ModelState::updateLayout(std::int32_t w, std::int32_t h)
{
    float width = static_cast<float>(w);
    float height = static_cast<float>(h);

    WindowLayouts[WindowID::Inspector] =
        std::make_pair(glm::vec2(0.f, ui::TitleHeight),
            glm::vec2(width * ui::InspectorWidth, height - (ui::TitleHeight/* + (height * ui::BrowserHeight)*/)));

    WindowLayouts[WindowID::Browser] =
        std::make_pair(glm::vec2(width * ui::InspectorWidth, height - (height * ui::BrowserHeight)),
            glm::vec2(width - (width * ui::InspectorWidth), height * ui::BrowserHeight));

    float ratio = width / 800.f;
    float matSlotWidth = std::max(ui::MinMaterialSlotSize, ui::MinMaterialSlotSize * ratio);
    WindowLayouts[WindowID::MaterialSlot] =
        std::make_pair(glm::vec2(0.f), glm::vec2(matSlotWidth));

    WindowLayouts[WindowID::ViewGizmo] =
        std::make_pair(glm::vec2(width - ui::ViewManipSize, ui::TitleHeight), glm::vec2(ui::ViewManipSize, ui::ViewManipSize));
}