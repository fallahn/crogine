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

#include "../golf/SharedStateData.hpp"
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
#include <crogine/util/Wavetable.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/gui/Gui.hpp>

namespace
{
    const cro::Time DropTime = cro::seconds(0.5f);
    constexpr float MinMultiplier = 0.2f;

    struct ExpandAnim final
    {
        explicit ExpandAnim(float r) : rad(r) {}

        float ct = 0.f;
        const float rad = 1.f;

        void operator()(cro::Entity e, float dt)
        {
            ct += dt * (1.f / DropTime.asSeconds());

            e.getComponent<cro::Transform>().setScale(glm::vec3(rad * cro::Util::Easing::easeOutElastic(std::min(ct, 1.f))));
            if (ct > 1)
            {
                ct = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        }
    };
}

SBallGameState::SBallGameState(cro::StateStack& stack, cro::State::Context ctx, SharedStateData& ss, SharedMinigameData& sd)
    : cro::State        (stack, ctx),
    m_sharedData        (ss),
    m_sharedGameData    (sd),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_currentID         (cro::Util::Random::value(0, 3)),
    m_nextID            (cro::Util::Random::value(0, 3)),
    m_inputFlags        (0),
    m_inputMultiplier   (MinMultiplier)
{
    addSystems();
    loadAssets();
    buildScene();
    buildUI();
}

//public
bool SBallGameState::handleEvent(const cro::Event& evt)
{
    switch (evt.type)
    {
    default: break;
    case SDL_KEYUP:
        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
        {
            m_inputFlags &= ~InputFlag::Left;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
        {
            m_inputFlags &= ~InputFlag::Right;
        }
        break;
    case SDL_KEYDOWN:
        if (evt.key.repeat == 0)
        {
            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
            {
                dropBall();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
            {
                m_inputFlags |= InputFlag::Left;
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
            {
                m_inputFlags |= InputFlag::Right;
            }
        }

        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            //TODO push pause state
            break;
        }
        break;
    case SDL_CONTROLLERBUTTONUP:
        if (cro::GameController::controllerID(evt.cbutton.which) == 0)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::DPadLeft:
                m_inputFlags &= ~InputFlag::Left;
                break;
            case cro::GameController::DPadRight:
                m_inputFlags &= ~InputFlag::Right;
                break;
            }
        }
        break;
    case SDL_CONTROLLERBUTTONDOWN:
        if (cro::GameController::controllerID(evt.cbutton.which) == 0)
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonA:
                dropBall();
                break;
            case cro::GameController::ButtonStart:
                //TODO puah pause state
                break;
            case cro::GameController::DPadLeft:
                m_inputFlags |= InputFlag::Left;
                break;
            case cro::GameController::DPadRight:
                m_inputFlags |= InputFlag::Right;
                break;
            }
        }
        break;
    case SDL_MOUSEBUTTONDOWN:
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            dropBall();
        }
        break;
    case SDL_CONTROLLERAXISMOTION:
        if (cro::GameController::controllerID(evt.cbutton.which) == 0)
        {
            if (evt.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX
                /*|| evt.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX*/) //hmm these will overwrite each other if we try reading both axes
            {
                if (evt.caxis.value < -cro::GameController::LeftThumbDeadZoneV)
                {
                    m_inputFlags |= InputFlag::Left;
                    m_inputFlags &= ~InputFlag::Right;
                }
                else if (evt.caxis.value > cro::GameController::LeftThumbDeadZoneV)
                {
                    m_inputFlags |= InputFlag::Right;
                    m_inputFlags &= ~InputFlag::Left;
                }
                else
                {
                    m_inputFlags &= ~(InputFlag::Left | InputFlag::Right);
                }
            }
        }
        break;
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
    else if (msg.id == sb::MessageID::GameMessage)
    {
        const auto& data = msg.getData<sb::GameEvent>();
        switch (data.type)
        {
        default: break;
        case sb::GameEvent::OutOfBounds:
            endGame();
            break;
        }
    }
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool SBallGameState::simulate(float dt)
{
    if (m_inputFlags)
    {
        m_inputMultiplier = std::min(1.f, m_inputMultiplier + (dt * 5.f));

        auto pos = m_cursor.getComponent<cro::Transform>().getPosition();
        glm::vec3 move(0.f);
        if (m_inputFlags & InputFlag::Left)
        {
            move.x -= 1.f;
        }

        if (m_inputFlags & InputFlag::Right)
        {
            move.x += 1.f;
        }
        pos += move * m_inputMultiplier * dt;

        const float padding = m_previewModels[m_currentID].getComponent<cro::Transform>().getScale().x + 0.01f;
        const float maxBounds = (BoxWidth / 2.f) - padding;
        pos.x = std::clamp(pos.x, -maxBounds, maxBounds);

        m_cursor.getComponent<cro::Transform>().setPosition(pos);
    }
    else
    {
        m_inputMultiplier = MinMultiplier;
    }


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
    m_environmentMap.loadFromFile("assets/images/indoor.hdr");
    
    std::vector<cro::Entity> wheelModels;

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
                    //TODO we only need to preview the first 4 models
                    auto e = m_previewModels.emplace_back(m_gameScene.createEntity());
                    e.addComponent<cro::Transform>().setScale(glm::vec3(info.radius));
                    info.modelDef->createModel(e);
                    e.getComponent<cro::Model>().setHidden(true);

                    //expand animation when showing preview
                    e.addComponent<cro::Callback>().function = ExpandAnim(info.radius);


                    e = m_nextModels.emplace_back(m_gameScene.createEntity());
                    e.addComponent<cro::Transform>().setScale(glm::vec3(info.radius));
                    info.modelDef->createModel(e);
                    e.getComponent<cro::Model>().setHidden(true);
                    e.addComponent<cro::Callback>().function = ExpandAnim(info.radius);


                    //for wheel of evolution - see below
                    e = wheelModels.emplace_back(m_gameScene.createEntity());
                    e.addComponent<cro::Transform>();
                    info.modelDef->createModel(e);

                    m_gameScene.getSystem<SBallPhysicsSystem>()->addBallData(std::move(info));
                }
            }
        }
    }
    else
    {
        LogI << "[SportsBall] Failed opening Data File" << std::endl;
    }


    //create the 'wheel of evolution'
    if(!wheelModels.empty())
    m_wheelEnt = m_gameScene.createEntity();
    m_wheelEnt.addComponent<cro::Transform>().setPosition({ 0.6f, 0.2f, 0.2f });

    static constexpr float Rad = 0.13f;
    const float Arc = cro::Util::Const::TAU / wheelModels.size();

    static constexpr float BaseScale = 0.01f;

    for (auto i = 0u; i < wheelModels.size(); ++i)
    {
        glm::vec3 p(std::cos(Arc * i), std::sin(Arc * i), 0.f);
        wheelModels[i].getComponent<cro::Transform>().setPosition(p * Rad);
        wheelModels[i].getComponent<cro::Transform>().setScale(glm::vec3(BaseScale + (i * 0.0045f)));

        m_wheelEnt.getComponent<cro::Transform>().addChild(wheelModels[i].getComponent<cro::Transform>());
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
    //box
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    md.loadFromFile("assets/arcade/sportsball/models/box.cmt");

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    md.createModel(entity);


    //preview/cursor
    m_cursor = m_gameScene.createEntity();
    m_cursor.addComponent<cro::Transform>().setPosition({ 0.f, CursorHeight, 0.f });

    for (auto e : m_previewModels)
    {
        m_cursor.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    }
    m_previewModels[m_currentID].getComponent<cro::Model>().setHidden(false);
    m_nextModels[m_nextID].getComponent<cro::Model>().setHidden(false);



    auto resize = [&](cro::Camera& cam)
        {
            //TODO update shadow quality from settings

            const glm::vec2 size(cro::App::getWindow().getSize());
            const float ratio = size.x / size.y;
            const float y = 1.2f;
            const float x = y * ratio;

            cam.setOrthographic(-x / 2.f, x / 2.f, 0.f, y, 0.1f, 4.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            const float wheelPos = (((x - BoxWidth) / 2.f) / 2.f) + (BoxWidth / 2.f);
            m_wheelEnt.getComponent<cro::Transform>().setPosition({ wheelPos, 0.2f, 0.f });

            for (auto e : m_nextModels)
            {
                e.getComponent<cro::Transform>().setPosition({ -wheelPos, CursorHeight, 0.f });
            }
        };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, -0.05f, 1.4f });

    //callback for camera shake
    camEnt.addComponent<cro::Callback>().setUserData<float>(1.f);
    camEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            static const auto wt = cro::Util::Wavetable::noise(1.f);
            static std::size_t idx = 0;

            idx = (idx + 1) % wt.size();

            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct -= (dt* 2.f);

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.x = (wt[idx] * ct) * 0.01f;

            if (ct < 0.f)
            {
                ct = 1.f;
                e.getComponent<cro::Callback>().active = false;
                pos.x = 0.f;
            }

            e.getComponent<cro::Transform>().setPosition(pos);
        };


    auto& cam = camEnt.getComponent<cro::Camera>();
    resize(cam);
    cam.resizeCallback = resize;
    cam.shadowMapBuffer.create(2048, 2048);
    cam.setMaxShadowDistance(10.f);
    cam.setBlurPassCount(1); //TODO read shadow quality
    cam.setShadowExpansion(15.f);


    auto lightEnt = m_gameScene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -45.f * cro::Util::Const::degToRad);
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

