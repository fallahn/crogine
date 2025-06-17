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

#include "../Colordome-32.hpp"
#include "../golf/GameConsts.hpp"
#include "../golf/SharedStateData.hpp"
#include "../golf/TextAnimCallback.hpp"
#include "../scrub/ScrubSharedData.hpp"
#include "../scrub/ScrubConsts.hpp"
#include "SBallGameState.hpp"
#include "SBallPhysicsSystem.hpp"
#include "SBallConsts.hpp"
#include "SBallParticleDirector.hpp"
#include "SBallSoundDirector.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

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
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/graphics/Font.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>
#include <crogine/util/Easings.hpp>

#include <crogine/gui/Gui.hpp>

#include <Social.hpp>

namespace
{
    cro::String InputKeyb(const InputBinding& ib)
    {
        cro::String str = cro::Keyboard::keyString(ib.keys[InputBinding::Left]) + "/" + cro::Keyboard::keyString(ib.keys[InputBinding::Right]) + ": Move     "
            + cro::Keyboard::keyString(ib.keys[InputBinding::Action]) + ": Drop     Escape: Pause/Quit";
#ifdef USE_GNS
        //const auto leader = Social::getSBallLeader();
        //str += "\nCurrent Leader: " + leader;
#endif

        return str;
    }
    const cro::String InputPS = cro::String(LeftStick) + " Move     " + cro::String(ButtonCross) + " Drop     " + cro::String(ButtonOption) + " Pause/Quit";
    const cro::String InputXBox = cro::String(LeftStick) + " Move     " + cro::String(ButtonA) + " Drop     " + cro::String(ButtonStart) + " Pause/Quit";

    std::int32_t lastInput = InputType::Keyboard;

    const cro::Time DropTime = cro::seconds(0.5f);
    const cro::Time RoundEndTime = cro::seconds(2.5f);
    constexpr float MinMultiplier = 0.2f;

    struct FloatingTextAnim final
    {
        FloatingTextAnim(cro::Scene& s, const float vs)
            : scene(s), viewScale(vs) { }

        cro::Scene& scene;
        const float viewScale = 1.f;
        float ct = 1.f;

