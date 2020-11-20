/*-----------------------------------------------------------------------

Matt Marchant 2020
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
#include "OriginIconBuilder.hpp"
#include "ResourceIDs.hpp"
#include "NormalVisMeshBuilder.hpp"
#include "GLCheck.hpp"
#include "UIConsts.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/gui/imgui.h>
#include <crogine/gui/imgui_stdlib.h>

#include <crogine/graphics/StaticMeshBuilder.hpp>
#include <crogine/graphics/DynamicMeshBuilder.hpp>
#include <crogine/graphics/SphereBuilder.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>
#include <crogine/util/String.hpp>

#include <crogine/detail/glm/gtx/euler_angles.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <string_view>

namespace ai = Assimp;

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

    const float DefaultFOV = 35.f * cro::Util::Const::degToRad;
    const float MaxFOV = 120.f * cro::Util::Const::degToRad;
    const float MinFOV = 5.f * cro::Util::Const::degToRad;
    const float DefaultFarPlane = 50.f;

    void updateView(cro::Entity entity, float farPlane, float fov)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        size.x -= (size.x * ui::InspectorWidth);
        size.y -= (size.y * ui::BrowserHeight);

        auto& cam3D = entity.getComponent<cro::Camera>();
        cam3D.projectionMatrix = glm::perspective(fov, size.x / size.y, 0.1f, farPlane);
        cam3D.viewport.left = ui::InspectorWidth;
        cam3D.viewport.width = 1.f - ui::InspectorWidth;
        cam3D.viewport.bottom = ui::BrowserHeight;
        cam3D.viewport.height = 1.f - ui::BrowserHeight;
    }


    const std::string prefPath = cro::FileSystem::getConfigDirectory("cro_model_viewer") + "prefs.cfg";

    const std::array<float, 6> worldScales = { 0.01f, 0.1f, 1.f, 10.f, 100.f, 1000.f };
    const glm::vec3 DefaultCameraPosition({ 0.f, 0.25f, 5.f });

    std::array<std::int32_t, MaterialID::Count> materialIDs = {};
    std::array<cro::Entity, EntityID::Count> entities = {};

    enum WindowID
    {
        Inspector,
        Browser,
        MaterialSlot,

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
}

ModelState::ModelState(cro::StateStack& stack, cro::State::Context context)
	: cro::State            (stack, context),
    m_scene                 (context.appInstance.getMessageBus()),
    m_previewScene          (context.appInstance.getMessageBus()),
    m_fov                   (DefaultFOV),
    m_showPreferences       (false),
    m_showGroundPlane       (false),
    m_showSkybox            (false),
    m_showAABB              (false),
    m_showSphere            (false),
    m_selectedTexture       (std::numeric_limits<std::uint32_t>::max()),
    m_selectedMaterial      (std::numeric_limits<std::uint32_t>::max())
{
    //launches a loading screen (registered in MyApp.cpp)
    context.mainWindow.loadResources([this]() {
        //add systems to scene
        addSystems();
        //load assets (textures, shaders, models etc)
        loadAssets();
        //create some entities
        createScene();
        //windowing
        buildUI();
    });

    context.appInstance.resetFrameTime();
    context.mainWindow.setTitle("Crogine Model Importer");
}

//public
bool ModelState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEMOTION:
        updateMouseInput(evt);
        break;
    case SDL_MOUSEWHEEL:
    {
        m_fov = std::min(MaxFOV, std::max(MinFOV, m_fov - (evt.wheel.y * 0.1f)));
        updateView(m_scene.getActiveCamera(), DefaultFarPlane, m_fov);
    }
        break;
    }

    m_previewScene.forwardEvent(evt);
    m_scene.forwardEvent(evt);
	return false;
}

void ModelState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateLayout(data.data0, data.data1);
            updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], m_fov);
        }
    }

    m_previewScene.forwardMessage(msg);
    m_scene.forwardMessage(msg);
}

bool ModelState::simulate(float dt)
{
    m_previewScene.simulate(dt);
    m_scene.simulate(dt);
	return false;
}

void ModelState::render()
{
    //render active material preview
    if (!m_materialDefs.empty())
    {
        m_materialDefs[m_selectedMaterial].previewTexture.clear(ui::PreviewClearColour);
        m_previewScene.render(m_materialDefs[m_selectedMaterial].previewTexture);
        m_materialDefs[m_selectedMaterial].previewTexture.display();
    }

    m_scene.render(cro::App::getWindow());
}

//private
void ModelState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);

    m_previewScene.addSystem<cro::CameraSystem>(mb);
    m_previewScene.addSystem<cro::ModelRenderer>(mb);
}

void ModelState::loadAssets()
{
    //create a default material to display models on import
    auto flags = cro::ShaderResource::DiffuseColour;
    auto shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::VertexLit, flags);
    materialIDs[MaterialID::Default] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(materialIDs[MaterialID::Default]).setProperty("u_colour", cro::Colour(1.f, 0.f, 1.f));

    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::ShadowMap, cro::ShaderResource::Skinning | cro::ShaderResource::DepthMap);
    materialIDs[MaterialID::DefaultShadow] = m_resources.materials.add(m_resources.shaders.get(shaderID));

    //used for drawing debug lines
    shaderID = m_resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::VertexColour);
    materialIDs[MaterialID::DebugDraw] = m_resources.materials.add(m_resources.shaders.get(shaderID));
    m_resources.materials.get(materialIDs[MaterialID::DebugDraw]).blendMode = cro::Material::BlendMode::Alpha;

    //black texture for empty texture slots
    cro::Image img;
    img.create(2, 2, cro::Colour::Black());
    LOG("Add creating textures from images", cro::Logger::Type::Info);
    m_blackTexture.create(2, 2);
    m_blackTexture.update(img.getPixelData());

    img.create(2, 2, cro::Colour::Magenta());
    m_magentaTexture.create(2, 2);
    m_magentaTexture.update(img.getPixelData());
}

void ModelState::createScene()
{
    //create ground plane
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });

    auto meshID = m_resources.meshes.loadMesh(cro::DynamicMeshBuilder(cro::VertexProperty::Position | cro::VertexProperty::Colour, 1, GL_LINE_STRIP));
    auto material = m_resources.materials.get(materialIDs[MaterialID::DebugDraw]);
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    auto& meshData = entity.getComponent<cro::Model>().getMeshData();
    updateGridMesh(meshData, std::nullopt, std::nullopt);
    entities[EntityID::GroundPlane] = entity;

    //create the camera - using a custom camera prevents the scene updating the projection on window resize
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(DefaultCameraPosition);
    entity.addComponent<cro::Camera>();
    updateView(entity, DefaultFarPlane, DefaultFOV);
    m_scene.setActiveCamera(entity);

    entities[EntityID::CamController] = m_scene.createEntity();
    entities[EntityID::CamController].addComponent<cro::Transform>();
    //entities[EntityID::CamController].addComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::GroundPlane].getComponent<cro::Transform>());

    //axis icon
    meshID = m_resources.meshes.loadMesh(OriginIconBuilder());
    material = m_resources.materials.get(materialIDs[MaterialID::DebugDraw]);
    material.enableDepthTest = false;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = worldScales[m_preferences.unitsPerMetre];
        e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
    };
    entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //set the default sunlight properties
    m_scene.getSystem<cro::ShadowMapRenderer>().setProjectionOffset({ 0.f, 6.f, -5.f });
    m_scene.getSunlight().setDirection({ -0.f, -1.f, -0.f });
    m_scene.getSunlight().setProjectionMatrix(glm::ortho(-5.6f, 5.6f, -5.6f, 5.6f, 0.1f, 80.f));



    //create the material preview scene
    meshID = m_resources.meshes.loadMesh(cro::SphereBuilder(0.5f, 8));
    material = m_resources.materials.get(materialIDs[MaterialID::Default]);
    entity = m_previewScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), material);
    m_previewEntity = entity;

    //camera
    entity = m_previewScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
    entity.addComponent<cro::Camera>();
    auto& cam3D = entity.getComponent<cro::Camera>();
    cam3D.projectionMatrix = glm::perspective(DefaultFOV, 1.f, 0.1f, 10.f);

    m_previewScene.setActiveCamera(entity);

    //not rendering shadows on here, but we still want a light direction
    m_previewScene.getSunlight().setDirection({ 0.5f, -0.5f, -0.5f });
    m_previewScene.getSunlight().setProjectionMatrix(glm::ortho(-5.6f, 5.6f, -5.6f, 5.6f, 0.1f, 80.f));
}

void ModelState::buildUI()
{
    loadPrefs();
    registerWindow([&]()
        {
            if(ImGui::BeginMainMenuBar())
            {
                //file menu
                if (ImGui::BeginMenu("File##Model"))
                {
                    if (ImGui::MenuItem("Open Model", nullptr, nullptr))
                    {
                        if (m_preferences.workingDirectory.empty())
                        {
                            if (cro::FileSystem::showMessageBox("", "Working directory currently not set. Would you like to set one now?", cro::FileSystem::YesNo, cro::FileSystem::Question))
                            {
                                auto path = cro::FileSystem::openFolderDialogue(m_preferences.workingDirectory);
                                if (!path.empty())
                                {
                                    m_preferences.workingDirectory = path;
                                    std::replace(m_preferences.workingDirectory.begin(), m_preferences.workingDirectory.end(), '\\', '/');
                                }
                            }
                        }

                        openModel();
                    }
                    if (ImGui::MenuItem("Save", nullptr, nullptr, false))
                    {
                        //if a model is open overwrite the model def with current materials
                    }
                    if (ImGui::MenuItem("Save As...", nullptr, nullptr, false))
                    {
                        //if a model is open create a new model def with current materials
                    }                    
                    
                    if (ImGui::MenuItem("Close Model", nullptr, nullptr, entities[EntityID::ActiveModel].isValid()))
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
                        exportModel();
                    }
                    
                    ImGui::Separator();

                    if (getStateCount() > 1)
                    {
                        if (ImGui::MenuItem("Return To World Editor"))
                        {
                            getContext().mainWindow.setTitle("Crogine Editor");
                            requestStackPop();
                        }
                    }
                    
                    if (ImGui::MenuItem("Quit", nullptr, nullptr))
                    {
                        cro::App::quit();
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
                    if (ImGui::MenuItem("Ground Plane", nullptr, &m_showGroundPlane))
                    {
                        if (m_showGroundPlane)
                        {
                            //set this to whichever world scale we're currently using
                            updateWorldScale();
                        }
                        else
                        {
                            entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });
                        }
                        savePrefs();
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
                    if (m_preferences.workingDirectory.empty())
                    {
                        ImGui::Text("%s", "Not Set");
                    }
                    else
                    {
                        auto dir = m_preferences.workingDirectory.substr(0, 30) + "...";
                        ImGui::Text("%s", dir.c_str());
                        ImGui::SameLine();
                        helpMarker(m_preferences.workingDirectory.c_str());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse"))
                    {
                        auto path = cro::FileSystem::openFolderDialogue(m_preferences.workingDirectory);
                        if (!path.empty())
                        {
                            m_preferences.workingDirectory = path;
                            std::replace(m_preferences.workingDirectory.begin(), m_preferences.workingDirectory.end(), '\\', '/');
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

            drawInspector();
            drawBrowser();
        });

    auto size = getContext().mainWindow.getSize();
    updateLayout(size.x, size.y);
}

void ModelState::openModel()
{
    auto path = cro::FileSystem::openFileDialogue(m_preferences.lastModelDirectory, "cmt");
    if (!path.empty()
        && cro::FileSystem::getFileExtension(path) == ".cmt")
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        if (path.find(m_preferences.workingDirectory) == std::string::npos)
        {
            cro::FileSystem::showMessageBox("Warning", "This model was not opened from the current working directory.");
        }

        openModelAtPath(path);

        m_preferences.lastModelDirectory = cro::FileSystem::getFilePath(path);
    }
    else
    {
        cro::Logger::log(path + ": invalid file path", cro::Logger::Type::Error);
    }
}

void ModelState::openModelAtPath(const std::string& path)
{
    closeModel();

    cro::ModelDefinition def(m_preferences.workingDirectory);
    if (def.loadFromFile(path, m_resources))
    {
        entities[EntityID::ActiveModel] = m_scene.createEntity();
        entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::ActiveModel].addComponent<cro::Transform>());

        def.createModel(entities[EntityID::ActiveModel], m_resources);
        m_currentModelConfig.loadFromFile(path);

        const auto& meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
        if (meshData.boundingSphere.radius > (2.f * worldScales[m_preferences.unitsPerMetre]))
        {
            cro::Logger::log("Bounding sphere radius is very large - model may not be visible", cro::Logger::Type::Warning);
        }

        //if this is an IQM file load the vert data into the import
        //structures so we can adjust the model and re-export if needed
        if (cro::FileSystem::getFileExtension(path) == "*.iqm")
        {
            importIQM(path);
        }

        //parse all of the material properties into the material/texture browser
        const auto& objects = m_currentModelConfig.getObjects();
        std::uint32_t submeshID = 0;
        for (const auto& obj : objects)
        {
            if (obj.getName() == "material")
            {
                MaterialDefinition matDef;
                readMaterialDefinition(matDef, obj);
                
                matDef.submeshIDs.push_back(submeshID++);
                m_activeMaterials.push_back(static_cast<std::int32_t>(m_materialDefs.size()));
                addMaterialToBrowser(std::move(matDef));

                //don't add more materials than we can use
                if (m_activeMaterials.size() == meshData.submeshCount)
                {
                    break;
                }
            }
        }

        if (m_activeMaterials.size() < meshData.submeshCount)
        {
            //pad with default material
            auto defMat = m_resources.materials.get(materialIDs[MaterialID::Default]);
            for (auto i = m_activeMaterials.size(); i < meshData.submeshCount; ++i)
            {
                m_activeMaterials.push_back(-1);
                entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, defMat);
            }
        }

        //make sure to update the bounding display if needed
        std::optional<float> sphere;
        if (m_showSphere) sphere = meshData.boundingSphere.radius;

        std::optional<cro::Box> box;
        if (m_showAABB) box = meshData.boundingBox;
        updateGridMesh(entities[EntityID::GroundPlane].getComponent<cro::Model>().getMeshData(), sphere, box);
    }
    else
    {
        cro::Logger::log("Check current working directory (Options)?", cro::Logger::Type::Error);
    }
}

void ModelState::closeModel()
{
    if (entities[EntityID::ActiveModel].isValid())
    {
        m_scene.destroyEntity(entities[EntityID::ActiveModel]);
        entities[EntityID::ActiveModel] = {};

        m_importedIndexArrays.clear();
        m_importedVBO.clear();
    }

    if (entities[EntityID::NormalVis].isValid())
    {
        m_scene.destroyEntity(entities[EntityID::NormalVis]);
        entities[EntityID::NormalVis] = {};
    }

    m_currentModelConfig = {};

    //remove any IDs from active materials
    for (auto i : m_activeMaterials)
    {
        if (i > -1)
        {
            m_materialDefs[i].submeshIDs.clear();
        }
    }
    m_activeMaterials.clear();
}

void ModelState::importModel()
{
    auto calcFlags = [](const aiMesh * mesh, std::uint32_t& vertexSize)->std::uint8_t
    {
        std::uint8_t retVal = cro::VertexProperty::Position | cro::VertexProperty::Normal;
        vertexSize = 6;
        if (mesh->HasVertexColors(0))
        {
            retVal |= cro::VertexProperty::Colour;
            vertexSize += 3;
        }
        if (!mesh->HasNormals())
        {
            cro::Logger::log("Missing normals for mesh data", cro::Logger::Type::Warning);
            vertexSize = 0;
            return 0;
        }
        if (mesh->HasTangentsAndBitangents())
        {
            retVal |= cro::VertexProperty::Bitangent | cro::VertexProperty::Tangent;
            vertexSize += 6;
        }
        if (!mesh->HasTextureCoords(0))
        {
            cro::Logger::log("UV coordinates were missing in (sub)mesh", cro::Logger::Type::Warning);
        }
        else
        {
            retVal |= cro::VertexProperty::UV0;
            vertexSize += 2;
        }
        if (mesh->HasTextureCoords(1))
        {
            retVal |= cro::VertexProperty::UV1;
            vertexSize += 2;
        }

        return retVal;
    };

    auto path = cro::FileSystem::openFileDialogue(m_preferences.lastImportDirectory, "obj,dae,fbx");
    if (!path.empty())
    {
        ai::Importer importer;
        auto* scene = importer.ReadFile(path, 
                            aiProcess_CalcTangentSpace
                            | aiProcess_GenSmoothNormals
                            | aiProcess_Triangulate
                            | aiProcess_JoinIdenticalVertices
                            | aiProcess_OptimizeMeshes
                            | aiProcess_GenBoundingBoxes);

        if(scene && scene->HasMeshes())
        {
            //sort the meshes by material ready for concatenation
            std::vector<aiMesh*> meshes;
            for (auto i = 0u; i < scene->mNumMeshes; ++i)
            {
                meshes.push_back(scene->mMeshes[i]);
            }
            std::sort(meshes.begin(), meshes.end(), 
                [](const aiMesh* a, const aiMesh* b) 
                {
                    return a->mMaterialIndex > b->mMaterialIndex;
                });

            //prep the header
            std::uint32_t vertexSize = 0;
            CMFHeader header;
            header.flags = calcFlags(meshes[0], vertexSize);

            if (header.flags == 0)
            {
                cro::Logger::log("Invalid vertex data...", cro::Logger::Type::Error);
                return;
            }

            std::vector<std::vector<std::uint32_t>> importedIndexArrays;
            std::vector<float> importedVBO;

            //concat sub meshes assuming they meet the same flags
            for (const auto* mesh : meshes)
            {
                if (calcFlags(mesh, vertexSize) != header.flags)
                {
                    cro::Logger::log("sub mesh vertex data differs - skipping...", cro::Logger::Type::Warning);
                    continue;
                }

                if (header.arrayCount < 32)
                {
                    std::uint32_t offset = static_cast<std::uint32_t>(importedVBO.size()) / vertexSize;
                    for (auto i = 0u; i < mesh->mNumVertices; ++i)
                    {
                        auto pos = mesh->mVertices[i];
                        importedVBO.push_back(pos.x);
                        importedVBO.push_back(pos.y);
                        importedVBO.push_back(pos.z);

                        if (mesh->HasVertexColors(0))
                        {
                            auto colour = mesh->mColors[0][i];
                            importedVBO.push_back(colour.r);
                            importedVBO.push_back(colour.g);
                            importedVBO.push_back(colour.b);
                        }

                        auto normal = mesh->mNormals[i];
                        importedVBO.push_back(normal.x);
                        importedVBO.push_back(normal.y);
                        importedVBO.push_back(normal.z);

                        if (mesh->HasTangentsAndBitangents())
                        {
                            auto tan = mesh->mTangents[i];
                            importedVBO.push_back(tan.x);
                            importedVBO.push_back(tan.y);
                            importedVBO.push_back(tan.z);

                            auto bitan = mesh->mBitangents[i];
                            importedVBO.push_back(bitan.x);
                            importedVBO.push_back(bitan.y);
                            importedVBO.push_back(bitan.z);
                        }

                        if (mesh->HasTextureCoords(0))
                        {
                            auto coord = mesh->mTextureCoords[0][i];
                            importedVBO.push_back(coord.x);
                            importedVBO.push_back(coord.y);
                        }

                        if (mesh->HasTextureCoords(1))
                        {
                            auto coord = mesh->mTextureCoords[1][i];
                            importedVBO.push_back(coord.x);
                            importedVBO.push_back(coord.y);
                        }
                    }

                    auto idx = mesh->mMaterialIndex;

                    while (idx >= importedIndexArrays.size())
                    {
                        //create an index array
                        importedIndexArrays.emplace_back();
                    }

                    for (auto i = 0u; i < mesh->mNumFaces; ++i)
                    {
                        importedIndexArrays[idx].push_back(mesh->mFaces[i].mIndices[0] + offset);
                        importedIndexArrays[idx].push_back(mesh->mFaces[i].mIndices[1] + offset);
                        importedIndexArrays[idx].push_back(mesh->mFaces[i].mIndices[2] + offset);
                    }
                }
                else
                {
                    cro::Logger::log("Max materials have been reached - model may be incomplete", cro::Logger::Type::Warning);
                }
            }

            //remove any empty arrays
            importedIndexArrays.erase(std::remove_if(importedIndexArrays.begin(), importedIndexArrays.end(), 
                [](const std::vector<std::uint32_t>& arr)
                {
                    return arr.empty();
                }), importedIndexArrays.end());

            //update header with array sizes
            for (const auto& indices : importedIndexArrays)
            {
                header.arraySizes.push_back(static_cast<std::int32_t>(indices.size() * sizeof(std::uint32_t)));
            }
            header.arrayCount = static_cast<std::uint8_t>(importedIndexArrays.size());

            auto indexOffset = sizeof(header.flags) + sizeof(header.arrayCount) + sizeof(header.arrayOffset) + (header.arraySizes.size() * sizeof(std::int32_t));
            indexOffset += sizeof(float) * importedVBO.size();
            header.arrayOffset = static_cast<std::int32_t>(indexOffset);

            if (header.arrayCount > 0 && !importedVBO.empty() && !importedIndexArrays.empty())
            {
                closeModel();
                
                m_importedHeader = header;
                m_importedIndexArrays.swap(importedIndexArrays);
                m_importedVBO.swap(importedVBO);

                
                //TODO check the size and flush after a certain amount
                //remembering to reload the ground plane..
                //m_resources.meshes.flush();
                ImportedMeshBuilder builder(m_importedHeader, m_importedVBO, m_importedIndexArrays, header.flags);
                auto meshID = m_resources.meshes.loadMesh(builder);

                entities[EntityID::ActiveModel] = m_scene.createEntity();
                entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::ActiveModel].addComponent<cro::Transform>());
                entities[EntityID::ActiveModel].addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialIDs[MaterialID::Default]));
                
                for (auto i = 0; i < header.arrayCount; ++i)
                {
                    entities[EntityID::ActiveModel].getComponent<cro::Model>().setShadowMaterial(i, m_resources.materials.get(materialIDs[MaterialID::DefaultShadow]));
                }
                entities[EntityID::ActiveModel].addComponent<cro::ShadowCaster>();

                m_importedTransform = {};

                //updateNormalVis();
            }
            else
            {
                cro::Logger::log("Failed opening " + path, cro::Logger::Type::Error);
                cro::Logger::log("Missing index or vertex data", cro::Logger::Type::Error);
            }
        }

        m_preferences.lastImportDirectory = cro::FileSystem::getFilePath(path);
        savePrefs();
    }
}

void ModelState::exportModel()
{
    //TODO assert we at least have valid header data
    //prevent accidentally writing a bad file

    auto path = cro::FileSystem::saveFileDialogue(m_preferences.lastExportDirectory + "/untitled", "cmf");
    std::replace(path.begin(), path.end(), '\\', '/');
    if (!path.empty())
    {
        if (cro::FileSystem::getFileExtension(path) != ".cmf")
        {
            path += ".cmf";
        }

        //write binary file
        cro::RaiiRWops file;
        file.file = SDL_RWFromFile(path.c_str(), "wb");
        if (file.file)
        {
            SDL_RWwrite(file.file, &m_importedHeader.flags, sizeof(CMFHeader::flags), 1);
            SDL_RWwrite(file.file, &m_importedHeader.arrayCount, sizeof(CMFHeader::arrayCount), 1);
            SDL_RWwrite(file.file, &m_importedHeader.arrayOffset, sizeof(CMFHeader::arrayOffset), 1);
            SDL_RWwrite(file.file, m_importedHeader.arraySizes.data(), sizeof(std::int32_t), m_importedHeader.arraySizes.size());
            SDL_RWwrite(file.file, m_importedVBO.data(), sizeof(float), m_importedVBO.size());
            for (const auto& indices : m_importedIndexArrays)
            {
                SDL_RWwrite(file.file, indices.data(), sizeof(std::uint32_t), indices.size());
            }

            SDL_RWclose(file.file);
            file.file = nullptr;

            //create config file and save as cmt
            auto modelName = cro::FileSystem::getFileName(path);
            modelName = modelName.substr(0, modelName.find_last_of('.'));

            //auto meshPath = path.substr(m_preferences.workingDirectory.length() + 1);
            std::string meshPath;
            if (!m_preferences.workingDirectory.empty() && path.find(m_preferences.workingDirectory) != std::string::npos)
            {
                meshPath = path.substr(m_preferences.workingDirectory.length() + 1);
            }
            else
            {
                meshPath = cro::FileSystem::getFileName(path);
            }

            std::replace(meshPath.begin(), meshPath.end(), '\\', '/');

            cro::ConfigFile cfg("model", modelName);
            cfg.addProperty("mesh", meshPath);
            
            //material placeholder for each sub mesh
            for (auto i = 0u; i < m_importedHeader.arrayCount; ++i)
            {
                auto material = cfg.addObject("material", "VertexLit");
                material->addProperty("colour", "1,0,1,1");
            }


            path.back() = 't';
            
            //TODO make this an option so we don't accidentally overwrite files
            //if (!cro::FileSystem::fileExists(path))
            {
                cfg.save(path);
            }

            m_preferences.lastExportDirectory = cro::FileSystem::getFilePath(path);
            savePrefs();

            openModelAtPath(path);
        }
    }
}

void ModelState::applyImportTransform()
{
    //keep rotation separate as we don't apply scale to normal data
    auto rotation = glm::toMat4(glm::toQuat(glm::orientate3(m_importedTransform.rotation)));
    auto transform = rotation * glm::scale(glm::mat4(1.f), glm::vec3(m_importedTransform.scale));

    auto meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
    auto vertexSize = meshData.vertexSize / sizeof(float);

    std::size_t normalOffset = 0;
    std::size_t tanOffset = 0;
    std::size_t bitanOffset = 0;

    //calculate the offset index into a single vertex which points
    //to any normal / tan / bitan values
    if (meshData.attributes[cro::Mesh::Attribute::Normal] != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Normal; ++i)
        {
            normalOffset += meshData.attributes[i];
        }
    }

    if (meshData.attributes[cro::Mesh::Attribute::Tangent] != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Tangent; ++i)
        {
            tanOffset += meshData.attributes[i];
        }
    }

    if (meshData.attributes[cro::Mesh::Attribute::Bitangent] != 0)
    {
        for (auto i = 0; i < cro::Mesh::Attribute::Bitangent; ++i)
        {
            bitanOffset += meshData.attributes[i];
        }
    }

    auto applyTransform = [&](const glm::mat4& tx, std::size_t idx)
    {
        glm::vec4 v(m_importedVBO[idx], m_importedVBO[idx + 1], m_importedVBO[idx + 2], 1.f);
        v = tx * v;
        m_importedVBO[idx] = v.x;
        m_importedVBO[idx+1] = v.y;
        m_importedVBO[idx+2] = v.z;
    };

    //loop over the vertex data and modify
    for (std::size_t i = 0u; i < m_importedVBO.size(); i += vertexSize)
    {
        //position
        applyTransform(transform, i);

        if (normalOffset != 0)
        {
            auto idx = i + normalOffset;
            applyTransform(rotation, idx);
        }

        if (tanOffset != 0)
        {
            auto idx = i + tanOffset;
            applyTransform(rotation, idx);
        }

        if (bitanOffset != 0)
        {
            auto idx = i + bitanOffset;
            applyTransform(rotation, idx);
        }
    }

    //upload the data to the preview model
    glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo);
    glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, m_importedVBO.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_importedTransform = {};
    entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(1.f));
    entities[EntityID::ActiveModel].getComponent<cro::Transform>().setRotation(glm::vec3(0.f));
}

void ModelState::loadPrefs()
{
    cro::ConfigFile prefs;
    if (prefs.loadFromFile(prefPath))
    {
        const auto& props = prefs.getProperties();
        for (const auto& prop : props)
        {
            auto name = cro::Util::String::toLower(prop.getName());
            if (name == "working_dir")
            {
                m_preferences.workingDirectory = prop.getValue<std::string>();
                std::replace(m_preferences.workingDirectory.begin(), m_preferences.workingDirectory.end(), '\\', '/');
            }
            else if (name == "units_per_metre")
            {
                m_preferences.unitsPerMetre = cro::Util::Maths::clamp(static_cast<std::size_t>(prop.getValue<std::int32_t>()), std::size_t(0u), worldScales.size());
            }
            else if (name == "show_groundplane")
            {
                m_showGroundPlane = prop.getValue<bool>();
            }
            else if (name == "show_skybox")
            {
                m_showSkybox = prop.getValue<bool>();
                if (m_showSkybox)
                {
                    m_scene.enableSkybox();
                }
            }
            else if (name == "sky_top")
            {
                m_preferences.skyTop = prop.getValue<cro::Colour>();
                m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
            }
            else if (name == "sky_bottom")
            {
                m_preferences.skyBottom = prop.getValue<cro::Colour>();
                m_scene.setSkyboxColours(m_preferences.skyBottom, m_preferences.skyTop);
            }
            else if (name == "import_dir")
            {
                m_preferences.lastImportDirectory = prop.getValue<std::string>();
            }
            else if (name == "export_dir")
            {
                m_preferences.lastExportDirectory = prop.getValue<std::string>();
            }
            else if (name == "model_dir")
            {
                m_preferences.lastModelDirectory = prop.getValue<std::string>();
            }
        }

        updateWorldScale();
    }
}

void ModelState::savePrefs()
{
    auto toString = [](cro::Colour c)->std::string
    {
        return std::to_string(c.getRed()) + ", " + std::to_string(c.getGreen()) + ", " + std::to_string(c.getBlue()) + ", " + std::to_string(c.getAlpha());
    };

    cro::ConfigFile prefsOut;
    prefsOut.addProperty("working_dir", m_preferences.workingDirectory);
    prefsOut.addProperty("units_per_metre", std::to_string(m_preferences.unitsPerMetre));
    prefsOut.addProperty("show_groundplane", m_showGroundPlane ? "true" : "false");
    prefsOut.addProperty("show_skybox", m_showSkybox ? "true" : "false");
    prefsOut.addProperty("sky_top", toString(m_preferences.skyTop));
    prefsOut.addProperty("sky_bottom", toString(m_preferences.skyBottom));

    prefsOut.addProperty("import_dir", m_preferences.lastImportDirectory);
    prefsOut.addProperty("export_dir", m_preferences.lastExportDirectory);
    prefsOut.addProperty("model_dir", m_preferences.lastModelDirectory);

    prefsOut.save(prefPath);
}

void ModelState::updateWorldScale()
{
    const float scale = worldScales[m_preferences.unitsPerMetre];
    if (m_showGroundPlane)
    {
        entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale({ scale, scale, scale });
    }
    else
    {
        entities[EntityID::GroundPlane].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    }
    m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition(DefaultCameraPosition * scale);
    
    entities[EntityID::CamController].getComponent<cro::Transform>().setPosition(glm::vec3(0.f));
    updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], m_fov);
}

void ModelState::updateNormalVis()
{
    //TODO this technically works but only on imported previews where m_importedVBO is populated.
    /*
    This is actually not what we want - rather it should only be displayed on the loaded model. If we
    track the loaded model file name (which ought to be in m_currentModelConfig) we can load the VBO
    data from there when needed (and check not already loaded etc to facilitate show/hiding already 
    loaded data). This also means that the entity used only needs a single 'DynamicMesh' where we update
    the vertex data each time instead of re-creating a new mesh entirely. Also means we can ditch the
    normal mesh vis class.
    */


    if (entities[EntityID::ActiveModel].isValid())
    {
        if (entities[EntityID::NormalVis].isValid())
        {
            m_scene.destroyEntity(entities[EntityID::NormalVis]);
        }

        const auto& meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();

        //pass to mesh builder - TODO we would be better recycling the VBO with new vertex data, rather than
        //destroying and creating a new one (unique instances will build up in the resource manager)
        auto meshID = m_resources.meshes.loadMesh(NormalVisMeshBuilder(meshData, m_importedVBO));

        entities[EntityID::NormalVis] = m_scene.createEntity();
        entities[EntityID::NormalVis].addComponent<cro::Transform>();
        entities[EntityID::NormalVis].addComponent<cro::Model>(m_resources.meshes.getMesh(meshID), m_resources.materials.get(materialIDs[MaterialID::DebugDraw]));
        entities[EntityID::CamController].getComponent<cro::Transform>().addChild(entities[EntityID::NormalVis].getComponent<cro::Transform>());
    }
}

