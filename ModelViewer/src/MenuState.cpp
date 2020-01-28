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

#include "MenuState.hpp"

#include <crogine/core/App.hpp>
#include <crogine/core/FileSystem.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/gui/Gui.hpp>
#include <crogine/gui/imgui.h>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Skeleton.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/ShadowCaster.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>

#include <crogine/ecs/systems/SkeletalAnimator.hpp>
#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

namespace
{
    const float DefaultFOV = 35.f * cro::Util::Const::degToRad;
    const float DefaultFarPlane = 50.f;
    const float MinZoom = 0.1f;
    const float MaxZoom = 2.5f;

    void updateView(cro::Entity entity, float farPlane, float fov)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        size.y = ((size.x / 16.f) * 9.f) / size.y;
        size.x = 1.f;

        auto& cam3D = entity.getComponent<cro::Camera>();
        cam3D.projectionMatrix = glm::perspective(fov, 16.f / 9.f, 0.1f, farPlane);
        cam3D.viewport.bottom = (1.f - size.y) / 2.f;
        cam3D.viewport.height = size.y;
    }


    const std::string prefPath = cro::FileSystem::getConfigDirectory("cro_model_viewer") + "prefs.cfg";

    //tooltip for UI
    static void HelpMarker(const char* desc)
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

    const std::array<float, 6> worldScales = { 0.01f, 0.1f, 1.f, 10.f, 100.f, 1000.f };
    const glm::vec3 DefaultCameraPosition({ 0.f, 1.f, 5.f });
}

MenuState::MenuState(cro::StateStack& stack, cro::State::Context context)
	: cro::State        (stack, context),
    m_scene             (context.appInstance.getMessageBus()),
    m_zoom              (1.f),
    m_showPreferences   (false),
    m_showGroundPlane   (true)
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
}

//public
bool MenuState::handleEvent(const cro::Event& evt)
{
    if(cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    switch (evt.type)
    {
    default: break;
    case SDL_MOUSEMOTION:
        if (evt.motion.state & SDL_BUTTON_LMASK)
        {
            auto& tx = m_camController.getComponent<cro::Transform>();
            tx.rotate(cro::Transform::Y_AXIS, static_cast<float>(-evt.motion.xrel / 2) * cro::Util::Const::degToRad);
            tx.rotate(cro::Transform::X_AXIS, static_cast<float>(-evt.motion.yrel / 2) * cro::Util::Const::degToRad);
        }
        else if (evt.motion.state & SDL_BUTTON_MMASK)
        {
            auto& tx = m_camController.getComponent<cro::Transform>();
            tx.move((glm::vec3(-evt.motion.xrel, evt.motion.yrel, 0.f ) / 60.f) * worldScales[m_preferences.unitsPerMetre]);
        }
        break;
    case SDL_MOUSEWHEEL:
        m_zoom = cro::Util::Maths::clamp(m_zoom - (0.1f * evt.wheel.y), MinZoom, MaxZoom);
        updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], m_zoom * DefaultFOV);
        break;
    case SDL_WINDOWEVENT_RESIZED:
        updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], m_zoom* DefaultFOV);
        break;
    }

    m_scene.forwardEvent(evt);
	return true;
}

void MenuState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MenuState::simulate(cro::Time dt)
{
    m_scene.simulate(dt);
	return true;
}

void MenuState::render()
{
	//draw any renderable systems
    m_scene.render();
}

//private
void MenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SkeletalAnimator>(mb);
    m_scene.addSystem<cro::SceneGraph>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::ShadowMapRenderer>(mb);
    m_scene.addSystem<cro::ModelRenderer>(mb);
}

void MenuState::loadAssets()
{

}

void MenuState::createScene()
{
    //create ground plane
    cro::ModelDefinition modelDef;
    modelDef.loadFromFile("assets/models/ground_plane.cmt", m_resources);

    m_groundPlane = m_scene.createEntity();
    m_groundPlane.addComponent<cro::Transform>().setRotation({ -90.f * cro::Util::Const::degToRad, 0.f, 0.f });
    modelDef.createModel(m_groundPlane, m_resources);

    //create the camera - using a custom camera prevents the scene updating the projection on window resize
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(DefaultCameraPosition);
    entity.addComponent<cro::Camera>();
    updateView(entity, DefaultFarPlane, DefaultFOV);
    m_scene.setActiveCamera(entity);

    m_camController = m_scene.createEntity();
    m_camController.addComponent<cro::Transform>().setRelativeToCamera(true);
    entity.getComponent<cro::Transform>().setParent(m_camController);

    //set the default sunlight properties
    m_scene.getSystem<cro::ShadowMapRenderer>().setProjectionOffset({ 0.f, 6.f, -5.f });
    m_scene.getSunlight().setDirection({ -0.f, -1.f, -0.f });
    m_scene.getSunlight().setProjectionMatrix(glm::ortho(-5.6f, 5.6f, -5.6f, 5.6f, 0.1f, 80.f));
}