        void operator()(cro::Entity e, float dt)
        {
            ct = std::max(0.f, ct - dt);

            auto c = TextNormalColour;
            c.setAlpha(ct);
            e.getComponent<cro::Text>().setFillColour(c);

            c = LeaderboardTextDark;
            c.setAlpha(ct);
            e.getComponent<cro::Text>().setShadowColour(c);

            const float speed = 10.f * viewScale;
            e.getComponent<cro::Transform>().move({ 0.f, speed * dt });

            if (ct == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                scene.destroyEntity(e);
            }
        }
    };

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
    m_gameScene         (ctx.appInstance.getMessageBus(), 1024),
    m_uiScene           (ctx.appInstance.getMessageBus(), 1280),
    m_currentID         (cro::Util::Random::value(0, 3)),
    m_nextID            (cro::Util::Random::value(0, 3)),
    m_inputFlags        (0),
    m_inputMultiplier   (MinMultiplier),
    m_gameEnded         (false)
{
    addSystems();
    loadAssets();
    buildScene();
    buildUI();

#ifdef CRO_DEBUG_
    //onCachedPush();
#endif

}

//public
bool SBallGameState::handleEvent(const cro::Event& evt)
{
    const auto pause =
        [&]()
        {
            requestStackPush(StateID::ScrubPause);
        };

    if (m_gameEnded)
    {
        const auto restart = 
            [&]()
            {
                onCachedPop();
                onCachedPush();
            };

        const auto quit = 
            [&]()
            {
                requestStackPop();
                requestStackPush(StateID::SBallAttract);
            };

        if (m_roundEndClock.elapsed() > RoundEndTime)
        {
            switch (evt.type)
            {
            default: break;
            case SDL_KEYUP:
                if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
                {
                    //restart the game
                    restart();
                }
                else if (evt.key.keysym.sym == SDLK_ESCAPE)
                {
                    quit();
                }
                break;
            case SDL_CONTROLLERBUTTONUP:
                if (cro::GameController::controllerID(evt.cbutton.which) == 0)
                {
                    if (evt.cbutton.button == cro::GameController::ButtonA)
                    {
                        restart();
                    }
                    else
                    {
                        quit();
                    }
                }
                break;
            }
        }
    }
    else
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
                pause();
                break;
#ifdef CRO_DEBUG_
            case SDLK_p:
                m_gameScene.getActiveCamera().getComponent<cro::Callback>().active = true;
                m_gameScene.getActiveCamera().getComponent<cro::Callback>().setUserData<float>(1.f);
                break;
            case SDLK_i:
                levelUp();
                break;
            case SDLK_o:
                endGame();
                break;
#endif
            }
            break;
        case SDL_CONTROLLERBUTTONUP:
            if (cro::GameController::controllerID(evt.cbutton.which) == 0)
            {
                //hmm this just gets overwritten by the left stick...
                //does it matter though?
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
                    pause();
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
                //TODO this should at least account for cursor pos
                //set he drop pos, and check if it's in bounds
                //dropBall();
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            if (cro::GameController::controllerID(evt.cbutton.which) == 0)
            {
                if (evt.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX
                    /*|| evt.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX*/) //hmm these will overwrite each other if we try reading both axes
                {
                    if (evt.caxis.value < -cro::GameController::LeftThumbDeadZoneV * 3)
                    {
                        m_inputFlags |= InputFlag::Left;
                        m_inputFlags &= ~InputFlag::Right;
                    }
                    else if (evt.caxis.value > cro::GameController::LeftThumbDeadZoneV * 3)
                    {
                        m_inputFlags |= InputFlag::Right;
                        m_inputFlags &= ~InputFlag::Left;
                    }
                    //this overrides DPad input...
                    else
                    {
                        m_inputFlags &= ~(InputFlag::Left | InputFlag::Right);
                    }
                }
            }
            break;
        }
    }

    //update interface regardless
    if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }
    else
    {
        switch (evt.type)
        {
        default:

            break;
        case SDL_CONTROLLERDEVICEREMOVED:
            pause();
            break;
        case SDL_CONTROLLERDEVICEADDED:
            for (auto i = 0; i < 4; ++i)
            {
                cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createWeapon(0, 1, 2));
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            cro::App::getWindow().setMouseCaptured(true);
            if (cro::GameController::hasPSLayout(cro::GameController::controllerID(evt.caxis.which)))
            {
                m_controlTextEntity.getComponent<cro::Text>().setString(InputPS);
                lastInput = InputType::PS;
            }
            else
            {
                m_controlTextEntity.getComponent<cro::Text>().setString(InputXBox);
                lastInput = InputType::XBox;
            }
            break;
        case SDL_CONTROLLERAXISMOTION:
            cro::App::getWindow().setMouseCaptured(true);

            if (evt.caxis.value < -cro::GameController::LeftThumbDeadZone || evt.caxis.value > cro::GameController::LeftThumbDeadZone)
            {
                if (cro::GameController::hasPSLayout(cro::GameController::controllerID(evt.caxis.which)))
                {
                    m_controlTextEntity.getComponent<cro::Text>().setString(InputPS);
                    lastInput = InputType::PS;
                }
                else
                {
                    m_controlTextEntity.getComponent<cro::Text>().setString(InputXBox);
                    lastInput = InputType::XBox;
                }
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            cro::App::getWindow().setMouseCaptured(true);
            m_controlTextEntity.getComponent<cro::Text>().setString(InputKeyb(m_sharedData.inputBinding));
            lastInput = InputType::Keyboard;
            break;
        }
    }


    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void SBallGameState::handleMessage(const cro::Message& msg)
{
    if (msg.id == sb::MessageID::CollisionMessage
        && !m_gameEnded) //stops the small chance there might be a collision / score update AFTER the game ends and prints the score
    {
        const auto& data = msg.getData<sb::CollisionEvent>();
        if (data.entityA.isValid()
            && data.entityB.isValid())
        {
            auto a = data.entityA;
            auto b = data.entityB;

            auto& phys0 = a.getComponent<SBallPhysics>();
            auto& phys1 = b.getComponent<SBallPhysics>();

            if (!a.destroyed() && !b.destroyed())
            {
                phys0.collisionHandled = true; //TODO this doesn't really do anything
                phys1.collisionHandled = true;

                if (data.type == sb::CollisionEvent::Match)
                {
                    m_gameScene.destroyEntity(data.entityA);
                    m_gameScene.destroyEntity(data.entityB);

                    
                    auto score = (1 * data.ballID) * 2;

                    if (data.ballID < BallID::Count - 1)
                    {
                        //m_gameScene.getSystem<SBallPhysicsSystem>()->spawnBall(data.ballID + 1, data.position);

                        //delay spawning by a frame so the physics can makes sure to tidy up
                        //first and not overlap the new body
                        auto ent = m_gameScene.createEntity();
                        ent.addComponent<cro::Callback>().active = true;
                        ent.getComponent<cro::Callback>().setUserData<std::uint32_t>(2);
                        ent.getComponent<cro::Callback>().function
                            = [&, data](cro::Entity e, float)
                            {
                                auto& c = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
                                c--;
                                if (c == 0)
                                {
                                    const auto id = data.ballID + 1;

                                    m_gameScene.getSystem<SBallPhysicsSystem>()->spawnBall(id, data.position);
                                    m_wheelModels[id].getComponent<cro::Callback>().active = true;
                                    e.getComponent<cro::Callback>().active = false;
                                    m_gameScene.destroyEntity(e);
                                }
                            };

                        score += 2 * (data.ballID + 1);
                    }
                    else
                    {
                        levelUp();
                    }

                    score *= m_sharedGameData.score.level;
                    m_sharedGameData.score.score += score;

                    if (m_sharedGameData.score.score > m_sharedGameData.score.personalBest)
                    {
                        m_sharedGameData.score.personalBest = m_sharedGameData.score.score;
                    }

                    floatingScore(score, data.position);

                    m_gameScene.getDirector<SBallSoundDirector>()->playSound(AudioID::Match, 2, 0.35f, data.position);
                }
                else if (data.type == sb::CollisionEvent::FastCol)
                {
                    m_gameScene.getDirector<SBallSoundDirector>()->playSound(cro::Util::Random::value(AudioID::Ball01, AudioID::Ball07), 2, 1.1f, data.position);
                }
            }
        }
        else if (data.type == sb::CollisionEvent::FastCol)
        {
            //box collision
            m_gameScene.getDirector<SBallSoundDirector>()->playSound(cro::Util::Random::value(AudioID::Ball01, AudioID::Ball07), 2, 0.5f);
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

        constexpr float padding = 0.07f;// m_previewModels[3].getComponent<cro::Transform>().getScale().x + 0.01f;
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
    //TODO this is actually the same as the scrub director
    //so we could just use that
    std::vector<std::string> paths = 
    {
        "assets/arcade/scrub/sound/fx/streak_broken.wav",
        "assets/arcade/scrub/sound/fx/zoom_in.wav",
        "assets/arcade/sportsball/sound/fx/match.wav",
        "assets/arcade/sportsball/sound/fx/ball01.wav",
        "assets/arcade/sportsball/sound/fx/ball02.wav",
        "assets/arcade/sportsball/sound/fx/ball03.wav",
        "assets/arcade/sportsball/sound/fx/ball04.wav",
        "assets/arcade/sportsball/sound/fx/ball05.wav",
        "assets/arcade/sportsball/sound/fx/ball06.wav",
        "assets/arcade/sportsball/sound/fx/ball07.wav",
    };
    m_gameScene.getDirector<SBallSoundDirector>()->loadSounds(paths, m_resources.audio);

    m_environmentMap.loadFromFile("assets/images/indoor.hdr");
    

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
                    e = m_wheelModels.emplace_back(m_gameScene.createEntity());
                    e.addComponent<cro::Transform>();
                    info.modelDef->createModel(e);

                    info.mass *= 2.f;
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
    if(!m_wheelModels.empty())
    m_wheelEnt = m_gameScene.createEntity();
    m_wheelEnt.addComponent<cro::Transform>().setPosition({ 0.6f, 0.2f, 0.2f });

    static constexpr float Rad = 0.13f;
    const float Arc = cro::Util::Const::TAU / m_wheelModels.size();

    static constexpr float BaseScale = 0.01f;

    for (auto i = 0u; i < m_wheelModels.size(); ++i)
    {
        const glm::vec3 p(std::cos(Arc * i), std::sin(Arc * i), 0.f);
        const auto scale = glm::vec3(BaseScale + (i * 0.0045f));

        m_wheelModels[i].getComponent<cro::Transform>().setPosition(p * Rad);
        m_wheelModels[i].getComponent<cro::Transform>().setScale(scale);
        m_wheelModels[i].addComponent<cro::Callback>().function = MenuTextCallback(scale.x);

        m_wheelEnt.getComponent<cro::Transform>().addChild(m_wheelModels[i].getComponent<cro::Transform>());
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
    m_gameScene.addSystem<cro::ParticleSystem>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    m_gameScene.addDirector<SBallParticleDirector>(m_resources.textures)->init();
    m_gameScene.addDirector<SBallSoundDirector>();

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

    md.loadFromFile("assets/arcade/sportsball/models/pointer.cmt");
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.5f, 1.f, 0.5f });
    md.createModel(entity);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            e.getComponent<cro::Model>().setHidden(m_previewModels[m_currentID].getComponent<cro::Model>().isHidden());
        };

    m_cursor.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto resize = [&](cro::Camera& cam)
        {
            //update shadow quality from settings
            std::uint32_t res = 512;
            switch (m_sharedData.shadowQuality)
            {
            case 0:
                res = 512;
                break;
            case 1:
                res = 1024;
                break;
            default:
                res = 2048;
                break;
            }
            cam.shadowMapBuffer.create(res, res);
            cam.setBlurPassCount(m_sharedData.shadowQuality == 0 ? 0 : 1);

            const glm::vec2 size(cro::App::getWindow().getSize());
            const float ratio = size.x / size.y;
            const float y = WorldHeight;
            const float x = y * ratio;

            cam.setOrthographic(-x / 2.f, x / 2.f, 0.f, y, 0.1f, 4.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };

            const float wheelPos = (((x - BoxWidth) / 2.f) / 2.f) + (BoxWidth / 2.f);
            m_wheelEnt.getComponent<cro::Transform>().setPosition({ wheelPos, WheelHeight, 0.f });

            for (auto e : m_nextModels)
            {
                e.getComponent<cro::Transform>().setPosition({ wheelPos, NextBallHeight, 0.f });
            }
        };

    auto camEnt = m_gameScene.getActiveCamera();
    camEnt.getComponent<cro::Transform>().setPosition({ 0.f, -0.05f, 1.4f });

    //callback for camera shake
    camEnt.addComponent<cro::Callback>().setUserData<float>(1.f);
    camEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            static const auto wt = cro::Util::Wavetable::sine(10.f);
            static std::size_t idx = 0;

            idx = (idx + 1) % wt.size();

            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct -= (dt* 2.f);

            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.x = (wt[idx] * ct) * 0.03f;

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
    cam.setMaxShadowDistance(10.f);
    cam.setShadowExpansion(15.f);


    auto lightEnt = m_gameScene.getSunlight();
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -45.f * cro::Util::Const::degToRad);
    lightEnt.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -1.f * cro::Util::Const::degToRad);
}

