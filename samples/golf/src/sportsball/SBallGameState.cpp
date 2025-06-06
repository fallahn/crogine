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
#include "SBallPhysicsSystem.hpp"
#include "SBallConsts.hpp"

#include <crogine/core/ConfigFile.hpp>

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
#include <crogine/util/Random.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    std::int32_t nextID = 0;
}

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
    if ((evt.type == SDL_KEYDOWN
        && evt.key.keysym.sym == SDLK_SPACE)
        || evt.type == SDL_MOUSEBUTTONDOWN)
    {
        const float x = cro::Util::Random::value(-0.4f, 0.4f);
        m_gameScene.getSystem<SBallPhysicsSystem>()->spawnBall(nextID, {x, 1.f, 0.f});
        nextID = cro::Util::Random::value(0, 3);
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void SBallGameState::handleMessage(const cro::Message& msg)
{
    if (msg.id == sb::MessageID::CollisionMessage)
    {
        const auto& data = msg.getData<sb::CollisionEvent>();
        if (data.entityA.isValid()
            && data.entityB.isValid())
        {
            auto a = data.entityA;
            a.getComponent<SBallPhysics>().collisionHandled = true;
            m_gameScene.destroyEntity(data.entityA);

            auto b = data.entityB;
            b.getComponent<SBallPhysics>().collisionHandled = true;
            m_gameScene.destroyEntity(data.entityB);

            if (data.ballID < BallID::Count - 1)
            {
                m_gameScene.getSystem<SBallPhysicsSystem>()->spawnBall(data.ballID + 1, data.position);
            }
        }
    }
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

#ifdef CRO_DEBUG_
    //auto cam = m_gameScene.getActiveCamera();
    //m_gameScene.getSystem<SBallPhysicsSystem>()->renderDebug(
    //    cam.getComponent<cro::Camera>().getActivePass().viewProjectionMatrix,
    //    cro::App::getWindow().getSize());
#endif
}

//private
void SBallGameState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
    //float previewX = -1.f;
    cro::ConfigFile cfg;
    if (cfg.loadFromFile("assets/arcade/sportsball/data/balls.dat"))
    {
        const auto& objs = cfg.getObjects();
        for (const auto& obj : objs)
        {
            if (obj.getName() == "ball")
            {
                BallData info;

                const auto& props = obj.getProperties();
                for (const auto& prop : props)
                {
                    const auto& name = prop.getName();
                    if (name == "mass")
                    {
                        info.mass = prop.getValue<float>();
                    }
                    else if (name == "restitution")
                    {
                        info.restititution = prop.getValue<float>();
                    }
                    else if (name == "radius")
                    {
                        info.radius = prop.getValue<float>();
                    }
                    else if (name == "model")
                    {
                        const auto path = "assets/arcade/sportsball/models/" + prop.getValue<std::string>();
                        info.modelDef = std::make_unique<cro::ModelDefinition>(m_resources, &m_environmentMap);
                        if (!info.modelDef->loadFromFile(path))
                        {
                            LogW << "Failed opening model " << prop.getValue<std::string>() << std::endl;
                        }
                    }
                }

                //hmm this will mis-align the indices
                if (info.modelDef->isLoaded())
                {
                    auto e = m_previewModels.emplace_back(m_gameScene.createEntity());
                    e.addComponent<cro::Transform>().setScale(glm::vec3(info.radius));
                    info.modelDef->createModel(e);
                    /*previewX += (2.f / 9.f);*/
                    e.getComponent<cro::Model>().setHidden(true);

                    m_gameScene.getSystem<SBallPhysicsSystem>()->addBallData(std::move(info));
                }
            }
        }
    }
    else
    {
        LogI << "[SportsBall] Failed opening Data File" << std::endl;
    }
}

void SBallGameState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<SBallPhysicsSystem>(mb);
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
    cam.setMaxShadowDistance(10.f);
    cam.setBlurPassCount(1); //TODO read shadow quality
    cam.setShadowExpansion(15.f);


    auto lightEnt = m_gameScene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -65.f * cro::Util::Const::degToRad);
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -1.f * cro::Util::Const::degToRad);
}

void SBallGameState::buildUI()
{
    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("erg");
    //        int i = 0;
    //        for (auto e : m_previewModels)
    //        {
    //            float s = e.getComponent<cro::Transform>().getScale().x;
    //            const std::string label = "Scale##" + std::to_string(i);
    //            if (ImGui::SliderFloat(label.c_str(), &s, 0.1f, 3.f))
    //            {
    //                e.getComponent<cro::Transform>().setScale(glm::vec3(s));
    //            }

    //            i++;
    //        }
    //        ImGui::End();
    //    });


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