void ModelState::updateMouseInput(const cro::Event& evt)
{
    const float moveScale = 0.004f;
    if (evt.motion.state & SDL_BUTTON_LMASK)
    {
        float pitchMove = static_cast<float>(evt.motion.yrel)* moveScale;
        float yawMove = static_cast<float>(evt.motion.xrel)* moveScale;

        auto& tx = entities[EntityID::CamController].getComponent<cro::Transform>();

        glm::quat pitch = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), pitchMove, glm::vec3(1.f, 0.f, 0.f));
        glm::quat yaw = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), yawMove, glm::vec3(0.f, 1.f, 0.f));

        auto rotation =  pitch * yaw * tx.getRotationQuat();
        tx.setRotation(rotation);
    }
    else if (evt.motion.state & SDL_BUTTON_RMASK)
    {
        //do roll
        float rollMove = static_cast<float>(-evt.motion.xrel)* moveScale;

        auto& tx = entities[EntityID::CamController].getComponent<cro::Transform>();

        glm::quat roll = glm::rotate(glm::quat(1.f, 0.f, 0.f, 0.f), rollMove, glm::vec3(0.f, 0.f, 1.f));

        auto rotation = roll * tx.getRotationQuat();
        tx.setRotation(rotation);
    }
    else if (evt.motion.state & SDL_BUTTON_MMASK)
    {
        auto& tx = entities[EntityID::CamController].getComponent<cro::Transform>();
        tx.move((glm::vec3(evt.motion.xrel, -evt.motion.yrel, 0.f) / 160.f) * worldScales[m_preferences.unitsPerMetre]);
    }
}