void SBallGameState::buildUI()
{
    const auto& font = m_sharedGameData.fonts->get(sc::FontID::Body);

    //left side is current score and PB top, leaderboard top 5 bottom
    //LB/RB or Prev/next club switches between monthly and all time?
    auto scoreRoot = m_uiScene.createEntity();
    scoreRoot.addComponent<cro::Transform>();
    scoreRoot.addComponent<cro::UIElement>(cro::UIElement::Type::Position, true);
    scoreRoot.getComponent<cro::UIElement>().relativePosition = { 0.12f, 0.8f };

    auto scoreTitle = m_uiScene.createEntity();
    scoreTitle.addComponent<cro::Transform>();
    scoreTitle.addComponent<cro::Drawable2D>();
    scoreTitle.addComponent<cro::Text>(font).setString("Score");
    scoreTitle.getComponent<cro::Text>().setFillColour(TextNormalColour);
    scoreTitle.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    scoreTitle.addComponent<cro::UIElement>(cro::UIElement::Type::Text, true).absolutePosition = { 0.f, 14.f };
    scoreTitle.getComponent<cro::UIElement>().characterSize = sc::MediumTextSize;
    scoreRoot.getComponent<cro::Transform>().addChild(scoreTitle.getComponent<cro::Transform>());

    auto scoreVal = m_uiScene.createEntity();
    scoreVal.addComponent<cro::Transform>();
    scoreVal.addComponent<cro::Drawable2D>();
    scoreVal.addComponent<cro::Text>(font);
    scoreVal.getComponent<cro::Text>().setFillColour(TextNormalColour);
    scoreVal.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    scoreVal.addComponent<cro::UIElement>(cro::UIElement::Type::Text, true).absolutePosition = { 0.f, -8.f };
    scoreVal.getComponent<cro::UIElement>().characterSize = sc::SmallTextSize;
    scoreRoot.getComponent<cro::Transform>().addChild(scoreVal.getComponent<cro::Transform>());
    m_scoreEntity = scoreVal;
    

    //this was going to be high score table but actually shows the current
    //level (which increases when making a beachball match)
    auto tableRoot = m_uiScene.createEntity();
    tableRoot.addComponent<cro::Transform>();
    tableRoot.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    tableRoot.getComponent<cro::UIElement>().relativePosition = { 0.12f, WheelHeight / WorldHeight };


    auto tableTitle = m_uiScene.createEntity();
    tableTitle.addComponent<cro::Transform>();
    tableTitle.addComponent<cro::Drawable2D>();
    tableTitle.addComponent<cro::Text>(font).setString("Level\n1");
    tableTitle.getComponent<cro::Text>().setFillColour(TextNormalColour);
    tableTitle.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    tableTitle.addComponent<cro::UIElement>(cro::UIElement::Type::Text, true).absolutePosition = { 0.f, 34.f };
    tableTitle.getComponent<cro::UIElement>().characterSize = sc::MediumTextSize;
    tableRoot.getComponent<cro::Transform>().addChild(tableTitle.getComponent<cro::Transform>());
    m_levelEntity = tableTitle;

    //auto levelEnt = m_uiScene.createEntity();
    //levelEnt.addComponent<cro::Transform>();
    //levelEnt.addComponent<cro::Drawable2D>();
    //levelEnt.addComponent<cro::Text>(font).setString("1");
    //levelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //levelEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    //levelEnt.addComponent<cro::UIElement>(cro::UIElement::Type::Text, true).absolutePosition = { 0.f, 4.f };
    //levelEnt.getComponent<cro::UIElement>().characterSize = sc::SmallTextSize;
    //tableRoot.getComponent<cro::Transform>().addChild(levelEnt.getComponent<cro::Transform>());





    //right side is next ball and wheel of evo. These have fixed world
    //positions so we should be able to line up the text relatively easily
    auto nextText = m_uiScene.createEntity();
    nextText.addComponent<cro::Transform>();
    nextText.addComponent<cro::Drawable2D>();
    nextText.addComponent<cro::Text>(font).setString("Next");
    nextText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    nextText.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    nextText.addComponent<cro::UIElement>(cro::UIElement::Type::Text, true);
    nextText.getComponent<cro::UIElement>().characterSize = sc::MediumTextSize;

    nextText.addComponent<cro::Callback>().active = true;
    nextText.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const auto pos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(m_nextModels[0].getComponent<cro::Transform>().getPosition());
            e.getComponent<cro::Transform>().setPosition(pos - glm::vec2(0.f, 24.f * cro::UIElementSystem::getViewScale()));
        };

    //ribbon along bottom diplays controls keyb/xbox/ps
    auto ribbonRoot = m_uiScene.createEntity();
    ribbonRoot.addComponent<cro::Transform>();
    ribbonRoot.addComponent<cro::UIElement>(cro::UIElement::Position, true).relativePosition = { 0.5f, 1.f };

    static constexpr float RibbonHeight = 20.f;
    auto ribbonBG = m_uiScene.createEntity();
    ribbonBG.addComponent<cro::Transform>();
    ribbonBG.addComponent<cro::Drawable2D>();
    ribbonBG.addComponent<cro::UIElement>(cro::UIElement::Sprite, true);
    ribbonBG.getComponent<cro::UIElement>().depth = -0.2f;
    ribbonBG.getComponent<cro::UIElement>().absolutePosition = { 0.f, -RibbonHeight };
    ribbonBG.getComponent<cro::UIElement>().resizeCallback =
        [](cro::Entity e)
        {
            const float width = cro::App::getWindow().getSize().x / cro::UIElementSystem::getViewScale();

            const auto c = CD32::Colours[CD32::Brown];
            e.getComponent<cro::Drawable2D>().setVertexData({
                    cro::Vertex2D(glm::vec2(-(width / 2.f), RibbonHeight), c),
                    cro::Vertex2D(glm::vec2(-(width / 2.f), 0.f), c),
                    cro::Vertex2D(glm::vec2(width / 2.f, RibbonHeight), c),
                    cro::Vertex2D(glm::vec2(width / 2.f, 0.f), c),
                });
        };

    ribbonRoot.getComponent<cro::Transform>().addChild(ribbonBG.getComponent<cro::Transform>());


    auto ribbonText = m_uiScene.createEntity();
    ribbonText.addComponent<cro::Transform>();
    ribbonText.addComponent<cro::Drawable2D>();
    ribbonText.addComponent<cro::Text>(font).setString(/*InputKeyb(m_sharedData.inputBinding)*/InputXBox);
    ribbonText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    ribbonText.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    ribbonText.addComponent<cro::UIElement>(cro::UIElement::Text, true).characterSize = sc::SmallTextSize;
    ribbonText.getComponent<cro::UIElement>().depth = 0.1f;
    ribbonText.getComponent<cro::UIElement>().absolutePosition = { 0.f, -4.f };
    ribbonText.getComponent<cro::UIElement>().verticalSpacing = 2.f;
    ribbonRoot.getComponent<cro::Transform>().addChild(ribbonText.getComponent<cro::Transform>());
    m_controlTextEntity = ribbonText;

