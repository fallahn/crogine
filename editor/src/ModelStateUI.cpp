/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2023
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

#include "ModelState.hpp"
#include "SharedStateData.hpp"
#include "ModelViewerConsts.inl"
#include "UIConsts.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/GBuffer.hpp>

#include <crogine/graphics/MeshBuilder.hpp>

#include <crogine/util/String.hpp>
#include <crogine/util/Matrix.hpp>

#include <sstream>
#include <iomanip>

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
        Info,

        Count
    };
    std::array<std::pair<glm::vec2, glm::vec2>, WindowID::Count> WindowLayouts =
    {
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f)),
        std::make_pair(glm::vec2(0.f), glm::vec2(0.f)),
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

    void drawAnimationData(std::array<cro::Entity, EntityID::Count>& entities, bool skelCheck = true)
    {
        if (skelCheck)
        {
            bool showSkel = !entities[EntityID::ActiveSkeleton].getComponent<cro::Model>().isHidden();
            if (ImGui::Checkbox("Show Skeleton##234897", &showSkel))
            {
                entities[EntityID::ActiveSkeleton].getComponent<cro::Model>().setHidden(!showSkel);
            }
            ImGui::NewLine();
        }
        auto& skel = entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();
        auto& animations = skel.getAnimations();

        ImGui::Text("%lu Animation(s)", animations.size());

        std::vector<const char*> items;
        for (const auto& anim : animations)
        {
            items.push_back(anim.name.c_str());
        }

        static int selectedAnim = 0;
        selectedAnim = std::min(selectedAnim, static_cast<std::int32_t>(animations.size() - 1));
        ImGui::SetNextItemWidth(WindowLayouts[WindowID::Inspector].second.x * 0.92f);
        if (ImGui::ListBox("##43345", &selectedAnim, items.data(), static_cast<std::int32_t>(items.size()), 4))
        {
            //play new anim if playing
            auto rate = animations[skel.getCurrentAnimation()].playbackRate;
            {
                skel.stop();
                skel.play(selectedAnim, rate);
            }
        }
        auto& anim = animations[selectedAnim];
        auto name = anim.name.substr(0, 16);
        ImGui::Text("Frames: %lu", anim.frameCount);
        toolTip(anim.name.c_str());

        ImGui::SameLine();
        ImGui::Text(" - Current Frame: %lu", animations[skel.getCurrentAnimation()].currentFrame - animations[skel.getCurrentAnimation()].startFrame);

        if (ImGui::Button("<")
            && animations[skel.getCurrentAnimation()].playbackRate == 0)
        {
            skel.prevFrame();
        }
        toolTip("Previous Frame");
        ImGui::SameLine();

        if (animations[skel.getCurrentAnimation()].playbackRate != 0)
        {
            //pause button
            if (ImGui::Button("Pause", ImVec2(50.f, 22.f)))
            {
                skel.stop();
            }
        }
        else
        {
            //play button
            if (ImGui::Button("Play", ImVec2(50.f, 22.f)))
            {
                skel.play(selectedAnim);
                entities[EntityID::JointNode].getComponent<cro::Model>().setHidden(true);
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(">")
            && animations[skel.getCurrentAnimation()].playbackRate == 0)
        {
            skel.nextFrame();
        }
        toolTip("Next Frame");

        ImGui::SameLine();
        ImGui::Checkbox("Loop", &anim.looped);
        
        ImGui::NewLine();
        ImGui::Separator();
    }

    bool setInspectorTab = false;
    std::int32_t inspectorTabIndex = 0;

    std::string uniqueID()
    {
        static std::size_t uid = 0;
        std::stringstream ss;
        ss << "." << std::setfill('0') << std::setw(4) << uid++;
        return ss.str();
    }
}