void ModelState::updateGridMesh(cro::Mesh::Data& meshData, std::optional<float> radius, std::optional<cro::Box> boundingBox)
{
    std::vector<float> verts;
    auto addVert = [&verts](glm::vec3 pos, glm::vec4 colour)
    {
        //pos
        verts.push_back(pos.x);
        verts.push_back(pos.y);
        verts.push_back(pos.z);

        //colour
        verts.push_back(colour.r);
        verts.push_back(colour.g);
        verts.push_back(colour.b);
        verts.push_back(colour.a);
    };    
    
    //yeah we don't really need the middle 4 verts
    //but it's just easier to build in a loop
    const float GridWidth = 3.f;
    const glm::vec4 grey(0.5f, 0.5f, 0.5f, 1.f);
    for (auto y = 0; y < 4; ++y)
    {
        for (auto x = 0; x < 4; ++x)
        {
            addVert({ static_cast<float>(x) - (GridWidth / 2.f) , 0.f, static_cast<float>(y) - (GridWidth / 2.f) }, grey);
        }
    }
    std::vector<std::uint32_t> indices =
    {
        0,3,15,12,0,
        1,13,14,2,3,
        7,4,8,11
    };

    auto vertStride = (meshData.vertexSize / sizeof(float));
    std::vector<float> lastVert(verts.end() - vertStride, verts.end());
    lastVert.back() = 0.f; //set alpha to zero. problem with vert colours means we dupe verts, but mehhhhh

    auto currentIndex = static_cast<std::uint32_t>(verts.size() / vertStride);

    const glm::vec4 transparent(0.f);

    if (radius)
    {
        const glm::vec4 green(0.1f, 0.9f, 0.4f, 1.f);
        constexpr std::int32_t SegmentCount = 16;
        constexpr float step = cro::Util::Const::TAU / SegmentCount;
        float rad = radius.value();

        std::vector<glm::vec2> points;
        for (auto i = 0; i < SegmentCount; ++i)
        {
            points.emplace_back(std::sin(i * step), std::cos(i * step));
            points.back() *= rad;
        }

        //draw the circle in all 3 planes
        //starting with transparent segment
        verts.insert(verts.end(), lastVert.begin(), lastVert.end());
        indices.push_back(currentIndex++);

        addVert({ points.front().x, points.front().y, 0.f }, transparent);
        indices.push_back(currentIndex++);

        for (auto p : points)
        {
            addVert({ p.x, p.y, 0.f }, green);
            indices.push_back(currentIndex++);
        }
        //close the circle
        indices.push_back(currentIndex - static_cast<std::uint32_t>(points.size()));

        //transparent segment
        addVert({ points.back().x, points.back().y, 0.f }, transparent);
        indices.push_back(currentIndex++);


        addVert({ points.front().x, 0.f, points.front().y }, transparent);
        indices.push_back(currentIndex++);

        for (auto p : points)
        {
            addVert({ p.x, 0.f, p.y }, green);
            indices.push_back(currentIndex++);
        }
        //close the circle
        indices.push_back(currentIndex - static_cast<std::uint32_t>(points.size()));

        //transparent segment
        addVert({ points.back().x, 0.f, points.back().y }, transparent);
        indices.push_back(currentIndex++);


        addVert({ 0.f, points.front().x, points.front().y }, transparent);
        indices.push_back(currentIndex++);

        for (auto p : points)
        {
            addVert({ 0.f, p.x, p.y }, green);
            indices.push_back(currentIndex++);
        }
        //close the circle
        indices.push_back(currentIndex - static_cast<std::uint32_t>(points.size()));

        //update lastVert in case we're also builind an AABB
        lastVert = { verts.end() - vertStride, verts.end() };
        lastVert.back() = 0.f;
    }

    if (boundingBox)
    {
        const glm::vec4 red(0.8f, 0.3f, 0.2f, 1.f);
        cro::Box box = boundingBox.value();

        //transparent seg
        verts.insert(verts.end(), lastVert.begin(), lastVert.end());
        indices.push_back(currentIndex++);

        addVert(box[0], transparent);
        indices.push_back(currentIndex++);

        //create the 8 verts
        addVert(box[0], red);
        addVert({ box[1].x, box[0].y, box[0].z }, red);
        addVert({ box[0].x, box[0].y, box[1].z }, red);
        addVert({ box[1].x, box[0].y, box[1].z }, red);

        addVert({ box[0].x, box[1].y, box[0].z }, red);
        addVert({ box[1].x, box[1].y, box[0].z }, red);
        addVert({ box[0].x, box[1].y, box[1].z }, red);
        addVert(box[1], red);


        //then add the indices
        std::vector<std::uint32_t> boxIndices =
        {
            0,1,3,2,0,  4,5,7,6,4,6,  2,3,7,5,1
        };
        for (auto i : boxIndices)
        {
            indices.push_back(i + currentIndex);
        }
    }


    meshData.vertexCount = verts.size() / vertStride;
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vbo));
    glCheck(glBufferData(GL_ARRAY_BUFFER, meshData.vertexSize * meshData.vertexCount, verts.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ARRAY_BUFFER, 0));

    auto& submesh = meshData.indexData[0];
    submesh.indexCount = indices.size();
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, submesh.ibo));
    glCheck(glBufferData(GL_ELEMENT_ARRAY_BUFFER, submesh.indexCount * sizeof(std::uint32_t), indices.data(), GL_STATIC_DRAW));
    glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