#ifdef USE_GNS
    ribbonText = m_uiScene.createEntity();
    ribbonText.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    ribbonText.addComponent<cro::Drawable2D>();
    ribbonText.addComponent<cro::Text>(font).setString(Social::getSBallLeader());
    ribbonText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    ribbonText.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    ribbonText.addComponent<cro::UIElement>(cro::UIElement::Text, true).characterSize = sc::SmallTextSize;
    ribbonText.getComponent<cro::UIElement>().depth = 0.1f;
    ribbonText.getComponent<cro::UIElement>().absolutePosition = { 0.f, -4.f };
    ribbonText.getComponent<cro::UIElement>().verticalSpacing = 2.f;

    static constexpr float DisplayTime = 6.f;
    struct RibbonData final
    {
        float currentTime = DisplayTime;
        float scale = 1.f;
        std::int32_t state = 0;
        std::int32_t entityIndex = 0;
    };
    ribbonText.addComponent<cro::Callback>().active = true;
    ribbonText.getComponent<cro::Callback>().setUserData<RibbonData>();
    ribbonText.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [ct, scale, state, idx] = e.getComponent<cro::Callback>().getUserData<RibbonData>();
            switch (state)
            {
            default: break;
            case 0:
                //pause
                ct -= dt;
                if (ct < 0)
                {
                    ct += DisplayTime;
                    state = 1;

                    //when the ent was created this may not be up to date
                    //so we check again here
                    e.getComponent<cro::Text>().setString(Social::getSBallLeader());
                }
                break;
            case 1:
                //shrink
            {
                auto activeEnt = idx == 1 ? e : m_controlTextEntity;
                scale = std::max(0.f, scale - (dt * 5.f));

                activeEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f, cro::Util::Easing::easeOutBack(scale)));

                if (scale == 0)
                {
                    state = 2;
                }
            }
                break;
            case 2:
                //grow
            {
                auto activeEnt = idx == 0 ? e : m_controlTextEntity;
                scale = std::min(1.f, scale + (dt * 5.f));

                activeEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f, cro::Util::Easing::easeOutBack(scale)));

                if (scale == 1)
                {
                    state = 0;
                    idx = idx == 0 ? 1 : 0;
                }
            }
                break;
            }
        };
    ribbonRoot.getComponent<cro::Transform>().addChild(ribbonText.getComponent<cro::Transform>());

