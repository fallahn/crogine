/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "GolfState.hpp"
#include "GameConsts.hpp"

#include <crogine/core/ConfigFile.hpp>

#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Constants.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    //not exactly EGA but close enough
    constexpr glm::vec2 ViewportSize(300.f, 225.f);
}

GolfState::GolfState(cro::StateStack& stack, cro::State::Context context)
    : cro::State(stack, context),
    m_gameScene (context.appInstance.getMessageBus()),
    m_uiScene   (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        loadAssets();
        addSystems();
        buildScene();
        });

#ifdef CRO_DEBUG_
    //registerWindow([]() 
    //    {
    //        if (ImGui::Begin("buns"))
    //        {

    //        }
    //        ImGui::End();
    //    });
#endif
}

//public
bool GolfState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse)
    {
        return true;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            requestStackClear();
            requestStackPush(States::MainMenu);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);

    return true;
}

void GolfState::handleMessage(const cro::Message& msg)
{

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool GolfState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);

    return true;
}

void GolfState::render()
{
    m_renderTexture.clear();
    m_gameScene.render(m_renderTexture);
    m_renderTexture.display();

    m_uiScene.render(cro::App::getWindow());
}

//private
void GolfState::loadAssets()
{
    m_gameScene.setCubemap("assets/images/skybox/sky.ccm");

    m_renderTexture.create(static_cast<std::uint32_t>(ViewportSize.x) , static_cast<std::uint32_t>(ViewportSize.y)); //this is EGA res
}

void GolfState::addSystems()
{
    auto& mb = m_gameScene.getMessageBus();

    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void GolfState::buildScene()
{
    cro::ConfigFile mapData;
    if (!mapData.loadFromFile("assets/golf/holes/01.hole"))
    {
        //TODO better error handling of missing files
        return;
    }

    static constexpr std::int32_t MaxProps = 4;
    std::int32_t propCount = 0;

    cro::ModelDefinition modelDef(m_resources);
    const auto& props = mapData.getProperties();
    for (const auto& prop : props)
    {
        const auto& name = prop.getName();
        if (name == "map")
        {
            if (m_holeData.map.loadFromFile(prop.getValue<std::string>()))
            {
                propCount++;
            }
        }
        else if (name == "model")
        {
            if (modelDef.loadFromFile(prop.getValue<std::string>()))
            {
                propCount++;
            }
        }
        else if (name == "pin")
        {
            //TODO how do we make certain this is a reasonable value?
            auto pin = prop.getValue<glm::vec2>();
            m_holeData.pin = { pin.x, 0.f, -pin.y };
            propCount++;
        }
        else if (name == "tee")
        {
            auto tee = prop.getValue<glm::vec2>();
            m_holeData.tee = { tee.x, 0.f, -tee.y };
            propCount++;
        }
    }

    if (propCount != MaxProps)
    {
        LogE << "Missing properties from hole file" << std::endl;
        return;
    }

    //model file is centred so needs offset correction
    glm::vec2 courseSize(m_holeData.map.getSize());

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin(glm::vec3(-courseSize.x / 2.f, 0.f, courseSize.y / 2.f));
    modelDef.createModel(entity);

    auto updateView = [](cro::Camera& cam)
    {
        cam.setPerspective(60.f * cro::Util::Const::degToRad, ViewportSize.x / ViewportSize.y, 0.1f, ViewportSize.x);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
    };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ m_holeData.tee.x, CameraHeight, m_holeData.tee.z });
    auto lookat = glm::lookAt(camEnt.getComponent<cro::Transform>().getPosition(), m_holeData.pin, cro::Transform::Y_AXIS);
    camEnt.getComponent<cro::Transform>().setRotation(glm::inverse(lookat));

    auto offset = -camEnt.getComponent<cro::Transform>().getForwardVector();
    camEnt.getComponent<cro::Transform>().move(offset * CameraOffset);

    auto& cam = camEnt.getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);

    buildUI(); //put this here because we don't want to do this if the map data didn't load
}

void GolfState::buildUI()
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_renderTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    auto courseEnt = entity;









    auto updateView = [courseEnt](cro::Camera& cam) mutable
    {
        auto size = glm::vec2(cro::App::getWindow().getSize());
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 1.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        //TODO call some sort of layout update
        float viewScale = std::floor(size.x / ViewportSize.x);
        courseEnt.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));
        courseEnt.getComponent<cro::Transform>().setPosition(size / 2.f);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = updateView;
    updateView(cam);
}