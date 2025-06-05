/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "../scrub/ScrubSharedData.hpp"
#include "SBallGameState.hpp"

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIElement.hpp>

#include <crogine/ecs/systems/ShadowMapRenderer.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Font.hpp>
#include <crogine/util/Constants.hpp>

SBallGameState::SBallGameState(cro::StateStack& stack, cro::State::Context ctx, SharedMinigameData& sd)
    : cro::State        (stack, ctx),
    m_sharedGameData    (sd),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_uiScene           (ctx.appInstance.getMessageBus())
{
    addSystems();
    loadAssets();
    buildScene();
    buildUI();
}

//public
bool SBallGameState::handleEvent(const cro::Event& evt)
{

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void SBallGameState::handleMessage(const cro::Message& msg)
{

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SBallGameState::simulate(float dt)
{

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void SBallGameState::render() 
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void SBallGameState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
}

void SBallGameState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::UIElementSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void SBallGameState::buildScene()
{
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/arcade/sportsball/models/box.cmt");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);


    auto resize = [&](cro::Camera& cam)
        {
            //TODO update shadow quality from settings

            cam.setPerspective(60.f * cro::Util::Const::degToRad, 4.f / 3.f, 0.1f, 4.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, 0.5f, 1.4f });

    auto& cam = camEnt.getComponent<cro::Camera>();
    resize(cam);
    cam.resizeCallback = resize;
    cam.shadowMapBuffer.create(1024, 1024);
    cam.setMaxShadowDistance(3.f);
    cam.setBlurPassCount(1); //TODO read shadow quality
    cam.setShadowExpansion(0.5f);


    auto lightEnt = m_gameScene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, 10.f * cro::Util::Const::degToRad);
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -10.f * cro::Util::Const::degToRad);
}

void SBallGameState::buildUI()
{

    auto resize = [](cro::Camera& cam)
        {
            cam.setOrthographic(0.f, 10.f, 0.f, 10.f, -10.f, 10.f);
            LogI << "Implement Me" << std::endl;
        };
    resize(m_uiScene.getActiveCamera().getComponent<cro::Camera>());
}

void SBallGameState::onCachedPush()
{

}

void SBallGameState::onCachedPop()
{

}