#endif


    //separate node has round end sign / final scores / high score on it
    //this is shown by endRound() hidden by onCachedPop()
    auto roundEndRoot = m_uiScene.createEntity();
    roundEndRoot.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    roundEndRoot.addComponent<cro::UIElement>(cro::UIElement::Position, true).relativePosition = { 0.5f, 0.5f };
    roundEndRoot.getComponent<cro::UIElement>().depth = 2.f;
    
    roundEndRoot.addComponent<cro::Callback>().setUserData<float>(0.f);
    roundEndRoot.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
            progress = std::min(1.f, progress + dt);

            const auto scale = cro::Util::Easing::easeOutBounce(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            if (progress == 1)
            {
                progress = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        };
    
    m_roundEndEntity = roundEndRoot;


    //background fade
    const cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    auto bgEnt = m_uiScene.createEntity();
    bgEnt.addComponent<cro::Transform>();
    bgEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-0.5f, 0.5f), c),
            cro::Vertex2D(glm::vec2(-0.5f), c),
            cro::Vertex2D(glm::vec2(0.5f), c),
            cro::Vertex2D(glm::vec2(0.5f, -0.5f), c),
        });
    bgEnt.addComponent<cro::Callback>().active = true;
    bgEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            const auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::Transform>().setScale(size);
        };
    roundEndRoot.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

    //round end text
    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Transform>();
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setString("Game Over");
    textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    textEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    textEnt.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    textEnt.getComponent<cro::UIElement>().characterSize = LargeTextSize;
    textEnt.getComponent<cro::UIElement>().depth = 0.1f;
    textEnt.getComponent<cro::UIElement>().resizeCallback =
        [](cro::Entity e)
        {
            const auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::UIElement>().absolutePosition.y = (size.y / 4.f) / cro::UIElementSystem::getViewScale();
        };
    roundEndRoot.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());

    //score summary
    textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Transform>();
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setString("You Scored: 123456\nSpace: Restart  -  Esc Quit");
    textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    textEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    textEnt.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    textEnt.getComponent<cro::UIElement>().characterSize = MediumTextSize;
    textEnt.getComponent<cro::UIElement>().depth = 0.1f;
    textEnt.getComponent<cro::UIElement>().absolutePosition = { 0.f, 22.f };
    roundEndRoot.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    m_endScoreTextEntity = textEnt;

    //personal best text to flash when appropriate
    textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setString("Personal Best!!");
    textEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
    textEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    textEnt.addComponent<cro::UIElement>(cro::UIElement::Text, true);
    textEnt.getComponent<cro::UIElement>().characterSize = MediumTextSize;
    textEnt.getComponent<cro::UIElement>().depth = 0.1f;
    textEnt.getComponent<cro::UIElement>().resizeCallback =
        [](cro::Entity e)
        {
            const auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::UIElement>().absolutePosition.y = -(size.y / 4.f) / cro::UIElementSystem::getViewScale();
        };

    struct FlashData final
    {
        float currTime = 0.1f;
        cro::Drawable2D::Facing facing = cro::Drawable2D::Facing::Front;
    };
    textEnt.addComponent<cro::Callback>().active = true;
    textEnt.getComponent<cro::Callback>().setUserData<FlashData>();
    textEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            static constexpr float FlashTime = 1.f;
            auto& [ct, facing] = e.getComponent<cro::Callback>().getUserData<FlashData>();
            ct -= dt;
            if (ct < 0)
            {
                ct += FlashTime;
                facing = facing == cro::Drawable2D::Facing::Front ? cro::Drawable2D::Facing::Back : cro::Drawable2D::Facing::Front;
                e.getComponent<cro::Drawable2D>().setFacing(facing);
            }
        };

    roundEndRoot.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    m_pbTextEntity = textEnt;



    //make sure to do this last
    updateScoreString();

    auto resize = [](cro::Camera& cam)
        {
            const auto size = glm::vec2(cro::App::getWindow().getSize());

            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    resize(cam);
    cam.resizeCallback = resize;
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

void SBallGameState::floatingScore(std::int32_t score, glm::vec3 pos)
{
    const auto& font = m_sharedGameData.fonts->get(sc::FontID::Body);
    const auto viewScale = cro::UIElementSystem::getViewScale();

    //spawn some score text at pos
    const auto screenPos = m_gameScene.getActiveCamera().getComponent<cro::Camera>().coordsToPixel(pos);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(screenPos, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(std::to_string(score));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize * viewScale);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset * viewScale);

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = FloatingTextAnim(m_uiScene, viewScale);


    //update the score UI element here too
    updateScoreString();
}