std::uint32_t ModelState::addTextureToBrowser(const std::string& path)
{
    auto fileName = cro::FileSystem::getFileName(path);

    auto relPath = path;
    std::replace(relPath.begin(), relPath.end(), '\\', '/');
    relPath = cro::FileSystem::getFilePath(relPath);
    relPath = cro::FileSystem::getRelativePath(relPath, m_preferences.workingDirectory);

    std::uint32_t id = 0;
    for (auto& [i, t] : m_materialTextures)
    {
        if (t.name == fileName)
        {
            id = i;
            break;
        }
    }

    bool updateMaterials = false;
    if (id != 0)
    {
        m_materialTextures.erase(id);
        updateMaterials = true;
    }
    m_selectedTexture = 0;


    std::uint32_t retVal = 0;

    MaterialTexture tex;
    tex.texture = std::make_unique<cro::Texture>();
    if (tex.texture->loadFromFile(path))
    {
        m_selectedTexture = tex.texture->getGLHandle();
        tex.name = fileName;
        tex.relPath = relPath;
        m_materialTextures.insert(std::make_pair(tex.texture->getGLHandle(), std::move(tex)));
        retVal = m_selectedTexture;
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Failed to open image", cro::FileSystem::OK, cro::FileSystem::Error);
    }

    if (updateMaterials)
    {
        for (auto& mat : m_materialDefs)
        {
            for (auto& texID : mat.textureIDs)
            {
                if (texID == id)
                {
                    texID = retVal;
                }
            }
        }
    }

    return retVal;
}