void MenuState::buildUI()
{
    loadPrefs();
    registerWindow([&]()
        {
            if(ImGui::BeginMainMenuBar())
            {
                //file menu
                if (ImGui::BeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open Model", nullptr, nullptr))
                    {
                        openModel();
                    }
                    if (ImGui::MenuItem("Close Model", nullptr, nullptr))
                    {
                        closeModel();
                    }

                    ImGui::MenuItem("Import Model", nullptr, nullptr);
                    ImGui::MenuItem("Export Model", nullptr, nullptr);
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
                    ImGui::MenuItem("Animation Data", nullptr, nullptr);
                    ImGui::MenuItem("Material Data", nullptr, nullptr);
                    if (ImGui::MenuItem("Ground Plane", nullptr, &m_showGroundPlane))
                    {
                        if (m_showGroundPlane)
                        {
                            //set this to whichever world scale we're currently using
                            updateWorldScale();
                        }
                        else
                        {
                            m_groundPlane.getComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::EndMainMenuBar();
            }

            //options window
            if (m_showPreferences)
            {
                ImGui::SetNextWindowSize({ 400.f, 160.f });
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
                        HelpMarker(m_preferences.workingDirectory.c_str());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Browse"))
                    {
                        auto path = cro::FileSystem::openFolderDialogue();
                        if (!path.empty())
                        {
                            m_preferences.workingDirectory = path;
                        }
                    }

                    ImGui::PushItemWidth(100.f);
                    //world scale selection
                    const char* items[] = { "0.01", "0.1", "1", "10", "100", "1000" };
                    static const char* currentItem = items[m_preferences.unitsPerMetre];
                    if (ImGui::BeginCombo("World Scale", currentItem))
                    {
                        for (auto i = 0u; i < worldScales.size(); ++i)
                        {
                            bool selected = (currentItem == items[i]);
                            if (ImGui::Selectable(items[i], selected))
                            {
                                currentItem = items[i];
                                m_preferences.unitsPerMetre = i;
                                updateWorldScale();
                            }

                            if (selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ImGui::Separator();

                    if (!m_showPreferences ||
                        ImGui::Button("Close"))
                    {
                        savePrefs();
                        m_showPreferences = false;
                    }
                }
                ImGui::End();
            }

            //model detail window
            if (m_activeModel.isValid())
            {
                ImGui::SetNextWindowSize({ 250.f, 430.f });
                if (ImGui::Begin("Model Properties"))
                {
                    std::string worldScale("World Scale:\n");
                    worldScale += std::to_string(worldScales[m_preferences.unitsPerMetre]);
                    worldScale += " units per metre";
                    ImGui::Text("%s", worldScale.c_str());
                }
                ImGui::End();
            }
        });
}

void MenuState::openModel()
{
    closeModel();

    auto path = cro::FileSystem::openFileDialogue("", "cmt");
    if (!path.empty()
        && cro::FileSystem::getFileExtension(path) == ".cmt")
    {
        m_activeModel = m_scene.createEntity();
        m_activeModel.addComponent<cro::Transform>();

        cro::ModelDefinition def(m_preferences.workingDirectory);
        if (def.loadFromFile(path, m_resources))
        {
            def.createModel(m_activeModel, m_resources);

            if (m_activeModel.getComponent<cro::Model>().getMeshData().boundingSphere.radius > 2.f)
            {
                cro::Logger::log("Bounding sphere radius is very large - model may not be visible", cro::Logger::Type::Warning);
            }
        }
        else
        {
            cro::Logger::log("Check current working directory (Options)?", cro::Logger::Type::Error);
            closeModel();
        }
    }
    else
    {
        cro::Logger::log(path + ": invalid file path", cro::Logger::Type::Error);
    }
}

void MenuState::closeModel()
{
    if (m_activeModel.isValid())
    {
        m_scene.destroyEntity(m_activeModel);
    }

    //TODO we might want to remove from any resource manager
    //too else eventually we'll end up with a lot of unused
    //resources if closing  and opening a lot of files.
}

void MenuState::loadPrefs()
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
            }
            else if (name == "units_per_metre")
            {
                m_preferences.unitsPerMetre = cro::Util::Maths::clamp(static_cast<std::size_t>(prop.getValue<std::int32_t>()), std::size_t(0u), worldScales.size());
            }
        }

        updateWorldScale();
    }
}

void MenuState::savePrefs()
{
    cro::ConfigFile prefsOut;
    prefsOut.addProperty("working_dir", m_preferences.workingDirectory);
    prefsOut.addProperty("units_per_metre", std::to_string(m_preferences.unitsPerMetre));

    prefsOut.save(prefPath);
}

void MenuState::updateWorldScale()
{
    const float scale = worldScales[m_preferences.unitsPerMetre];
    m_groundPlane.getComponent<cro::Transform>().setScale({ scale, scale, scale });
    m_scene.getActiveCamera().getComponent<cro::Transform>().setPosition(DefaultCameraPosition * scale);
    
    m_camController.getComponent<cro::Transform>().setPosition(glm::vec3(0.f));
    updateView(m_scene.getActiveCamera(), DefaultFarPlane * worldScales[m_preferences.unitsPerMetre], m_zoom * DefaultFOV);
}