void SBallGameState::updateScoreString()
{
    cro::String scoreStr = std::to_string(m_sharedGameData.score.score) + "\n\nPersonal Best:\n" + std::to_string(m_sharedGameData.score.personalBest);
//#ifdef USE_GNS
//    const auto leader = Social::getSBallLeader();
//    scoreStr += "\n\nCurrent Leader:\n" + leader;// +Social::getSBallLeader();
//#endif
    
    m_scoreEntity.getComponent<cro::Text>().setString(scoreStr);

    //update level text
    m_levelEntity.getComponent<cro::Text>().setString("Level\n" + std::to_string(m_sharedGameData.score.level));
}

void SBallGameState::levelUp()
{
    Achievements::awardAchievement(AchievementStrings[AchievementID::AnotherLevel]);

    m_sharedGameData.score.level++;

    const auto& font = m_sharedGameData.fonts->get(sc::FontID::Body);
    const auto viewScale = cro::UIElementSystem::getViewScale();
    const auto screenPos = glm::vec2(cro::App::getWindow().getSize()) / 2.f;

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(screenPos, 0.1f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Level Up!!");
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize * viewScale);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::LargeTextOffset * viewScale);

    auto bounds = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::Transform>().move({ 0.f, bounds.height / 2.f });

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = FloatingTextAnim(m_uiScene, viewScale);


    //camera shake
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().active = true;
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().setUserData<float>(1.f);

    m_gameScene.getDirector<SBallSoundDirector>()->playSound(AudioID::LevelUp, 2, 0.5f);
}