void ModelState::addMaterialToBrowser(MaterialDefinition&& def)
{
    auto shaderType = cro::ShaderResource::Unlit;
    if (def.type == MaterialDefinition::VertexLit)
    {
        shaderType = cro::ShaderResource::VertexLit;
        if (def.textureIDs[MaterialDefinition::Normal])
        {
            def.shaderFlags |= cro::ShaderResource::NormalMap;
        }
    }

    if (def.textureIDs[MaterialDefinition::Lightmap])
    {
        def.shaderFlags |= cro::ShaderResource::LightMap;
    }

    if (def.textureIDs[MaterialDefinition::Mask])
    {
        def.shaderFlags |= cro::ShaderResource::MaskMap;
    }

    if (def.textureIDs[MaterialDefinition::Diffuse])
    {
        def.shaderFlags |= cro::ShaderResource::DiffuseMap;
    }
    else
    {
        def.shaderFlags |= cro::ShaderResource::DiffuseColour;
    }

    if (def.alphaClip > 0)
    {
        def.shaderFlags |= cro::ShaderResource::AlphaClip;
    }

    if (def.vertexColoured)
    {
        def.shaderFlags |= cro::ShaderResource::VertexColour;
    }

    if (def.receiveShadows)
    {
        def.shaderFlags |= cro::ShaderResource::RxShadows;
    }

    if (def.useRimlighing)
    {
        def.shaderFlags |= cro::ShaderResource::RimLighting;
    }

    def.shaderID = m_resources.shaders.loadBuiltIn(shaderType, def.shaderFlags);
    def.materialData.setShader(m_resources.shaders.get(def.shaderID));

    //update the thumbnail
    applyPreviewSettings(def);
    m_previewEntity.getComponent<cro::Model>().setMaterial(0, def.materialData);

    def.previewTexture.clear(ui::PreviewClearColour);
    m_previewScene.render(def.previewTexture);
    def.previewTexture.display();

    m_materialDefs.push_back(std::move(def));
    m_selectedMaterial = m_materialDefs.size() - 1;
}