void SBallGameState::dropBall()
{
    //check timer to prevent spamming
    if (m_dropClock.elapsed() > DropTime)
    {
        m_dropClock.restart();

        m_gameScene.getSystem<SBallPhysicsSystem>()->spawnBall(m_currentID, m_cursor.getComponent<cro::Transform>().getPosition());

        //update next/current indices
        m_previewModels[m_currentID].getComponent<cro::Model>().setHidden(true);
        m_previewModels[m_currentID].getComponent<cro::Transform>().setScale(glm::vec3(0.f));

        m_nextModels[m_nextID].getComponent<cro::Model>().setHidden(true);

        m_currentID = m_nextID;
        m_nextID = cro::Util::Random::value(0, 3);

        //update preview models with animation

        //use an ent to delay this a bit
        auto ent = m_gameScene.createEntity();
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().setUserData<float>(DropTime.asSeconds());
        ent.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
                ct -= dt;

                if (ct < 0.f)
                {
                    m_previewModels[m_currentID].getComponent<cro::Model>().setHidden(false);
                    m_previewModels[m_currentID].getComponent<cro::Callback>().active = true;

                    e.getComponent<cro::Callback>().active = false;
                    m_gameScene.destroyEntity(e);
                }
            };
        m_nextModels[m_nextID].getComponent<cro::Model>().setHidden(false);
        m_nextModels[m_nextID].getComponent<cro::Callback>().active = true;
    }
}

void SBallGameState::endGame()
{
    m_gameScene.setSystemActive<SBallPhysicsSystem>(false);

    //camera shake
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().setUserData<float>(1.f);
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().active = true;

    //TODO show score summary / post scores to leaderboard
}

void SBallGameState::onCachedPush()
{
    //reset the game
    auto pos = m_cursor.getComponent<cro::Transform>().getPosition();
    pos.x = 0.f;
    m_cursor.getComponent<cro::Transform>().setPosition(pos);


    m_gameScene.setSystemActive<SBallPhysicsSystem>(true);
    m_gameScene.getSystem<SBallPhysicsSystem>()->clearBalls();
}

void SBallGameState::onCachedPop()
{
    //reset any end-game overlay
}