void SBallGameState::endGame()
{
    //prevents input for RoundEndTime
    m_roundEndClock.restart();

    m_gameScene.setSystemActive<SBallPhysicsSystem>(false);
    m_gameEnded = true;

    //camera shake
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().setUserData<float>(1.f);
    m_gameScene.getActiveCamera().getComponent<cro::Callback>().active = true;

    //show score summary 
    //TODO post scores to leaderboard
    cro::String scoreText = "Score: " + std::to_string(m_sharedGameData.score.score) + "\n";
    switch (lastInput)
    {
    default:
        scoreText += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " Restart  -  " + "Escape Quit";
        break;
    case InputType::PS:
        scoreText += cro::String(ButtonCross) + " Restart  -  " + cro::String(ButtonCircle) + " Quit";
        break;
    case InputType::XBox:
        scoreText += cro::String(ButtonA) + " Restart  -  " + cro::String(ButtonB) + " Quit";
        break;
    }
    m_endScoreTextEntity.getComponent<cro::Text>().setString(scoreText);

    //show PB text if needed
    if (m_sharedGameData.score.score > Social::getSBallPB())
    {
        m_pbTextEntity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }
    Social::setSBallScore(m_sharedGameData.score.score);

    //display game over sign
    m_roundEndEntity.getComponent<cro::Callback>().setUserData<float>(0.f);
    m_roundEndEntity.getComponent<cro::Callback>().active = true;

    m_gameScene.getDirector<SBallSoundDirector>()->playSound(AudioID::GameOver, 2, 0.5f);
}