void ModelState::applyPreviewSettings(MaterialDefinition& matDef)
{
    if (matDef.textureIDs[MaterialDefinition::Diffuse])
    {
        matDef.materialData.setProperty("u_diffuseMap", *m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Diffuse]).texture);
        if (matDef.alphaClip > 0)
        {
            matDef.materialData.setProperty("u_alphaClip", matDef.alphaClip);
        }
    }

    if (matDef.textureIDs[MaterialDefinition::Mask])
    {
        matDef.materialData.setProperty("u_maskMap", *m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Mask]).texture);
    }
    else if(matDef.type == MaterialDefinition::VertexLit)
    {
        matDef.materialData.setProperty("u_maskColour", matDef.maskColour);
    }

    if (matDef.textureIDs[MaterialDefinition::Normal])
    {
        matDef.materialData.setProperty("u_normalMap", *m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Normal]).texture);
    }

    if (matDef.textureIDs[MaterialDefinition::Lightmap])
    {
        matDef.materialData.setProperty("u_lightMap", *m_materialTextures.at(matDef.textureIDs[MaterialDefinition::Lightmap]).texture);
    }

    if (matDef.shaderFlags & cro::ShaderResource::DiffuseColour)
    {
        matDef.materialData.setProperty("u_colour", matDef.colour);
    }

    if (matDef.useRimlighing)
    {
        matDef.materialData.setProperty("u_rimColour", matDef.rimlightColour);
        matDef.materialData.setProperty("u_rimFalloff", matDef.rimlightFalloff);
    }

    matDef.materialData.blendMode = matDef.blendMode;
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
}