float updateView(cro::Entity entity, float farPlane, float fov)
{
    glm::vec2 size(cro::App::getWindow().getSize());
    auto windowHeight = size.y;
    size.x -= (size.x * uiConst::InspectorWidth);
    size.y -= (size.y * uiConst::BrowserHeight);

    auto& cam3D = entity.getComponent<cro::Camera>();
    cam3D.setPerspective(fov, size.x / size.y, 0.1f, farPlane, 3);
    cam3D.viewport.left = uiConst::InspectorWidth;
    cam3D.viewport.width = 1.f - uiConst::InspectorWidth;
    cam3D.viewport.bottom = uiConst::BrowserHeight + (uiConst::InfoBarHeight / windowHeight);
    cam3D.viewport.height = 1.f - cam3D.viewport.bottom;

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
                cro::FileSystem::showMessageBox("TODO", "Show options here.");
                //export the model
                exportModel(false, false);
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
    for (auto& [s, i] : m_combinedImages)
    {
        s = "None Selected";
        i = cro::Image(true);
    }

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
                    
                    if (ImGui::MenuItem("Flip Normals", nullptr, nullptr, m_entities[EntityID::ActiveModel].isValid()))
                    {
                        flipNormals();
                    }
                    
                    if (ImGui::MenuItem("Export Model", nullptr, nullptr, !m_importedVBO.empty()))
                    {
                        //TODO show modal asking if we want to export animation
                        //and overwrite existing material
                        cro::FileSystem::showMessageBox("TODO", "Show options here");
                        exportModel();
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
                            m_scene.setCubemap(m_environmentMap);
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
                    if (ImGui::MenuItem("Use Free Look", nullptr, nullptr))
                    {
                        toggleFreecam();
                    }  
                    if (!m_useFreecam)
                    {
                        if (ImGui::MenuItem("Reset Camera"))
                        {
                            m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setLocalTransform(glm::mat4(1.f));
                            m_entities[EntityID::ArcBall].getComponent<cro::Transform>().setPosition(DefaultArcballPosition);
                            m_cameras[CameraID::Default].FOV = DefaultFOV;
                            updateView(m_cameras[CameraID::Default].camera, DefaultFarPlane, DefaultFOV);
                        }
                    }
                    ImGui::EndMenu();
                }

                //tools menu
                if (ImGui::BeginMenu("Tools"))
                {
                    if (ImGui::MenuItem("Options", nullptr, nullptr))
                    {
                        m_showPreferences = true;
                    }
                    if (ImGui::MenuItem("AO Baker", nullptr, &m_showBakingWindow, m_entities[EntityID::ActiveModel].isValid() && m_importedVBO.empty()))
                    {

                    }
                    if (ImGui::MenuItem("Mask Editor", nullptr, &m_showMaskEditor))
                    {

                    }
                    ImGui::MenuItem("Text Editor", nullptr, &m_textEditor.visible);
                    ImGui::MenuItem("Image Combiner", nullptr, &m_showImageCombiner);

                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            //options window
            if (m_showPreferences)
            {
                ImGui::SetNextWindowSize({ 400.f, 260.f });
                if (ImGui::Begin("Preferences##model", &m_showPreferences))
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

                    auto skyColour = getContext().appInstance.getClearColour();
                    if (ImGui::ColorEdit3("Sky Colour", skyColour.asArray()))
                    {
                        getContext().appInstance.setClearColour(skyColour);
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
                    ImGui::Image(m_materialDefs[m_selectedMaterial].previewTexture.getTexture(), { uiConst::PreviewTextureSize, uiConst::PreviewTextureSize }, { 0.f, 1.f }, { 1.f, 0.f });
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
                    ImGui::Image(*m_lightmapTextures[i], { uiConst::PreviewTextureSize / 4.f, uiConst::PreviewTextureSize / 4.f }, { 0.f, 1.f }, { 1.f, 0.f });

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
            drawInfo();
            drawGizmo();
            
            if (m_showImageCombiner)
            {
                drawImageCombiner();
            }

            if (m_browseGLTF)
            {
                showGLTFBrowser();
            }

            m_maskEditor.doImGui(&m_showMaskEditor);
            m_textEditor.update();


#ifdef CRO_DEBUG_
            if (m_useDeferred)
            {
                if (ImGui::Begin("GBuffer"))
                {
                    static std::int32_t index = 0;
                    if (ImGui::InputInt("Channel", &index))
                    {
                        index = (index + 6) % 6;
                    }

                    const auto& buffer = m_scene.getActiveCamera().getComponent<cro::GBuffer>().buffer;
                    ImGui::Image(buffer.getTexture(index), { 256.f, 256.f }, { 0.f, 1.f }, { 1.f, 0.f });
                }
                ImGui::End();
            }
#endif
        });

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void ModelState::drawInspector()
{
    //ImGui::ShowDemoWindow();
    auto [pos, size] = WindowLayouts[WindowID::Inspector];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
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
                        drawAnimationData(m_entities);
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
                    helpMarker("Applies this transform directly to the model data, before exporting the model.\nUseful if an imported model uses z-up coordinates, or is much\nlarger or smaller than other models in the scene.\nTIP: if a model doesn't scale enough in either direction try applying the current scale first before rescaling");

                    ImGui::NewLine();
                    static bool modelOnly = false;
                    ImGui::Checkbox("Export Model Only", &modelOnly);
                    ImGui::SameLine();
                    helpMarker("If this is checked then only the model data will exported to the crogine file, leaving any existing material data in tact.");
                    if (m_importedHeader.animated)
                    {
                        ImGui::Checkbox("Export Animations", &m_exportAnimation);
                    }
                    if (ImGui::Button("Convert##01"))
                    {
                        exportModel(modelOnly);
                    }
                    ImGui::SameLine();
                    helpMarker("Export this model to Crogine format, and create a model definition file.\nThe model will then be automatically re-opened for material editing.");
                }
                else
                {
                    ImGui::PushItemWidth(size.x * uiConst::TextBoxWidth);
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
                        drawAnimationData(m_entities);
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

                ImGui::PushItemWidth(size.x * uiConst::TextBoxWidth);
                ImGui::InputText("##name", &matDef.name);
                ImGui::PopItemWidth();

                auto imgSize = size;
                imgSize.x *= uiConst::MaterialPreviewWidth;
                imgSize.y = imgSize.x;

                ImGui::SetCursorPos({ pos.x + ((size.x - imgSize.y) / 2.f), pos.y + 60.f });
                ImGui::Image((void*)(std::size_t)m_materialDefs[m_selectedMaterial].previewTexture.getTexture().getGLHandle(),
                    { imgSize.x, imgSize.y }, { 0.f, 1.f }, { 1.f, 0.f });

                static float rotation = 0.f;
                ImGui::PushItemWidth(size.x * uiConst::TextBoxWidth);
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
                ImGui::PushItemWidth(size.x * uiConst::TextBoxWidth);
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
                    if (type == MaterialDefinition::VertexLit)
                    {
                        if (ImGui::ColorEdit4("Mask Colour", matDef.maskColour.asArray()))
                        {
                            applyMaterial = true;
                        }
                        ImGui::SameLine();
                        helpMarker("Defines the mask colour of the material. Specular intensity is stored in the red channel, specular amount in the green channel, self-illumination is stored in the blue channel and skybox reflection in the alpha channel.");
                    }
                    else
                    {
                        if (ImGui::ColorEdit3("Mask Colour", matDef.maskColour.asArray()))
                        {
                            applyMaterial = true;
                        }
                        ImGui::SameLine();
                        helpMarker("Defines the mask colour of the material in lieu of a mask map. The metallic value is stored in the red channel, roughness in the green channel and AO in the blue channel.");
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

                ImGui::NewLine();
                ImGui::Checkbox("Receive Shadows", &matDef.receiveShadows);
                ImGui::SameLine();
                helpMarker("Check this box if the material should receive shadows from the active shadow map");
                if (matDef.receiveShadows)
                {
                    shaderFlags |= cro::ShaderResource::RxShadows;
                }

                if (ImGui::Checkbox("Enable Depth Test", &matDef.depthTest))
                {
                    applyMaterial = true;
                }
                ImGui::SameLine();
                helpMarker("Enable or disable depth testing for this material");

                if (ImGui::Checkbox("Double Sided", &matDef.doubleSided))
                {
                    applyMaterial = true;
                }
                ImGui::SameLine();
                helpMarker("Enables rendering both sides of a material.\nThis is usually desired for alpha blended materials\nor other instances where the back face would be visible");

                if (ImGui::Checkbox("Use Mipmaps", &matDef.useMipmaps))
                {
                    //do nothing really, this isn't applied until the material is loaded in-game
                }
                ImGui::SameLine();
                helpMarker("Tells crogine to generate mipmaps to the diffuse texture when it is loaded in-game.\nMaterials with alpha-clipped transparency may not look desirable.");

                ImGui::Checkbox("Use Vertex Colours", &matDef.vertexColoured);
                ImGui::SameLine();
                helpMarker("Any colour information stored in the model's vertex data will be multiplied with the diffuse colour of the material");
                if (matDef.vertexColoured)
                {
                    shaderFlags |= cro::ShaderResource::VertexColour;
                }

                if (type != MaterialDefinition::PBR)
                {
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


                    ImGui::NewLine();
                    if (ImGui::Checkbox("Animated", &matDef.animated))
                    {
                        applyMaterial = true;
                    }
                    ImGui::SameLine();
                    helpMarker("If animated the material will attempt to split the UV coords by the number of row/columns (below) and cycle through them based on the current frame rate. Frames are sorted column first, top to bottom. Enabling this overrides any subrect setting.");

                    ImGui::PushItemWidth(100.f);
                    std::int32_t rowCount = matDef.rowCount;
                    if (ImGui::InputInt("Row Count", &rowCount))
                    {
                        rowCount = std::max(1, std::min(100, rowCount));
                        matDef.rowCount = rowCount;

                        applyMaterial = matDef.animated;
                    }
                    ImGui::SameLine();
                    helpMarker("UVs are divided vertically by this value to create frames, does nothing if animation disabled.");

                    std::int32_t colCount = matDef.colCount;
                    if (ImGui::InputInt("Column Count", &colCount))
                    {
                        colCount = std::max(1, std::min(100, colCount));
                        matDef.colCount = colCount;

                        applyMaterial = matDef.animated;
                    }
                    ImGui::SameLine();
                    helpMarker("UVs are divided horizontally by this value to create frames, does nothing if animation disabled.");

                    if (ImGui::InputFloat("Frame Rate", &matDef.frameRate))
                    {
                        matDef.frameRate = std::max(1.f, std::min(60.f, matDef.frameRate));
                        applyMaterial = matDef.animated;
                    }
                    ImGui::PopItemWidth();
                }

                if (matDef.useSubrect || matDef.animated)
                {
                    shaderFlags |= cro::ShaderResource::Subrects;
                }

                ImGui::NewLine();
                ImGui::Text("Blend Mode:");
                ImGui::PushItemWidth(size.x * uiConst::TextBoxWidth);
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


                //material tags list
                ImGui::NewLine();
                ImGui::Text("Tags");
                static std::string tagBuffer;
                ImGui::InputText("##0", &tagBuffer);
                ImGui::SameLine();
                if (ImGui::Button("Add##tag"))
                {
                    if (!tagBuffer.empty())
                    {
                        matDef.tags.push_back(tagBuffer);
                    }
                    tagBuffer.clear();
                }

                static std::vector<const char*> items;
                items.clear();

                if (!matDef.tags.empty())
                {
                    for (auto i = 0u; i < matDef.tags.size(); ++i)
                    {
                        items.push_back(matDef.tags[i].c_str());
                    }

                    static std::int32_t idx = 0;
                    if (ImGui::BeginListBox("Tags##1", ImVec2(-FLT_MIN, 6.f * ImGui::GetTextLineHeightWithSpacing())))
                    {
                        for (auto n = 0u; n < items.size(); ++n)
                        {
                            const bool selected = (idx == n);
                            if (ImGui::Selectable(items[n], selected))
                            {
                                idx = n;
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndListBox();
                    }
                    if (ImGui::Button("Remove##item"))
                    {
                        matDef.tags.erase(std::remove_if(matDef.tags.begin(), matDef.tags.end(), 
                            [&](const std::string& s)
                            {
                                return s.c_str() == items[idx];
                            }), matDef.tags.end());
                    }
                }



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
                    auto shaderType = m_useDeferred ? cro::ShaderResource::UnlitDeferred : cro::ShaderResource::Unlit;
                    if (m_modelProperties.type == ModelProperties::Billboard)
                    {
                        shaderType = cro::ShaderResource::BillboardUnlit;
                    }

                    if (matDef.type == MaterialDefinition::VertexLit)
                    {
                        shaderType = m_useDeferred ? cro::ShaderResource::VertexLitDeferred : cro::ShaderResource::VertexLit;

                        if (m_modelProperties.type == ModelProperties::Billboard)
                        {
                            shaderType = cro::ShaderResource::BillboardVertexLit;
                        }
                    }
                    else if (matDef.type == MaterialDefinition::PBR)
                    {
                        shaderType = m_useDeferred ? cro::ShaderResource::PBRDeferred : cro::ShaderResource::PBR;
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
                imgSize.x *= uiConst::TexturePreviewWidth;
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
    //ImGui::ShowDemoWindow();
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Browser", nullptr, ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::BeginTabBar("##b");

        if (ImGui::BeginTabItem("Materials"))
        {
            if (ImGui::Button("Add##01"))
            {
                if (m_materialDefs.size() < uiConst::MaxMaterials)
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
            thumbSize.y *= uiConst::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            auto frameSize = thumbSize;
            frameSize.x += ImGui::GetStyle().FramePadding.x * uiConst::FramePadding.x;
            frameSize.y += ImGui::GetStyle().FramePadding.y * uiConst::FramePadding.y;

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
                    ImGui::SetDragDropPayload("MATERIAL_SRC", &count, sizeof(std::int32_t));

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
                    && m_materialTextures.size() < uiConst::MaxMaterials)
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
            thumbSize.y *= uiConst::ThumbnailHeight;
            thumbSize.x = thumbSize.y;

            auto frameSize = thumbSize;
            frameSize.x += ImGui::GetStyle().FramePadding.x * uiConst::FramePadding.x;
            frameSize.y += ImGui::GetStyle().FramePadding.y * uiConst::FramePadding.y;

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
                    ImGui::SetDragDropPayload("TEXTURE_SRC", &id, sizeof(std::uint32_t));

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

        if (m_entities[EntityID::ActiveModel].isValid()
            && m_importedVBO.empty()
            && m_entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
        {
            if (ImGui::BeginTabItem("Attachments"))
            {
                auto& skel = m_entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();
                auto& attachments = skel.getAttachments();
                if (attachments.empty())
                {
                    m_attachmentIndex = 0;
                }

                const auto refreshAttachmentView = [&]()
                {
                    if (attachments[m_attachmentIndex].getModel().isValid())
                    {
                        attachments[m_attachmentIndex].getModel().getComponent<cro::Model>().setHidden(true);
                    }
                    attachments[m_attachmentIndex].setModel(cro::Entity());
                    m_attachmentIndex = static_cast<std::int32_t>(attachments.size()) - 1;

                    attachments[m_attachmentIndex].setModel(m_attachmentModels[0]);
                    m_attachmentModels[0].getComponent<cro::Model>().setHidden(false);
                };

                ImGui::BeginChild("##attachments", {260.f, 0.f}, true);
                if (ImGui::Button("Add##attachment"))
                {
                    attachments.emplace_back().setName("attachment" + uniqueID());
                    m_attachmentAngles.emplace_back(0.f, 0.f, 0.f);
                    
                    refreshAttachmentView();
                }
                if (!attachments.empty())
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Duplicate##attachment"))
                    {
                        auto a = attachments[m_attachmentIndex];
                        a.setName(a.getName() + uniqueID());
                        attachments.push_back(a);
                        m_attachmentAngles.emplace_back(glm::eulerAngles(a.getRotation()) * cro::Util::Const::radToDeg);

                        refreshAttachmentView();
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Remove##attachment"))
                    {
                        auto removeIndex = m_attachmentIndex;
                        if (m_attachmentIndex > 0)
                        {
                            if (attachments[m_attachmentIndex].getModel().isValid())
                            {
                                attachments[m_attachmentIndex].getModel().getComponent<cro::Model>().setHidden(true);
                            }
                            m_attachmentIndex--;
                        }                        
                        
                        attachments.erase(attachments.begin() + removeIndex);
                        m_attachmentAngles.erase(m_attachmentAngles.begin() + removeIndex);

                        if (!attachments.empty())
                        {
                            //TODO set to selected preview
                            attachments[m_attachmentIndex].setModel(m_attachmentModels[0]);
                            m_attachmentModels[0].getComponent<cro::Model>().setHidden(false);
                        }
                        else
                        {
                            m_attachmentModels[0].getComponent<cro::Model>().setHidden(true);
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Clear##attachments")
                        && cro::FileSystem::showMessageBox("Warning", "Are You Sure?", cro::FileSystem::YesNo, cro::FileSystem::Warning))
                    {
                        attachments.clear();
                        m_attachmentAngles.clear();
                        m_attachmentIndex = 0;
                        m_attachmentModels[0].getComponent<cro::Model>().setHidden(true);
                    }
                }
                //I'll be buggered if I can figure this out
                /*ImGui::ListBoxHeader("##attachments_list");
                for (auto i = 0u; i < attachments.size(); ++i)
                {
                    bool isSelected = (m_attachmentIndex == i);
                    if (ImGui::Selectable(attachments[i].getName().c_str(), isSelected))
                    {
                        m_attachmentIndex = i;
                    }
                }
                ImGui::ListBoxFooter();*/

                std::vector<const char*> names;
                for (const auto& a : attachments)
                {
                    names.push_back(a.getName().c_str());
                }

                auto lastIndex = m_attachmentIndex;
                ImGui::PushItemWidth(200.f);
                if (ImGui::ListBox("##attachment_list", &m_attachmentIndex, names.data(), static_cast<std::int32_t>(names.size()), 6))
                {
                    if (!m_attachmentModels.empty())
                    {
                        auto oldModel = attachments[lastIndex].getModel();
                        //if (oldModel.isValid())
                        if (oldModel == m_attachmentModels[0])
                        {
                            oldModel.getComponent<cro::Model>().setHidden(true);
                            attachments[lastIndex].setModel(cro::Entity());
                        }

                        if (!attachments[m_attachmentIndex].getModel().isValid())
                        {
                            //show default model
                            attachments[m_attachmentIndex].setModel(m_attachmentModels[0]);
                            m_attachmentModels[0].getComponent<cro::Model>().setHidden(false);
                        }
                    }
                }
                ImGui::PopItemWidth();

                if (ImGui::Button("Export##attachments"))
                {
                    auto path = cro::FileSystem::saveFileDialogue("", "atc");
                    if (!path.empty())
                    {
                        cro::ConfigFile cfg("attachments");
                        for (auto i = 0u; i < attachments.size(); ++i)
                        {
                            const auto& attachment = attachments[i];
                            auto* a = cfg.addObject("attachment", attachment.getName());
                            a->addProperty("position").setValue(attachment.getPosition());
                            a->addProperty("rotation").setValue(m_attachmentAngles[i]);
                            a->addProperty("scale").setValue(attachment.getScale());
                            a->addProperty("parent").setValue(attachment.getParent());
                        }
                        if (!cfg.save(path))
                        {
                            cro::FileSystem::showMessageBox("Error", "Failed to export attachements\nSee console for details");
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Import##attachments")
                    && cro::FileSystem::showMessageBox("Warning", "This will replace all existing attachment data", cro::FileSystem::OKCancel, cro::FileSystem::Warning))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "atc");
                    if (!path.empty())
                    {
                        cro::ConfigFile cfg;
                        if (!cfg.loadFromFile(path))
                        {
                            cro::FileSystem::showMessageBox("Error", "Failed to open attachment file\nSee console for details");
                        }
                        else
                        {
                            attachments.clear();
                            m_attachmentAngles.clear();
                            m_attachmentIndex = 0;

                            const auto& objs = cfg.getObjects();
                            for (const auto& obj : objs)
                            {
                                const auto& name = obj.getName();
                                if (name == "attachment")
                                {
                                    auto aName = obj.getId();
                                    if (!aName.empty())
                                    {
                                        if (aName.length() >= cro::Attachment::MaxNameLength)
                                        {
                                            aName = aName.substr(0, cro::Attachment::MaxNameLength - 1);
                                        }

                                        auto& attachment = attachments.emplace_back();
                                        auto& rotation = m_attachmentAngles.emplace_back();

                                        attachment.setName(aName);

                                        const auto& props = obj.getProperties();
                                        for (const auto& prop : props)
                                        {
                                            auto pName = prop.getName();
                                            if (pName == "position")
                                            {
                                                attachment.setPosition(prop.getValue<glm::vec3>());
                                            }
                                            else if (pName == "rotation")
                                            {
                                                rotation = prop.getValue<glm::vec3>();
                                                glm::quat r = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rotation.z * cro::Util::Const::degToRad, cro::Transform::Z_AXIS);
                                                r = glm::rotate(r, rotation.y * cro::Util::Const::degToRad, cro::Transform::Y_AXIS);
                                                r = glm::rotate(r, rotation.x * cro::Util::Const::degToRad, cro::Transform::X_AXIS);
                                                attachment.setRotation(r);                                                
                                            }
                                            else if (pName == "scale")
                                            {
                                                attachment.setScale(prop.getValue<glm::vec3>());
                                            }
                                            else if (pName == "parent")
                                            {
                                                attachment.setParent(prop.getValue<std::int32_t>());
                                            }
                                        }
                                    }
                                }
                            }

                            //TODO we want to make sure to remove duplicate names
                            //at some point...
                        }
                    }
                }


                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("##attachment_options", {240.f, 0.f}, true);
                if (attachments.empty())
                {
                    ImGui::Text("No attachments added");
                }
                else
                {
                    ImGui::Text("Attachment Properties");

                    auto& ap = attachments[m_attachmentIndex];
                    auto name = ap.getName();
                    if (ImGui::InputText("Name##attachment", &name, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        if (!name.empty())
                        {
                            if (name.length() >= cro::Attachment::MaxNameLength)
                            {
                                name = name.substr(0, cro::Attachment::MaxNameLength - 1);
                            }

                            //this works as long as we don't have too many attachments...
                            auto result = std::find_if(attachments.begin(), attachments.end(), 
                                [&name](const cro::Attachment& a) 
                                {
                                    return a.getName() == name;
                                });

                            if (result != attachments.end())
                            {
                                name = name.substr(0, std::min(name.length(), cro::Attachment::MaxNameLength - 6)) += uniqueID();
                            }
                            ap.setName(name);
                        }
                    }
                    
                    if (ImGui::Button("R##pos"))
                    {
                        ap.setPosition(glm::vec3(0.f));
                    }
                    toolTip("Reset Transform");
                    ImGui::SameLine();                    
                    auto pos = ap.getPosition();
                    if (ImGui::DragFloat3("Position##attachment", &pos[0], 1.f, 0.f, 0.f, "%3.2f"))
                    {
                        ap.setPosition(pos);
                    }

                    if (ImGui::Button("R##rot"))
                    {
                        ap.setRotation(glm::quat(1.f, 0.f, 0.f, 0.f));
                        m_attachmentAngles[m_attachmentIndex] = glm::vec3(0.f);
                    }
                    toolTip("Reset Rotation");
                    ImGui::SameLine();
                    glm::vec3& rot = m_attachmentAngles[m_attachmentIndex];
                    if (ImGui::DragFloat3("Rotation##attachment", &rot[0], 1.f, -180.f, 180.f, "%3.2f"))
                    {
                        ap.setRotation(cro::Util::Vector::eulerToQuat(rot * cro::Util::Const::degToRad));
                    }

                    if (ImGui::Button("R##scale"))
                    {
                        ap.setScale(glm::vec3(1.f));
                    }
                    toolTip("Reset Scale");
                    ImGui::SameLine();
                    auto scale = ap.getScale();
                    if (ImGui::DragFloat3("Scale##attachment", &scale[0], 1.f, 0.f, 0.f, "%3.2f"))
                    {
                        ap.setScale(scale);
                    }


                    auto parent = ap.getParent();
                    if (ImGui::InputInt("Parent##attachment", &parent))
                    {
                        parent = std::max(0, std::min(static_cast<std::int32_t>(skel.getFrameSize()) - 1, parent));
                        ap.setParent(parent);
                    }
                }
                ImGui::EndChild();

                ImGui::SameLine();

                ImGui::BeginChild("##attachment_details", { 240.f, 0.f }, true);
                ImGui::Text("Attachment models (preview)");
                if (ImGui::Button("Quick Scale"))
                {
                    glm::vec3 aPos, aScale;
                    glm::quat aRot;
                    cro::Util::Matrix::decompose(skel.getAttachmentTransform(m_attachmentIndex), aPos, aRot, aScale);
                    attachments[m_attachmentIndex].setScale(glm::vec3(1.f) / aScale);
                }
                ImGui::SameLine();
                helpMarker("Resizes the attachment to a world scale of 1.0, to compensate any scale applied by the skeleton");

                std::vector<const char*> labels;
                for (auto e : m_attachmentModels)
                {
                    labels.push_back(e.getLabel().c_str());
                }
                static std::int32_t selectedModel = 0;
                if (ImGui::ListBox("##preview_model", &selectedModel, labels.data(), static_cast<std::int32_t>(labels.size()), 4))
                {
                    if (!attachments.empty())
                    {
                        if (attachments[m_attachmentIndex].getModel().isValid())
                        {
                            attachments[m_attachmentIndex].getModel().getComponent<cro::Model>().setHidden(true);
                        }
                        attachments[m_attachmentIndex].setModel(m_attachmentModels[selectedModel]);
                        attachments[m_attachmentIndex].getModel().getComponent<cro::Model>().setHidden(false);
                    }
                }
                if (ImGui::Button("Add##preview_model"))
                {
                    auto path = cro::FileSystem::openFileDialogue("", "cmt");
                    if (!path.empty())
                    {
                        std::replace(path.begin(), path.end(), '\\', '/');
                        /*if (auto found = path.find(m_sharedData.workingDirectory); found != std::string::npos)
                        {
                            path = path.substr(found);
                        }*/

                        cro::ModelDefinition md(m_resources, &m_environmentMap, m_sharedData.workingDirectory);
                        if (md.loadFromFile(path))
                        {
                            auto entity = m_scene.createEntity();
                            entity.addComponent<cro::Transform>();
                            md.createModel(entity);
                            entity.setLabel(cro::FileSystem::getFileName(path));
                            entity.getComponent<cro::Model>().setHidden(true);
                            m_attachmentModels.push_back(entity);
                        }
                    }
                }

                if (selectedModel != 0)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Remove##preview_model"))
                    {
                        m_scene.destroyEntity(m_attachmentModels[selectedModel]);
                        m_attachmentModels.erase(m_attachmentModels.begin() + selectedModel);

                        if (selectedModel > 0)
                        {
                            selectedModel--;
                            if (!attachments.empty())
                            {
                                attachments[m_attachmentIndex].setModel(m_attachmentModels[selectedModel]);
                                attachments[m_attachmentIndex].getModel().getComponent<cro::Model>().setHidden(false);
                            }
                        }
                    }
                }

                if (m_attachmentModels.size() > 1)
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Clear##preview_models"))
                    {
                        for (auto i = 1u; i < m_attachmentModels.size(); ++i)
                        {
                            m_scene.destroyEntity(m_attachmentModels[i]);
                        }
                        m_attachmentModels.resize(1);

                        selectedModel = 0;
                        if (!attachments.empty())
                        {
                            attachments[m_attachmentIndex].setModel(m_attachmentModels[selectedModel]);
                            attachments[m_attachmentIndex].getModel().getComponent<cro::Model>().setHidden(false);
                        }
                    }
                }

                ImGui::EndChild();

                ImGui::EndTabItem();
            }
            else
            {
                m_attachmentModels[0].getComponent<cro::Model>().setHidden(true);
            }

            if (ImGui::BeginTabItem("Animation"))
            {
                auto& skeleton = m_entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();
                const auto& animations = skeleton.getAnimations();

                ImGui::BeginChild("##anim_controls", { 300.f, -44.f }, true);
                drawAnimationData(m_entities, false);


                if (animations[skeleton.getCurrentAnimation()].playbackRate == 0)
                {
                    if (ImGui::Button("Export##notif"))
                    {
                        auto path = cro::FileSystem::saveFileDialogue("", "ntf");
                        if (!path.empty())
                        {
                            cro::ConfigFile cfg("animation_data");
                            const auto& notifications = skeleton.getNotifications();
                            for (auto i = 0u; i < notifications.size(); ++i)
                            {
                                const auto& nFrame = notifications[i];
                                auto* nObj = cfg.addObject("frame");
                                for (const auto& notif : nFrame)
                                {
                                    auto* nfObj = nObj->addObject("event", notif.name);
                                    nfObj->addProperty("joint").setValue(static_cast<std::int32_t>(notif.jointID));
                                    nfObj->addProperty("user_data").setValue(notif.userID);
                                }
                            }

                            if (!cfg.save(path))
                            {
                                cro::FileSystem::showMessageBox("Error", "Failed to export animation data.\nSee console for information");
                            }
                        }
                    }
                    toolTip("Export all frame notifications to a file");
                    ImGui::SameLine();
                    if (ImGui::Button("Import##notif")
                        && cro::FileSystem::showMessageBox("Warning", "This will overwrite all existing animation data", cro::FileSystem::OKCancel, cro::FileSystem::Warning))
                    {
                        auto path = cro::FileSystem::openFileDialogue("", "ntf");
                        if (!path.empty())
                        {
                            cro::ConfigFile cfg;
                            if (!cfg.loadFromFile(path))
                            {
                                cro::FileSystem::showMessageBox("Error", "Failed to open animation data.\nSeeconsole for details", cro::FileSystem::OK, cro::FileSystem::Error);
                            }
                            else
                            {
                                auto& notifications = skeleton.getNotifications();
                                auto size = notifications.size();
                                notifications.clear();

                                const auto objs = cfg.getObjects();
                                auto i = 0u;

                                for (const auto& obj : objs)
                                {
                                    //don't load too many frames
                                    if (i == size)
                                    {
                                        break;
                                    }

                                    if (obj.getName() == "frame")
                                    {
                                        auto& frame = notifications.emplace_back();

                                        const auto& fObjs = obj.getObjects();
                                        for (const auto& fObj : fObjs)
                                        {
                                            if (fObj.getName() == "event")
                                            {
                                                auto name = fObj.getId();
                                                if (!name.empty())
                                                {
                                                    if (name.length() >= cro::Attachment::MaxNameLength)
                                                    {
                                                        name = name.substr(0, cro::Attachment::MaxNameLength - 1);
                                                    }
                                                    auto& n = frame.emplace_back();
                                                    n.name = name;
                                                    
                                                    for (const auto& prop : fObj.getProperties())
                                                    {
                                                        if (prop.getName() == "joint")
                                                        {
                                                            n.jointID = prop.getValue<std::int32_t>();
                                                        }
                                                        else if (prop.getName() == "user_data")
                                                        {
                                                            n.userID = prop.getValue<std::int32_t>();
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        i++;
                                    }
                                }

                                //pad out to correct size if too small
                                while (notifications.size() < size)
                                {
                                    notifications.emplace_back();
                                }
                            }
                        }
                    }
                }
                ImGui::EndChild();



                if (animations[skeleton.getCurrentAnimation()].playbackRate == 0)
                {
                    ImGui::SameLine();
                    ImGui::BeginChild("##anim_notifications", { 200.f, -44.f }, true);
                    auto& notifications = skeleton.getNotifications()[skeleton.getCurrentFrame()];
                    static std::int32_t notIndex = 0;

                    if (ImGui::Button("Add##notification"))
                    {
                        auto& n = notifications.emplace_back();
                        n.name = "Notif" + uniqueID();
                        notIndex = static_cast<std::int32_t>(notifications.size()) - 1;
                    }
                    if (!notifications.empty())
                    {
                        ImGui::SameLine();
                        if (ImGui::Button("Remove##notification"))
                        {
                            notifications.erase(notifications.begin() + notIndex);
                            if (notIndex > 0)
                            {
                                notIndex--;
                            }
                        }

                        if (!notifications.empty())
                        {
                            m_entities[EntityID::JointNode].getComponent<cro::Model>().setHidden(false);
                            auto pos = glm::vec3(
                                skeleton.getRootTransform() *
                                skeleton.getFrames()[(skeleton.getCurrentFrame() * skeleton.getFrameSize()) + notifications[notIndex].jointID].worldMatrix[3]);
                            m_entities[EntityID::JointNode].getComponent<cro::Transform>().setPosition(pos);
                        }
                    }
                    else
                    {
                        m_entities[EntityID::JointNode].getComponent<cro::Model>().setHidden(true);
                    }
                    std::vector<const char*> names;
                    for (const auto& n : notifications)
                    {
                        names.push_back(n.name.c_str());
                    }

                    ImGui::SetNextItemWidth(200.f);
                    if (ImGui::ListBox("##notifs", &notIndex, names.data(), static_cast<std::int32_t>(names.size()), 6))
                    {

                    }
                    ImGui::EndChild();
                    ImGui::SameLine();
                    ImGui::BeginChild("##anim_notif_details", { 400.f, -44.f }, true);

                    if (!notifications.empty())
                    {
                        auto name = notifications[notIndex].name;
                        if (ImGui::InputText("Name##notif", &name, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
                        {
                            if (!name.empty())
                            {
                                //we need to clamp this to the character limit
                                if (name.length() >= cro::Attachment::MaxNameLength)
                                {
                                    name = name.substr(0, cro::Attachment::MaxNameLength - 1);
                                }
                                //and make duplicates unique
                                auto result = std::find_if(notifications.begin(), notifications.end(),
                                    [&name](const cro::Skeleton::Notification& a)
                                    {
                                        return a.name == name;
                                    });
                                
                                if (result != notifications.end())
                                {
                                    name = name.substr(0, std::min(name.length(), cro::Attachment::MaxNameLength - 6)) += uniqueID();
                                }
                                notifications[notIndex].name = name;
                            }
                        }

                        auto jointID = static_cast<std::int32_t>(notifications[notIndex].jointID);
                        if (ImGui::InputInt("JointID##notif", &jointID))
                        {
                            jointID = std::max(0, std::min(jointID, static_cast<std::int32_t>(skeleton.getFrameSize()) - 1));
                            notifications[notIndex].jointID = jointID;
                        }

                        ImGui::InputInt("User Data##notif", &notifications[notIndex].userID);
                    }
                    else
                    {
                        ImGui::Text("No Events This Frame");
                    }

                    ImGui::EndChild();

                    //creates a button per frame and highlights those with events
                    const auto& anim = animations[skeleton.getCurrentAnimation()];
                    ImGui::BeginChild("##frame_buttons", { 0.f, 0.f }, true, ImGuiWindowFlags_::ImGuiWindowFlags_HorizontalScrollbar);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.0f, 1.0f));
                    for (auto i = anim.startFrame; i < anim.startFrame + anim.frameCount; ++i)
                    {
                        auto notifCount = skeleton.getNotifications()[i].size();
                        if (notifCount)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, { 1.f, 0.f, 0.f, 1.f });
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, { 1.f, 1.f, 0.f, 1.f });
                        }

                        std::string label = /*std::to_string(i - anim.startFrame) + */"##" + std::to_string(i);
                        if (ImGui::Button(label.c_str(), { 10.f, 14.f }))
                        {
                            skeleton.gotoFrame(i - anim.startFrame);
                        }
                        ImGui::PopStyleColor();
                        std::string tip = "Frame " + std::to_string(i - anim.startFrame) + ": " + std::to_string(notifCount) + " notification(s)";
                        toolTip(tip.c_str());
                        ImGui::SameLine();
                    }
                    ImGui::PopStyleVar();
                    ImGui::EndChild();
                }
                /*else
                {
                    m_entities[EntityID::JointNode].getComponent<cro::Model>().setHidden(true);
                }*/

                ImGui::EndTabItem();
            }
            else
            {
                m_entities[EntityID::JointNode].getComponent<cro::Model>().setHidden(true);
            }
        }

        ImGui::EndTabBar();
    }
    ImGui::End();
}

void ModelState::drawInfo()
{
    auto [pos, size] = WindowLayouts[WindowID::Info];
    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("InfoBar", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar))
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

void ModelState::drawGizmo()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(io.DisplaySize.x * uiConst::InspectorWidth, 0, io.DisplaySize.x - (uiConst::InspectorWidth * io.DisplaySize.x),
        io.DisplaySize.y - (uiConst::BrowserHeight * io.DisplaySize.y));


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
    /*if (m_entities[EntityID::ActiveModel].isValid()
        && m_importedVBO.empty())
    {
        const auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
        tx = m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().getLocalTransform();
        ImGuizmo::Manipulate(&cam.getActivePass().viewMatrix[0][0], &cam.getProjectionMatrix()[0][0], ImGuizmo::OPERATION::ROTATE, ImGuizmo::MODE::LOCAL, &tx[0][0]);
        m_entities[EntityID::ActiveModel].getComponent<cro::Transform>().setLocalTransform(tx);
    }*/
}

void ModelState::drawImageCombiner()
{
    ImGui::SetNextWindowSize({ 380.f, 160.f });
    if (ImGui::Begin("Image Combiner", &m_showImageCombiner, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize))
    {
        static const std::array<std::string, 4u> labels =
        {
            "Red Channel: ", "Green Channel: ", "Blue Channel: ", "Alpha Channel: "
        };

        std::int32_t i = 0;
        for (const auto& l : labels)
        {
            ImGui::Text(l.c_str());
            ImGui::SameLine();
            ImGui::Text(m_combinedImages[i].first.c_str());
            ImGui::SameLine();

            std::string button = "Open File##" + std::to_string(i);
            if (ImGui::Button(button.c_str()))
            {
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
                if (!path.empty())
                {
                    if (m_combinedImages[i].second.loadFromFile(path))
                    {
                        m_combinedImages[i].first = cro::FileSystem::getFileName(path);
                    }
                }
            }
            ImGui::SameLine();
            button = "Clear##" + std::to_string(i);
            if (ImGui::Button(button.c_str()))
            {
                m_combinedImages[i] = std::make_pair("None Selected", cro::Image(true));
            }
            i++;
        }

        if (ImGui::Button("Save##0034"))
        {
            std::uint32_t minWidth = 0;
            glm::uvec2 lastSize(0);
            bool imageOK = false;
            for (const auto& [s,i] : m_combinedImages)
            {
                auto size = i.getSize();
                if (size.x > minWidth)
                {
                    minWidth = size.x;
                }

                if (size.x != 0)
                {
                    if (size == lastSize)
                    {
                        imageOK = true;
                    }
                    else if (lastSize.x != 0)
                    {
                        cro::Console::print("Not all images are same dimension");
                        cro::FileSystem::showMessageBox("Error", s + ": Not all images have the same dimensions");
                        imageOK = false;
                        break;
                    }
                    lastSize = size;
                }

            }

            if (imageOK)
            {
                auto path = cro::FileSystem::saveFileDialogue("", "png");

                if(!path.empty())
                {
                    if (cro::FileSystem::getFileExtension(path) != ".png")
                    {
                        path += ".png";
                    }

                    //write image file
                    cro::Image output;
                    output.create(lastSize.x, lastSize.y, cro::Colour::Black);

                    for (auto y = 0u; y < lastSize.y; ++y)
                    {
                        for (auto x = 0u; x < lastSize.x; ++x)
                        {
                            auto red = m_combinedImages[0].second.getPixel(x, y);
                            auto green = m_combinedImages[1].second.getPixel(x, y);
                            auto blue = m_combinedImages[2].second.getPixel(x, y);
                            auto alpha = m_combinedImages[3].second.getPixel(x, y);

                            cro::Colour outPixel;
                            if (red)
                            {
                                outPixel.setRed(red[0]);
                            }
                            if (green)
                            {
                                outPixel.setGreen(green[1]);
                            }
                            if (blue)
                            {
                                outPixel.setBlue(blue[2]);
                            }
                            if (alpha && m_combinedImages[3].second.getFormat() == cro::ImageFormat::RGBA)
                            {
                                outPixel.setAlpha(alpha[3]);
                            }

                            output.setPixel(x, y, outPixel);
                        }
                    }
                    if (output.write(path))
                    {
                        cro::FileSystem::showMessageBox("Success", "Wrote image successfully");
                    }
                    else
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed writing " + cro::FileSystem::getFileName(path));
                    }
                }
            }
        }

        if (!m_showImageCombiner)
        {
            //window was closed
            for (auto& [str, img] : m_combinedImages)
            {
                str = "None Selected";
                img = cro::Image(true);
            }
        }
    }
    ImGui::End();
}

void ModelState::updateLayout(std::int32_t w, std::int32_t h)
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

    float ratio = width / 800.f;
    float matSlotWidth = std::max(uiConst::MinMaterialSlotSize, uiConst::MinMaterialSlotSize * ratio);
    WindowLayouts[WindowID::MaterialSlot] =
        std::make_pair(glm::vec2(0.f), glm::vec2(matSlotWidth));

    WindowLayouts[WindowID::ViewGizmo] =
        std::make_pair(glm::vec2(width - uiConst::ViewManipSize, uiConst::TitleHeight), glm::vec2(uiConst::ViewManipSize, uiConst::ViewManipSize));
}