void SBallGameState::onCachedPush()
{
    //reset the game
    auto pos = m_cursor.getComponent<cro::Transform>().getPosition();
    pos.x = 0.f;
    m_cursor.getComponent<cro::Transform>().setPosition(pos);

    m_sharedGameData.score.personalBest = Social::getSBallPB();
    m_sharedGameData.score.level = 1;
    m_sharedGameData.score.score = 0;
    updateScoreString();

    m_gameScene.setSystemActive<SBallPhysicsSystem>(true);
    m_gameScene.getSystem<SBallPhysicsSystem>()->clearBalls();
    m_gameEnded = false;

    //hacky intro text
    const auto size = glm::vec2(cro::App::getWindow().getSize());
    const auto& font = m_sharedGameData.fonts->get(sc::FontID::Title);
    const auto viewScale = cro::UIElementSystem::getViewScale();

    auto ent = m_uiScene.createEntity();
    ent.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 1.f));
    ent.addComponent<cro::Drawable2D>();
    ent.addComponent<cro::Text>(font).setString("Ready!");
    ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
    ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    ent.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize * viewScale);

    struct CBData final
    {
        float progress = 0.f;
        std::int32_t state = 0;
    };

    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    ent.addComponent<cro::Callback>().active = true;
    ent.getComponent<cro::Callback>().setUserData<CBData>();
    ent.getComponent<cro::Callback>().function =
        [&, size, viewScale](cro::Entity e, float dt)
        {
            auto& [progress, state] = e.getComponent<cro::Callback>().getUserData<CBData>();
            
            if (state == 0)
            {
                //zoom in
                progress = std::min(1.f, progress + (dt * 3.f));

                const float scale = cro::Util::Easing::easeOutBounce(progress);
                e.getComponent<cro::Transform>().setScale(glm::vec2(scale, 1.f));

                if (progress == 1)
                {
                    state = 1;
                }
            }
            else if (state == 1)
            {
                progress = std::max(0.f, progress - dt);
                if (progress == 0)
                {
                    state = 2;
                    e.getComponent<cro::Text>().setString("GO!");
                }
            }
            else if (state == 2)
            {
                progress = std::min(1.f, progress + dt);
                if (progress == 1)
                {
                    state = 3;
                }
            }
            else
            {
                //move off screen
                e.getComponent<cro::Transform>().move({ -(3200.f * viewScale) * dt, 0.f });
                if (e.getComponent<cro::Transform>().getPosition().x < -size.x)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            }
        };

}

void SBallGameState::onCachedPop()
{
    //reset any end-game overlay
    m_roundEndEntity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_pbTextEntity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
}