void ModelState::drawInspector()
{
    auto [pos, size] = WindowLayouts[WindowID::Inspector];

    ImGui::SetNextWindowPos({ pos.x, pos.y });
    ImGui::SetNextWindowSize({ size.x, size.y });
    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::BeginTabBar("##a");

        if (ImGui::BeginTabItem("Model"))
        {
            //model details
            if (entities[EntityID::ActiveModel].isValid())
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
                    ImGui::Text("%s", flags.c_str());
                    
                    ImGui::NewLine();
                    ImGui::Separator();

                    ImGui::NewLine();
                    ImGui::Text("Transform"); ImGui::SameLine(); helpMarker("Double Click to change Values");
                    if (ImGui::DragFloat3("Rotation", &m_importedTransform.rotation[0], -180.f, 180.f))
                    {
                        entities[EntityID::ActiveModel].getComponent<cro::Transform>().setRotation(m_importedTransform.rotation * cro::Util::Const::degToRad);
                    }
                    if (ImGui::DragFloat("Scale", &m_importedTransform.scale, 0.01f, 0.1f, 10.f))
                    {
                        //scale needs to be uniform, else we'd have to recalc all the normal data
                        m_importedTransform.scale = std::min(10.f, std::max(0.1f, m_importedTransform.scale));
                        entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }
                    ImGui::Text("Quick Scale:"); ImGui::SameLine();
                    if (ImGui::Button("0.5"))
                    {
                        m_importedTransform.scale = std::max(0.1f, m_importedTransform.scale / 2.f);
                        entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }ImGui::SameLine();
                    if (ImGui::Button("2.0"))
                    {
                        m_importedTransform.scale = std::min(10.f, m_importedTransform.scale * 2.f);
                        entities[EntityID::ActiveModel].getComponent<cro::Transform>().setScale(glm::vec3(m_importedTransform.scale));
                    }
                    if (ImGui::Button("Apply Transform"))
                    {
                        applyImportTransform();
                    }
                    ImGui::SameLine();
                    helpMarker("Applies this transform directly to the vertex data, before exporting the model.\nUseful if an imported model uses z-up coordinates, or is much\nlarger or smaller than other models in the scene.\nTIP: if a model doesn't scale enough in either direction try applying the current scale first before rescaling");

                    ImGui::NewLine();
                    if (ImGui::Button("Convert##01"))
                    {
                        exportModel();
                    }
                    ImGui::SameLine();
                    helpMarker("Export this model to Crogine format, and create a model definition file.\nThe model will then be automatically re-opened for material editing.");
                }
                else
                {
                    const auto& meshData = entities[EntityID::ActiveModel].getComponent<cro::Model>().getMeshData();
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
                                entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, m_materialDefs[m_activeMaterials[i]].materialData);
                            }
                            else
                            {
                                //apply default material
                                entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, m_resources.materials.get(materialIDs[MaterialID::Default]));
                            }
                        }
                    }
                    ImGui::NewLine();
                    ImGui::Text("Vertex Attributes:");
                    for (auto i = 0u; i < meshData.attributes.size(); ++i)
                    {
                        if (meshData.attributes[i] > 0)
                        {
                            ImGui::Text(AttribStrings[i]);
                        }
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
                        std::optional<float> sphere;
                        if (m_showSphere) sphere = meshData.boundingSphere.radius;
                        
                        std::optional<cro::Box> box;
                        if (m_showAABB) box = meshData.boundingBox;

                        updateGridMesh(entities[EntityID::GroundPlane].getComponent<cro::Model>().getMeshData(), sphere, box);
                    }

                    if (entities[EntityID::ActiveModel].hasComponent<cro::Skeleton>())
                    {
                        auto& skeleton = entities[EntityID::ActiveModel].getComponent<cro::Skeleton>();

                        ImGui::NewLine();
                        ImGui::Separator();
                        ImGui::NewLine();

                        static bool showSkeleton = false;
                        if (ImGui::Checkbox("Show Skeleton", &showSkeleton))
                        {
                            //TODO toggle skeleton display
                        }

                        ImGui::Text("Animations: %d", skeleton.animations.size());
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
                                && !skeleton.animations[currentAnim].playing)
                            {
                                currentAnim = std::min(currentAnim, static_cast<int>(skeleton.animations.size()) - 1);
                            }
                            else
                            {
                                currentAnim = prevAnim;
                            }

                            ImGui::SameLine();
                            if (skeleton.animations[currentAnim].playing)
                            {
                                if (ImGui::Button("Stop"))
                                {
                                    skeleton.animations[currentAnim].playing = false;
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

        if (ImGui::BeginTabItem("Material"))
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
                ImGui::PushItemWidth(size.x* ui::TextBoxWidth);
                if (ImGui::SliderFloat("##rotation", &rotation, -180.f, 180.f))
                {
                    m_previewEntity.getComponent<cro::Transform>().setRotation({ 0.f, 0.f, rotation * cro::Util::Const::degToRad });
                }
                ImGui::PopItemWidth();

                ImGui::NewLine();
                ImGui::Text("Shader Type:");
                ImGui::PushItemWidth(size.x* ui::TextBoxWidth);
                if (ImGui::BeginCombo("##Shader", ShaderStrings[matDef.type]))
                {
                    for (auto i = 0; i < ShaderStrings.size(); ++i)
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

                std::int32_t shaderFlags = 0;
                bool applyMaterial = false;

                ImGui::NewLine();
                ImGui::Text("Texture Maps:");
                auto type = matDef.type;
                std::string slotLabel;
                std::uint32_t thumb = m_blackTexture.getGLHandle();
                if (type != MaterialDefinition::PBR)
                {
                    //diffuse map
                    slotLabel = "Diffuse";
                    
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
                    }

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

                    if (matDef.textureIDs[MaterialDefinition::Lightmap] != 0)
                    {
                        shaderFlags |= cro::ShaderResource::LightMap;
                    }
                }

                if (type == MaterialDefinition::VertexLit)
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
                }

                if (type != MaterialDefinition::Unlit)
                {
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

                if (type == MaterialDefinition::PBR)
                {
                    ImGui::Text("Coming Soon!");
                    //metal map
                    //drawTextureSlot("Metalness", m_materialDefs[m_selectedMaterial].metalness, m_materialThumb);
                    //spec map
                    //ao map
                }

                ImGui::NewLine();
                if (matDef.textureIDs[MaterialDefinition::Diffuse] == 0)
                {
                    if (ImGui::ColorEdit4("Diffuse Colour", matDef.colour.asArray()))
                    {
                        applyMaterial = true;
                    }
                    ImGui::SameLine();
                    helpMarker("If the Diffuse texture map is not set then this defines the diffuse colour of the material");

                    shaderFlags |= cro::ShaderResource::DiffuseColour;
                }
                if (type == MaterialDefinition::VertexLit
                    && matDef.textureIDs[MaterialDefinition::Mask] == 0)
                {
                    if (ImGui::ColorEdit3("Mask Colour", matDef.maskColour.asArray()))
                    {
                        applyMaterial = true;
                    }
                    ImGui::SameLine();
                    helpMarker("If the Mask texture map is not set then this defines the mask colour of the material");
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
                ImGui::Checkbox("Use Vertex Colours", &matDef.vertexColoured);
                ImGui::SameLine();
                helpMarker("Any colour information stored in the model's vertex data will be multiplied with the diffuse colour of the material");
                if (matDef.vertexColoured)
                {
                    shaderFlags |= cro::ShaderResource::VertexColour;
                }

                ImGui::Checkbox("Receive Shadows", &matDef.receiveShadows);
                ImGui::SameLine();
                helpMarker("Check this box if the material should receive shadows from the active shadow map");
                if (matDef.receiveShadows)
                {
                    shaderFlags |= cro::ShaderResource::RxShadows;
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

                ImGui::NewLine();
                ImGui::Text("Blend Mode:");
                ImGui::PushItemWidth(size.x * ui::TextBoxWidth);
                auto oldMode = matDef.blendMode;
                if (ImGui::BeginCombo("##BlendMode", BlendStrings[static_cast<std::int32_t>(matDef.blendMode)]))
                {
                    for (auto i = 0; i < BlendStrings.size(); ++i)
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


                if (matDef.shaderFlags != shaderFlags
                    || matDef.activeType != matDef.type)
                {
                    auto shaderType = cro::ShaderResource::Unlit;
                    if (matDef.type == MaterialDefinition::VertexLit)
                    {
                        shaderType = cro::ShaderResource::VertexLit;
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
                        entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(idx, matDef.materialData);
                    }
                }
            }

            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Texture"))
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
                }

                bool repeated = texture.texture->isRepeated();
                if (ImGui::Checkbox("Repeated", &repeated))
                {
                    texture.texture->setRepeated(repeated);
                }
            }
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
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
                    m_materialDefs.back().materialData = m_resources.materials.get(materialIDs[MaterialID::Default]);
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
                auto path = cro::FileSystem::openFileDialogue(m_preferences.workingDirectory + "/untitled", "mdf,cmt");
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
                        auto defMat = m_resources.materials.get(materialIDs[MaterialID::Default]);
                        for (auto i : m_materialDefs[m_selectedMaterial].submeshIDs)
                        {
                            m_activeMaterials[i] = -1;
                            entities[EntityID::ActiveModel].getComponent<cro::Model>().setMaterial(i, defMat);
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
                        m_previewEntity.getComponent<cro::Model>().setMaterial(0, m_materialDefs[m_selectedMaterial].materialData);

                        m_materialDefs[m_selectedMaterial].previewTexture.clear(ui::PreviewClearColour);
                        m_previewScene.render(m_materialDefs[m_selectedMaterial].previewTexture);
                        m_materialDefs[m_selectedMaterial].previewTexture.display();
                    }
                }
            }

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

                ImGui::BeginChildFrame(7767 + count, { frameSize.x, frameSize.y}, ImGuiWindowFlags_NoScrollbar);

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
                    ImGui::Text(ShaderStrings[material.type]);
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
                auto path = cro::FileSystem::openFileDialogue("", "png,jpg,bmp");
                if (!path.empty()
                    && m_materialTextures.size() < ui::MaxMaterials)
                {
                    addTextureToBrowser(path);
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
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Reload##00"))
            {
                if (m_selectedTexture != 0)
                {
                    auto path = m_preferences.workingDirectory + "/" + m_materialTextures.at(m_selectedTexture).relPath + m_materialTextures.at(m_selectedTexture).name;
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

void ModelState::exportMaterial() const
{
    auto path = cro::FileSystem::saveFileDialogue(m_preferences.workingDirectory + "/untitled", "mdf");
    if (!path.empty())
    {
        if (cro::FileSystem::getFileExtension(path) != ".mdf")
        {
            path += ".mdf";
        }

        const auto& matDef = m_materialDefs[m_selectedMaterial];
        auto name = matDef.name;
        std::replace(name.begin(), name.end(), ' ', '_');

        cro::ConfigFile file("material_definition", name);
        file.addProperty("type").setValue(matDef.type);
        file.addProperty("colour").setValue(matDef.colour.getVec4());
        file.addProperty("mask_colour").setValue(matDef.maskColour.getVec4());
        file.addProperty("alpha_clip").setValue(matDef.alphaClip);
        file.addProperty("vertex_coloured").setValue(matDef.vertexColoured);
        file.addProperty("rx_shadow").setValue(matDef.receiveShadows);
        file.addProperty("blend_mode").setValue(static_cast<std::int32_t>(matDef.blendMode));
        file.addProperty("use_rimlight").setValue(matDef.useRimlighing);
        file.addProperty("rimlight_falloff").setValue(matDef.rimlightFalloff);
        file.addProperty("rimlight_colour").setValue(matDef.rimlightColour);

        //textures
        auto getTextureName = [&](std::uint32_t id)
        {
            const auto& t = m_materialTextures.at(id);
            return t.relPath + t.name;
        };

        if (matDef.textureIDs[MaterialDefinition::Diffuse] != 0)
        {
            file.addProperty("diffuse", getTextureName(matDef.textureIDs[MaterialDefinition::Diffuse]));
        }

        if (matDef.textureIDs[MaterialDefinition::Mask] != 0)
        {
            file.addProperty("mask", getTextureName(matDef.textureIDs[MaterialDefinition::Mask]));
        }

        if (matDef.textureIDs[MaterialDefinition::Normal] != 0)
        {
            file.addProperty("normal", getTextureName(matDef.textureIDs[MaterialDefinition::Normal]));
        }

        if (matDef.textureIDs[MaterialDefinition::Lightmap] != 0)
        {
            file.addProperty("lightmap", getTextureName(matDef.textureIDs[MaterialDefinition::Lightmap]));
        }

        file.save(path);
    }
}

void ModelState::importMaterial(const std::string& path)
{
    cro::ConfigFile file;
    auto extension = cro::FileSystem::getFileExtension(path);
    if (extension == ".mdf")
    {
        if (file.loadFromFile(path))
        {
            auto name = file.getId();
            std::replace(name.begin(), name.end(), '_', ' ');

            MaterialDefinition def;
            def.materialData = m_resources.materials.get(materialIDs[MaterialID::Default]);

            if (!name.empty())
            {
                def.name = name;
            }

            const auto& properties = file.getProperties();
            for (const auto& prop : properties)
            {
                name = prop.getName();
                if (name == "type")
                {
                    auto val = prop.getValue<std::int32_t>();
                    if (val > -1 && val < MaterialDefinition::Count)
                    {
                        def.type = static_cast<MaterialDefinition::Type>(val);
                    }
                }
                else if (name == "colour")
                {
                    def.colour = prop.getValue<cro::Colour>();
                }
                else if (name == "mask_colour")
                {
                    def.maskColour = prop.getValue<cro::Colour>();
                }
                else if (name == "alpha_clip")
                {
                    def.alphaClip = std::max(0.f, std::min(1.f, prop.getValue<float>()));
                }
                else if (name == "vertex_coloured")
                {
                    def.vertexColoured = prop.getValue<bool>();
                }
                else if (name == "rx_shadow")
                {
                    def.receiveShadows = prop.getValue<bool>();
                }
                else if (name == "blend_mode")
                {
                    auto val = prop.getValue<std::int32_t>();
                    if (val > -1 && val <= static_cast<std::int32_t>(cro::Material::BlendMode::Additive))
                    {
                        def.blendMode = static_cast<cro::Material::BlendMode>(val);
                    }
                }
                else if (name == "diffuse")
                {
                    def.textureIDs[MaterialDefinition::Diffuse] = addTextureToBrowser(m_preferences.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Diffuse] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "mask")
                {
                    def.textureIDs[MaterialDefinition::Mask] = addTextureToBrowser(m_preferences.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Mask] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "lightmap")
                {
                    def.textureIDs[MaterialDefinition::Lightmap] = addTextureToBrowser(m_preferences.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Lightmap] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "normal")
                {
                    def.textureIDs[MaterialDefinition::Normal] = addTextureToBrowser(m_preferences.workingDirectory + "/" + prop.getValue<std::string>());
                    if (def.textureIDs[MaterialDefinition::Normal] == 0)
                    {
                        cro::FileSystem::showMessageBox("Error", "Failed opening texture. Check the working directory is set (View->Options)");
                    }
                }
                else if (name == "use_rimlight")
                {
                    def.useRimlighing = prop.getValue<bool>();
                }
                else if (name == "rimlight_colour")
                {
                    def.rimlightColour = prop.getValue<cro::Colour>();
                }
                else if (name == "rimlight_falloff")
                {
                    def.rimlightFalloff = std::min(1.f, std::max(0.f, prop.getValue<float>()));
                }
            }

            addMaterialToBrowser(std::move(def));
        }
    }
    else if (extension == ".cmt")
    {
        if (file.loadFromFile(path))
        {
            //this is a regular model def so try parsing the materials
            const auto& objects = file.getObjects();
            for (const auto& obj : objects)
            {
                if (obj.getName() == "material")
                {
                    MaterialDefinition matDef;
                    readMaterialDefinition(matDef, obj);

                    addMaterialToBrowser(std::move(matDef));
                }
            }
        }
    }
    else
    {
        cro::FileSystem::showMessageBox("Error", "Could not open material file.");
    }
}

void ModelState::readMaterialDefinition(MaterialDefinition& matDef, const cro::ConfigObject& obj)
{
    std::string_view id = obj.getId();
    if (id == "Unlit")
    {
        matDef.type = MaterialDefinition::Unlit;
    }
    else if (id == "VertexLit")
    {
        matDef.type = MaterialDefinition::VertexLit;
    }
    //TODO PBR shader

    //read textures
    const auto& properties = obj.getProperties();
    for (const auto& prop : properties)
    {
        std::string_view name = prop.getName();
        if (name == "diffuse"
            || name == "mask"
            || name == "normal"
            || name == "lightmap")
        {
            auto path = m_preferences.workingDirectory + "/" + prop.getValue<std::string>();

            if (name == "diffuse") matDef.textureIDs[MaterialDefinition::Diffuse] = addTextureToBrowser(path);
            if (name == "mask") matDef.textureIDs[MaterialDefinition::Mask] = addTextureToBrowser(path);
            if (name == "normal") matDef.textureIDs[MaterialDefinition::Normal] = addTextureToBrowser(path);
            if (name == "lightmap") matDef.textureIDs[MaterialDefinition::Lightmap] = addTextureToBrowser(path);
        }
        else if (name == "colour")
        {
            matDef.colour = prop.getValue<cro::Colour>();
        }
        else if (name == "vertex_coloured")
        {
            matDef.vertexColoured = prop.getValue<bool>();
        }
        else if (name == "rim")
        {
            matDef.useRimlighing = true;
            matDef.rimlightColour = prop.getValue<cro::Colour>();
        }
        else if (name == "rim_falloff")
        {
            matDef.useRimlighing = true;
            matDef.rimlightFalloff = prop.getValue<float>();
        }
        else if (name == "rx_shadows")
        {
            matDef.receiveShadows = prop.getValue<bool>();
        }
        else if (name == "blendmode")
        {
            auto mode = prop.getValue<std::string>();
            if (mode == "alpha")
            {
                matDef.blendMode = cro::Material::BlendMode::Alpha;
            }
            else if (mode == "add")
            {
                matDef.blendMode = cro::Material::BlendMode::Additive;
            }
            else if (mode == "multiply")
            {
                matDef.blendMode = cro::Material::BlendMode::Multiply;
            }
        }
        else if (name == "alpha_clip")
        {
            matDef.alphaClip = prop.getValue<float>();
        }
        else if (name == "mask_colour")
        {
            matDef.maskColour = prop.getValue<cro::Colour>();
        }
        else if (name == "name")
        {
            auto val = prop.getValue<std::string>();
            if (!val.empty())
            {
                matDef.name = val;
            }
        }
    }
}