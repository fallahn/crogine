/*-----------------------------------------------------------------------

Matt Marchant 2024
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

#include "ScrubGameState.hpp"
#include "ScrubSoundDirector.hpp"
#include "ScrubSharedData.hpp"
#include "ScrubPhysicsSystem.hpp"
#include "../golf/GameConsts.hpp"
#include "../golf/InputBinding.hpp"
#include "../golf/SharedStateData.hpp"
#include "../Colordome-32.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ShadowMapRenderer.hpp>

#include <crogine/graphics/Font.hpp>

#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

#include <crogine/detail/OpenGL.hpp>

#include <sstream>

namespace
{
#include "Shaders.inl"

    const std::string tempBackground = "assets/arcade/scrub/images/noise.png";

    constexpr float BallRadius = 0.021f;
    constexpr float StrokeDistance = 0.16f - BallRadius;
    constexpr float BallOffsetPos = 0.2f;
    constexpr float BallExitPos = 0.01f;

    constexpr std::int32_t MaxSoapBars = 5; //don't add if counter is this much

    struct BallAnimationData final
    {
        glm::vec3 velocity = glm::vec3(1.8f, 0.8f, 0.f);
        std::int32_t state = 0;
    };

    struct AudioID final
    {
        enum
        {
            FXThreeX, FXFiveX, FXTenX,
            FXEjectBall, FXInsertBall,
            FXFillSoap, FXNoSoap,
            FXScrubDown, FXScrubUp,
            FXSoapAdded, FXStreakBroken,

            FXMessage, FXPremature,

            VOThreeX, VOFiveX, VOTenX,
            VOGo, VONewSoap, VOReady,
            VORoundEnd,

            Count
        };
    };
}

ScrubGameState::ScrubGameState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd, SharedScrubData& sc)
    : cro::State            (stack, context),
    m_sharedData            (sd),
    m_sharedScrubData       (sc),
    m_soundDirector         (nullptr),
    m_gameScene             (context.appInstance.getMessageBus()),
    m_uiScene               (context.appInstance.getMessageBus(), 512),
    m_pitchStage            (0),
    m_soapAnimationActive   (false),
    m_axisPosition          (0),
    m_leftTriggerPosition   (0),
    m_rightTriggerPosition  (0),
    m_controllerIndex       (0)
{
    //this is a pre-cached state
    addSystems();
    loadAssets();
    createScene();
    createUI();
}

//public
bool ScrubGameState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return false;
    }

    static const auto pumpDown = 
        [&]()
        {
            if (m_score.gameRunning)
            {
                m_ball.scrub(m_handle.switchDirection(Handle::Down, m_score.ballsWashed));
                m_soundDirector->playSound(AudioID::FXScrubDown, MixerChannel::Effects, 0.4f).getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.85f, 1.2f));
            }
        };
    static const auto pumpUp = 
        [&]()
        {
            if (m_score.gameRunning)
            {
                m_ball.scrub(m_handle.switchDirection(Handle::Up, m_score.ballsWashed));
                m_soundDirector->playSound(AudioID::FXScrubUp, MixerChannel::Effects, 0.4f).getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.85f, 1.2f));
            }
        };
    static const auto insertBall =
        [&]()
        {
            if (m_score.gameRunning)
            {
                if (m_handle.progress == 0
                    && m_handle.speed == 0
                    && !m_handle.hasBall
                    && m_ball.state == Ball::State::Idle)
                {
                    m_handle.locked = true;

                    m_ball.state = Ball::State::Insert;
                    m_soundDirector->playSound(AudioID::FXInsertBall, MixerChannel::Effects);
                }
            }
        };
    static const auto removeBall = 
        [&]()
        {
            if (m_score.gameRunning)
            {
                if (m_handle.progress == 0
                    && m_handle.speed == 0
                    && m_handle.hasBall)
                {
                    m_handle.hasBall = false;
                    m_ball.state = Ball::State::Extract;

                    //this *should* work because the models *should* all be at 0,0,0
                    m_handle.entity.getComponent<cro::Transform>().removeChild(m_ball.entity.getComponent<cro::Transform>());

                    m_soundDirector->playSound(AudioID::FXEjectBall, MixerChannel::Effects);
                }
            }
        };
    static const auto addSoap = 
        [&]()
        {
            if (m_score.gameRunning)
            {
                if (m_handle.soap.count)
                {
                    //this also does the resetting of the soap values
                    showSoapEffect();
                }
                else
                {
                    m_soundDirector->playSound(AudioID::FXNoSoap, MixerChannel::Menu);
                }
            }
        };

    static const auto pause = [&]() 
        {
            if (!m_score.gameFinished)
            {
                m_music.getComponent<cro::AudioEmitter>().pause();
                m_gameScene.simulate(0.f);
                requestStackPush(StateID::ScrubPause);
            }
        };

    static const auto quit = [&]() 
        {
            if (!m_score.gameRunning
                && m_score.quitTimeout > 2.f)
            {
                requestStackPop();
            }
        };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            pause();
            break;
#ifdef CRO_DEBUG_
        case SDLK_l:
            m_score.remainingTime = 0.f;
            break;
        case SDLK_o:
            showSoapEffect();
            break;
        case SDLK_p:
            m_gameScene.getSystem<ScrubPhysicsSystem>()->spawnBall(cro::Colour::Magenta);
            break;
#endif

        }


        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left])
        {
            pumpDown();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right])
        {
            pumpUp();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            insertBall();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            removeBall();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            addSoap();
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        //TODO do we want to prevent other controller input
        //and lock to only the controller used to start this game?
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftShoulder:
            insertBall();
            break;
        case cro::GameController::ButtonRightShoulder:
            removeBall();
            break;
        case cro::GameController::DPadLeft:
            pumpDown();
            break;
        case cro::GameController::DPadRight:
            pumpUp();
            break;
        case cro::GameController::ButtonA:
            addSoap();
            break;
        case cro::GameController::ButtonStart:
            pause();
            break;
        }
        m_controllerIndex = cro::GameController::controllerID(evt.cbutton.which);
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        //TODO again do we want to lock the controller ID?
        //will having two controllers somehow be an advantage?
        static constexpr std::int16_t Threshold = std::numeric_limits<std::int16_t>::max() / 2;
        const auto v = evt.caxis.value;
        
        if (evt.caxis.axis == cro::GameController::AxisRightX)
        {
            if (m_axisPosition < Threshold
                && v > Threshold)
            {
                pumpDown();
            }
            else if (m_axisPosition > -Threshold
                && v < -Threshold)
            {
                pumpUp();
            }

            m_axisPosition = v;
        }
        else if (evt.caxis.axis == cro::GameController::TriggerLeft)
        {
            if (m_leftTriggerPosition < Threshold
                && v > Threshold)
            {
                pumpDown();
            }
            m_leftTriggerPosition = v;
        }
        else if (evt.caxis.axis == cro::GameController::TriggerRight)
        {
            if (m_rightTriggerPosition < Threshold
                && v > Threshold)
            {
                pumpUp();
            }
            m_rightTriggerPosition = v;
        }



        if (evt.caxis.value < -cro::GameController::LeftThumbDeadZone
            || evt.caxis.value > cro::GameController::LeftThumbDeadZone)
        {
            m_controllerIndex = cro::GameController::controllerID(evt.caxis.which);
        }
    }
    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        pause();
    }
    else if (evt.type == SDL_CONTROLLERDEVICEADDED)
    {
        for (auto i = 0; i < 4; ++i)
        {
            cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createFeedback(0, 1));
        }
    }

    else if (evt.type == SDL_KEYUP
        || evt.type == SDL_CONTROLLERBUTTONUP)
    {
        //skips through the score summary and quits
        if (m_score.summary.active)
        {
            switch (m_score.summary.status)
            {
            default: break;
            case Score::Summary::Time:
                m_score.totalScore += m_score.summary.runtimeBonus - m_score.summary.counter;
                m_score.summary.status = Score::Summary::BallCount;
                break;
            case Score::Summary::BallCount:
                m_score.totalScore += m_score.summary.ballCountBonus - m_score.summary.counter;
                m_score.summary.status = Score::Summary::Avg;
                break;
            case Score::Summary::Avg:
                m_score.totalScore += m_score.summary.cleanAvgBonus - m_score.summary.counter;
                m_score.summary.status = Score::Summary::Done;
                break;
            case Score::Summary::Done:
                quit();
                break;
            }
            m_score.summary.counter = 0;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void ScrubGameState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && data.id == StateID::ScrubPause)
        {
            //resume music
            m_music.getComponent<cro::AudioEmitter>().play();
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool ScrubGameState::simulate(float dt)
{
    if (m_score.gameRunning)
    {
        //if there are messages waiting set the first one active
        m_messageQueue.erase(
            std::remove_if(m_messageQueue.begin(), m_messageQueue.end(),
                [](const cro::Entity& e) {return !e.isValid(); }),
            m_messageQueue.end());

        if (!m_messageQueue.empty())
        {
            if(!m_messageQueue[0].getComponent<cro::Callback>().active)
            {
                m_soundDirector->playSound(AudioID::FXMessage, MixerChannel::Effects);
            }
            m_messageQueue[0].getComponent<cro::Callback>().active = true;
        }

        auto oldTime = m_score.remainingTime;

        m_score.totalRunTime += dt;
#ifndef CRO_DEBUG_
        m_score.remainingTime = std::max(m_score.remainingTime - dt, 0.f);
#endif
        switch (m_pitchStage)
        {
        default:
        case 0:
            if (oldTime > 5
                && m_score.remainingTime <= 5)
            {
                m_pitchStage = 1;
                m_music.getComponent<cro::AudioEmitter>().setPitch(PitchChangeA);
            }
            break;
        case 1:
            if (oldTime > 3
                && m_score.remainingTime <= 3)
            {
                m_pitchStage = 2;
                m_music.getComponent<cro::AudioEmitter>().setPitch(PitchChangeB);
            }
            break;
        case 2: 
            //do nothing but also consume this state as we don't want default behaviour
            break;
        }

        m_score.quitTimeout = 0.f;
        if (m_score.remainingTime == 0)
        {
            endRound();
        }
    }
    else
    {
        m_score.quitTimeout += dt;
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ScrubGameState::render()
{
#ifdef HIDE_BACKGROUND
    m_tempBground.draw();
#endif

    auto oldCam = m_gameScene.setActiveCamera(m_bucketCamera);
    m_bucketTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
#ifdef CRO_DEBUG_
    //m_gameScene.getSystem<ScrubPhysicsSystem>()->renderDebug(m_bucketCamera.getComponent<cro::Camera>().getActivePass().viewProjectionMatrix, m_bucketTexture.getSize());
#endif
    m_bucketTexture.display();


    m_soapTexture.clear(cro::Colour::Black);
    m_soapVertices.draw();
    m_soapTexture.display();

    m_gameScene.setActiveCamera(oldCam);
    m_gameScene.render();
    m_uiScene.render();


    
}

//private
void ScrubGameState::onCachedPush()
{
    for (auto i = 0; i < 4; ++i)
    {
        cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, cro::GameController::DSEffect::createFeedback(0, 1));
    }

    //check ball state and detach if parented to handle
    //if (m_ball.entity.getComponent<cro::Transform>().getDepth() == 1)
    {
        m_handle.entity.getComponent<cro::Transform>().removeChild(m_ball.entity.getComponent<cro::Transform>());
        m_ball.entity.getComponent<cro::Transform>().setPosition({ -BallOffsetPos, 0.f, 0.f });
    }

    m_ball.reset();
    m_handle.reset();
    m_score = {};
    m_score.summary = {};

    //makes sure to update the UI layout/textures
    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback(cam);


    //calls createCountIn()
    resetCamera();
    
    //spin-in animation
    m_animatedEntities[AnimatedEntity::ScrubberRoot].getComponent<cro::Transform>().setScale(glm::vec3(0.f));
    m_animatedEntities[AnimatedEntity::ScrubberRoot].getComponent<cro::Callback>().setUserData<float>(3.f);
    m_animatedEntities[AnimatedEntity::ScrubberRoot].getComponent<cro::Callback>().active = true;

    //ui animations
    const auto height = m_animatedEntities[AnimatedEntity::UITop].getComponent<cro::Sprite>().getTextureBounds().height;
    m_animatedEntities[AnimatedEntity::UITop].getComponent<cro::Transform>().setOrigin({ 0.f, -height });
    m_animatedEntities[AnimatedEntity::UITop].getComponent<cro::Callback>().active = true;



    //reset the music volume
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct = std::min(1.f, ct + dt);

            cro::AudioMixer::setPrefadeVolume(ct, MixerChannel::Music);

            if (ct == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };

    cro::AudioMixer::setPrefadeVolume(0.f, MixerChannel::Music);
    m_music.getComponent<cro::AudioEmitter>().play();


    //choose a model at random
    if (cro::Util::Random::value(0, 1) == 0)
    {
        m_body0.getComponent<cro::Model>().setHidden(true);
        m_body1.getComponent<cro::Model>().setHidden(false);
    }
    else
    {
        m_body0.getComponent<cro::Model>().setHidden(false);
        m_body1.getComponent<cro::Model>().setHidden(true);
    }
}

void ScrubGameState::onCachedPop()
{
    for (auto i = 0; i < 4; ++i)
    {
        cro::GameController::applyDSTriggerEffect(i, cro::GameController::DSTriggerBoth, {});
    }

    cro::Command cmd;
    cmd.targetFlags = CommandID::UI::GarbageCollect;
    cmd.action = [&](cro::Entity e, float) {m_uiScene.destroyEntity(e); };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    m_uiScene.simulate(0.f); //forces immediate garbage collection

    m_pitchStage = 0;
    m_music.getComponent<cro::AudioEmitter>().setPitch(1.f);
    m_music.getComponent<cro::AudioEmitter>().stop();

    m_gameScene.getSystem<ScrubPhysicsSystem>()->clearBalls();

    m_gameScene.simulate(0.f);

    m_soapVertexData.clear();
    m_soapVertices.setVertexData(m_soapVertexData);
    m_soapAnimationActive = false;
}

void ScrubGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<ScrubPhysicsSystem>(mb);
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb); //hm we should probably move this to ui scene for consistency with other states?

    m_soundDirector = m_gameScene.addDirector<ScrubSoundDirector>();

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::ParticleSystem>(mb);
}

void ScrubGameState::loadAssets()
{
#ifdef HIDE_BACKGROUND
    //temp
    auto& tempTex = m_resources.textures.get(tempBackground);
    tempTex.setRepeated(true);
    m_tempBground.setTexture(tempTex);
#endif

    m_environmentMap.loadFromFile("assets/images/hills.hdr");

    //shaders
    m_resources.shaders.loadFromString(sc::ShaderID::LevelMeter, cro::RenderSystem2D::getDefaultVertexShader(), LevelMeterFragment, "#define TEXTURED\n");
    m_resources.shaders.loadFromString(sc::ShaderID::Fire, cro::RenderSystem2D::getDefaultVertexShader(), FireFragment, "#define TEXTURED\n");
    m_resources.shaders.loadFromString(sc::ShaderID::Soap, cro::RenderSystem2D::getDefaultVertexShader(), SoapFragment, "#define TEXTURED\n");


    //render textures
    m_bucketTexture.create(BucketTextureSize.x, BucketTextureSize.y, true, false, 2); //hmm really we should be following the user setting for MSAA
    m_bucketTexture.setSmooth(true);
    m_soapTexture.create(SoapTextureSize.x, SoapTextureSize.y, false);
    m_soapTexture.setSmooth(true);

    m_soapVertices.setBlendMode(cro::Material::BlendMode::Additive);
    m_soapVertices.setTexture(m_resources.textures.get("assets/arcade/scrub/images/droplet.png"));
    m_soapVertices.setPrimitiveType(GL_TRIANGLES);

    //bucket physics
    m_gameScene.getSystem<ScrubPhysicsSystem>()->loadMeshData();


    //load audio
    std::vector<std::string> paths =
    {
        "assets/arcade/scrub/sound/fx/3x.wav",
        "assets/arcade/scrub/sound/fx/5x.wav",
        "assets/arcade/scrub/sound/fx/10x.wav",
        "assets/arcade/scrub/sound/fx/eject_ball.wav",
        "assets/arcade/scrub/sound/fx/insert_ball.wav",
        "assets/arcade/scrub/sound/fx/fill_soap.wav",
        "assets/arcade/scrub/sound/fx/no_soap.wav",
        "assets/arcade/scrub/sound/fx/scrub_down.wav",
        "assets/arcade/scrub/sound/fx/scrub_up.wav",
        "assets/arcade/scrub/sound/fx/soap_added.wav",
        "assets/arcade/scrub/sound/fx/streak_broken.wav",

        "assets/arcade/scrub/sound/fx/message.wav",
        "assets/arcade/scrub/sound/fx/premature.wav",

        "assets/arcade/scrub/sound/vo/3x.wav",
        "assets/arcade/scrub/sound/vo/5x.wav",
        "assets/arcade/scrub/sound/vo/10x.wav",
        "assets/arcade/scrub/sound/vo/go.wav",
        "assets/arcade/scrub/sound/vo/new_soap.wav",
        "assets/arcade/scrub/sound/vo/ready.wav",
        "assets/arcade/scrub/sound/vo/round_end.wav",
    };

    m_soundDirector->loadSounds(paths, m_resources.audio);
    
    //load menu music
    auto id = m_resources.audio.load("assets/arcade/scrub/sound/music/game.ogg", true);
    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>().setSource(m_resources.audio.get(id));
    entity.getComponent<cro::AudioEmitter>().setLooped(true);
    entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::UserMusic);
    entity.getComponent<cro::AudioEmitter>().setVolume(0.45f);
    m_music = entity;
}

void ScrubGameState::createScene()
{
    cro::Entity rootNode = m_gameScene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });
    rootNode.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::max(0.f, currTime - dt);
            const float rotation = cro::Util::Const::TAU * currTime * 4.f;
            const float scale = std::clamp(1.f - (currTime / 3.f), 0.f, 1.f);

            e.getComponent<cro::Transform>().setScale(glm::vec3(scale));
            e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);

            if (currTime == 0)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        };
    m_animatedEntities[AnimatedEntity::ScrubberRoot] = rootNode;

    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/arcade/scrub/models/handle.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = 
            std::bind(&ScrubGameState::handleCallback, this, std::placeholders::_1, std::placeholders::_2);

        m_handle.entity = entity;
        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    const std::string ballPath = "assets/arcade/scrub/models/ball.cmt";
    if (md.loadFromFile(ballPath))
    {
        m_gameScene.getSystem<ScrubPhysicsSystem>()->loadBallData(m_resources, &m_environmentMap, ballPath);


        m_ball.colourIndex = cro::Util::Random::value(0u, CD32::Colours.size() - 1);


        //player ball
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -BallOffsetPos, 0.f, 0.f });
        md.createModel(entity);

        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CD32::Colours[m_ball.colourIndex]);
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            std::bind(&ScrubGameState::ballCallback, this, std::placeholders::_1, std::placeholders::_2);

        m_ball.entity = entity;


        //animated ball
        entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -BallOffsetPos, 0.f, -10.f });
        md.createModel(entity);

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<BallAnimationData>();
        entity.getComponent<cro::Callback>().function = 
            [](cro::Entity e, float dt)
            {
                auto& data = e.getComponent<cro::Callback>().getUserData<BallAnimationData>();
                if (data.state == 0)
                {
                    static constexpr glm::vec3 Gravity = glm::vec3(0.f, -8.f, 0.f);
                    data.velocity += Gravity * dt;
                    e.getComponent<cro::Transform>().move(data.velocity * dt);

                    if (e.getComponent<cro::Transform>().getPosition().y < -2.f)
                    {
                        data.state = 1;
                    }
                }
            };
        m_ball.animatedEntity = entity;
    }

    if (md.loadFromFile("assets/arcade/scrub/models/body.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        
        m_body0 = entity;

        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    if (md.loadFromFile("assets/arcade/scrub/models/body_v2.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        
        m_body1 = entity;
        
        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    //represents how clean the current ball is
    if (md.loadFromFile("assets/arcade/scrub/models/gauge_inner.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -0.10836f, -0.24753f, 0.008162f });
        md.createModel(entity);
        entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CD32::Colours[CD32::Red]);
        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(0.f);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                float cleanliness = (Ball::MaxFilth - m_ball.filth);
                const auto c = cleanliness > m_score.threshold ? CD32::Colours[CD32::GreenLight] : CD32::Colours[CD32::Red];

                e.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", c);

                cleanliness /= Ball::MaxFilth;

                const float Speed = dt * 3.f;
                auto& pos = e.getComponent<cro::Callback>().getUserData<float>();
                if (pos < cleanliness)
                {
                    pos = std::min(cleanliness, pos + Speed);
                }
                else
                {
                    pos = std::max(cleanliness, pos - Speed);
                }

                e.getComponent<cro::Transform>().setScale({ 1.f, pos });
            };
    }

    if (md.loadFromFile("assets/arcade/scrub/models/gauge_outer.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }


    auto resize = [](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setPerspective(74.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto camera = m_gameScene.getActiveCamera();
    auto& cam = camera.getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    cam.shadowMapBuffer.create(2048, 2048);
    cam.setMaxShadowDistance(1.f);
    cam.setShadowExpansion(6.f);

    //callback is set up in resetCamera();
    camera.addComponent<cro::Callback>();

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -1.2f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.6f);



    //bucket scene rendered with a second camera
    if (md.loadFromFile("assets/arcade/scrub/models/bucket.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ BucketOffset, 0.f, 0.f });
        entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, 0.3f);
        md.createModel(entity);
    }


    auto resize2 = [&](cro::Camera& cam)
        {
            glm::vec2 targetSize(m_bucketTexture.getSize());
            cam.setPerspective(48.f * cro::Util::Const::degToRad, targetSize.x / targetSize.y, 3.f, 7.f);
            cam.viewport = { 0.f, 0.f, 1.f ,1.f };
        };
    camera = m_gameScene.createEntity();
    camera.addComponent<cro::Transform>().setPosition({ BucketOffset, 1.9f, 5.f });
    auto& cam2 = camera.addComponent<cro::Camera>();
    cam2.resizeCallback = resize2;
    cam2.shadowMapBuffer.create(2048, 2048);
    cam2.setMaxShadowDistance(8.f);
    cam2.setShadowExpansion(12.f);

    resize2(cam2);
    m_bucketCamera = camera;

    //registerWindow([&]() 
    //    {
    //        ImGui::Begin("Cam");

    //        /*static float fov = 48.f;
    //        static float y = 0.f;

    //        auto updateCam = [&]()
    //            {
    //                glm::vec2 targetSize(m_bucketTexture.getSize());
    //                
    //                auto& c = m_bucketCamera.getComponent<cro::Camera>();
    //                c.setPerspective(fov * cro::Util::Const::degToRad, targetSize.x / targetSize.y, 0.1f, 10.f);

    //                auto pos = m_bucketCamera.getComponent<cro::Transform>().getPosition();
    //                pos.y = y;
    //                m_bucketCamera.getComponent<cro::Transform>().setPosition(pos);
    //            };

    //        if (ImGui::SliderFloat("FOV", &fov, 10.f, 90.f))
    //        {
    //            updateCam();
    //        }

    //        if (ImGui::SliderFloat("VPos", &y, 0.f, 5.f))
    //        {
    //            updateCam();
    //        }*/

    //        /*static float shadowDist = 8.f;
    //        static float shadowEspand = 12.f;

    //        if (ImGui::SliderFloat("Dist: %3.3f", &shadowDist, 1.f, 12.f))
    //        {
    //            m_gameScene.getActiveCamera().getComponent<cro::Camera>().setMaxShadowDistance(shadowDist);
    //        }

    //        if (ImGui::SliderFloat("Exp: %3.3f", &shadowEspand, 0.2f, 15.f))
    //        {
    //            m_gameScene.getActiveCamera().getComponent<cro::Camera>().setShadowExpansion(shadowEspand);
    //        }*/

    //        ImGui::End();
    //    });
}

void ScrubGameState::createUI()
{
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("Buns"))
    //        {
    //            //ImGui::Image(m_gameScene.getActiveCamera().getComponent<cro::Camera>().shadowMapBuffer.getTexture(), { 200.f ,200.f }, { 0.f, 1.f }, { 1.f, 0.f });

    //            ImGui::Text("Remaining Time: %3.2f", m_score.remainingTime);
    //            ImGui::Text("Balls Washed: %d", m_score.ballsWashed);
    //            ImGui::Text("Avg Cleanliness %3.2f", m_score.avgCleanliness);

    //            ImGui::NewLine();
    //            ImGui::Separator();
    //            ImGui::NewLine();

    //            ImGui::Text("Handle Speed: %3.2f", m_handle.speed);
    //            ImGui::Text("Handle Direction: %3.2f", m_handle.direction);

    //            ImGui::Text("Handle Stroke: %3.2f", m_handle.stroke);
    //            ImGui::Text("Handle Progress: %3.2f", m_handle.progress);

    //            ImGui::NewLine();
    //            ImGui::Separator();
    //            ImGui::NewLine();

    //            ImGui::Text("Ball Filth: %3.2f", m_ball.filth);
    //            ImVec4 c = (Ball::MaxFilth - m_ball.filth) > m_score.threshold ? ImVec4(0.f, 1.f, 0.f, 1.f) : ImVec4(1.f, 0.f, 0.f, 1.f);
    //            ImGui::SameLine();
    //            ImGui::ColorButton("##buns", c, 0, { 12.f, 12.f });

    //            ImGui::Text("Soap Level: %3.2f", m_handle.soap.amount);
    //            ImGui::Text("Soap Bars: %d", m_handle.soap.count);
    //            ImGui::Text("Soap LifeTime %3.3f", m_handle.soap.lifeTime);
    //            ImGui::Text("Soap Reduction %3.3f", m_handle.soap.getReduction());
    //        }
    //        ImGui::End();
    //    });



    //scales down sprites to lower resolutions
    m_spriteRoot = m_uiScene.createEntity();
    m_spriteRoot.addComponent<cro::Transform>();

    const auto& largeFont = m_sharedScrubData.fonts->get(sc::FontID::Title);
    const auto& smallFont = m_sharedScrubData.fonts->get(sc::FontID::Body);

    //remaining time
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(sc::SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { 12.f, -12.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            std::stringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(2);
            ss << "Remaining: " << m_score.remainingTime << "s";
            e.getComponent<cro::Text>().setString(ss.str());
        };

    //ball count
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(sc::SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { 16.f, 16.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            std::stringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(2);
            ss << "Balls Cleaned: " << m_score.ballsWashed;
            e.getComponent<cro::Text>().setString(ss.str());
        };

    //avg cleanliness
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(sc::SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { 12.f, -26.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            std::stringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(2);
            ss << "Average: " << m_score.avgCleanliness << "%";
            e.getComponent<cro::Text>().setString(ss.str());
        };


    //score
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(sc::MediumTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -14.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            std::stringstream ss;
            ss << m_score.totalScore;
            e.getComponent<cro::Text>().setString(ss.str());
        };

    //soap count
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(sc::MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(1.f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { -212.f, -20.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            std::stringstream ss;
            ss << "Soap Bars: " << m_handle.soap.count;
            e.getComponent<cro::Text>().setString(ss.str());
        };

    //balls until next soap
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(sc::SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(1.f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { -212.f, -8.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const auto nextSoap = m_score.awardLevel - ((m_score.ballsWashed - m_score.countAtThreshold) % m_score.awardLevel);
            
            std::stringstream ss;
            ss << "Next Soap in: " << nextSoap;
            e.getComponent<cro::Text>().setString(ss.str());
        };


    //streak count
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(sc::MediumTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { -106.f, -16.f};
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_score.bonusRun)
            {
                std::stringstream ss;
                ss << m_score.bonusRun << "x";
                e.getComponent<cro::Text>().setString(ss.str());
            }
            else
            {
                e.getComponent<cro::Text>().setString(" ");
            }
        };


#ifdef HIDE_BACKGROUND
    auto& bgTex = m_resources.textures.get(tempBackground);
#else
    auto& bgTex = m_sharedScrubData.backgroundTexture->getTexture();
    //auto& bgTex = m_resources.textures.get("assets/images/startup.png");
#endif

    //wooden bar at top
    auto& barTexture = m_resources.textures.get("assets/arcade/scrub/images/ui_top.png");
    barTexture.setRepeated(true);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(barTexture);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -entity.getComponent<cro::Sprite>().getTextureBounds().height };
    entity.getComponent<UIElement>().depth = sc::UIBackgroundDepth;
    entity.getComponent<UIElement>().resizeCallback = [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.width = static_cast<float>(cro::App::getWindow().getSize().x) * (MaxSpriteScale / getViewScale());
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        };
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, -entity.getComponent<cro::Sprite>().getTextureBounds().height });
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            const auto Speed = 250.f * dt;
            static constexpr float MaxOffset = 30.f;

            auto o = e.getComponent<cro::Transform>().getOrigin();
            o.y = std::min(MaxOffset, o.y + Speed);
            e.getComponent<cro::Transform>().setOrigin(o);

            if (o.y == MaxOffset)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        };
    m_animatedEntities[AnimatedEntity::UITop] = entity;
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    
    //100% streak
    static constexpr float ProgBarHeight = 180.f;
    static constexpr float ProgBarWidth = 800.f;

    auto& noiseTex = m_resources.textures.get("assets/golf/images/wind.png");
    noiseTex.setRepeated(true);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, ProgBarHeight), glm::vec2(0.f, 2.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f, 0.2f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(ProgBarWidth, ProgBarHeight), glm::vec2(1.f, 2.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(ProgBarWidth, 0.f), glm::vec2(1.f, 0.2f), cro::Colour::White)
        });
    m_resources.textures.setFallbackColour(cro::Colour::White);
    entity.getComponent<cro::Drawable2D>().setTexture(&noiseTex);
    entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(sc::ShaderID::Fire));
    entity.getComponent<cro::Drawable2D>().setBlendMode(cro::Material::BlendMode::Additive);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { -ProgBarWidth / 2.f, -((ProgBarHeight / 2.f) + 52.f) };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
            const float target = static_cast<float>(std::min(10, m_score.bonusRun)) / 10.f;

            const float Speed = dt / 2.f;
            if (progress < target)
            {
                progress = std::min(target, progress + Speed);
            }
            else if (progress > target)
            {
                progress = std::max(target, progress - Speed);
            }
            
            cro::FloatRect cropping = { 0.f, 0.f, ProgBarWidth * progress, ProgBarHeight };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);

            static float t = 0.f;
            t += dt;
            e.getComponent<cro::Drawable2D>().bindUniform("u_time", t); //TODO - this, properly.
        };
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto createLevelMeter = 
        [&](cro::Colour c)
        {
            auto ent = m_uiScene.createEntity();
            ent.addComponent<cro::Transform>();
            ent.addComponent<cro::Drawable2D>().setVertexData(
                {
                    cro::Vertex2D(glm::vec2(0.f, BarHeight), c),
                    cro::Vertex2D(glm::vec2(0.f), c),
                    cro::Vertex2D(glm::vec2(BarWidth, BarHeight), c),
                    cro::Vertex2D(glm::vec2(BarWidth, 0.f), c)
                }
            );
            ent.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(sc::ShaderID::LevelMeter));
            ent.getComponent<cro::Drawable2D>().setTexture(&bgTex);
            ent.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
            ent.addComponent<UIElement>().depth = sc::UIMeterDepth;
            ent.getComponent<UIElement>().resizeCallback = std::bind(&ScrubGameState::levelMeterCallback, this, std::placeholders::_1);
            m_spriteRoot.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            return ent;
        };


    //soap level
    entity = createLevelMeter(SoapMeterColour);
    entity.getComponent<UIElement>().relativePosition = glm::vec2(1.f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { -(BarWidth + 42.f), 12.f };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& currPos = e.getComponent<cro::Callback>().getUserData<float>();
            const auto target = std::clamp((m_handle.soap.amount - Handle::Soap::MinSoap) / (Handle::Soap::MaxSoap - Handle::Soap::MinSoap), 0.f, 1.f);

            const float speed = dt;// *(0.1f + (0.9f * currPos));

            if (currPos > target)
            {
                currPos = std::max(target, currPos - speed);
            }
            else
            {
                currPos = std::min(target, currPos + speed);
            }
            
            //TODO it might be nice to do this as part of the shader but I cleverly didn't
            //include an effecient way to set uniforms per-drawable.
            /*cro::Colour c = glm::mix(glm::vec4(1.f), SoapMeterColour.getVec4(), currPos);
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour = c;
            }*/

            //so let's crop the drawable instead
            cro::FloatRect cropping = { 0.f, 0.f, BarWidth, BarHeight * currPos };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        };
    
    auto tubeEnt = createLevelMeter(cro::Colour::White);
    tubeEnt.getComponent<UIElement>().depth = -0.6f;
    entity.getComponent<cro::Transform>().addChild(tubeEnt.getComponent<cro::Transform>());
    auto soapEnt = entity;

    //current ball cleanliness
    entity = createLevelMeter(cro::Colour::Blue);
    entity.getComponent<UIElement>().relativePosition = glm::vec2(1.f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { -((BarWidth + 42.f) * 2.f), 12.f };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            float cleanliness = (Ball::MaxFilth - m_ball.filth);
            const auto c = cleanliness > m_score.threshold ? CD32::Colours[CD32::GreenLight] : CD32::Colours[CD32::Red];

            cleanliness /= Ball::MaxFilth;
            
            cro::FloatRect cropping = { 0.f, 0.f, BarWidth, BarHeight * cleanliness };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);

            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour = c;
            }
        };
    
    tubeEnt = createLevelMeter(cro::Colour::White);
    tubeEnt.getComponent<UIElement>().depth = -0.1f;
    entity.getComponent<cro::Transform>().addChild(tubeEnt.getComponent<cro::Transform>());

    //soap animation quad - hmm should we just parent this to the soap meter?
    const auto soapQuadSize = glm::vec2(SoapTextureSize);
    static constexpr cro::Colour SoapColour = SoapMeterColour;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, soapQuadSize.y), SoapColour),
            cro::Vertex2D(glm::vec2(0.f), SoapColour),
            cro::Vertex2D(soapQuadSize, SoapColour),
            cro::Vertex2D(glm::vec2(soapQuadSize.x, 0.f), SoapColour)
        }
    );
    entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(sc::ShaderID::Soap));
    entity.getComponent<cro::Drawable2D>().setTexture(&bgTex);
    entity.getComponent<cro::Drawable2D>().bindUniform("u_bubbleTexture", cro::TextureID(m_soapTexture.getTexture()));
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = -0.3f;
    entity.getComponent<UIElement>().resizeCallback = std::bind(&ScrubGameState::levelMeterCallback, this, std::placeholders::_1);
    entity.getComponent<UIElement>().absolutePosition = { -((soapQuadSize.x - BarWidth) / 2.f), 0.f };
    soapEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //soap particles
    entity = m_uiScene.createEntity();;
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 100.f });
    entity.addComponent<cro::ParticleEmitter>().settings.loadFromFile("assets/arcade/scrub/particles/soap.cps", m_resources.textures);
    entity.getComponent<cro::ParticleEmitter>().start();
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //bucket animation quad - TODO add this to m_animatedEntities and have it slide in on start?
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_bucketTexture.getTexture());
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { 42.f, 60.f };
    entity.getComponent<UIElement>().depth = sc::UIBackgroundDepth ;
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    auto resize = [&](cro::Camera& cam) mutable
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);

#ifdef HIDE_BACKGROUND
        glm::vec2 bgSize(m_resources.textures.get(tempBackground).getSize());
        auto bgScale = size / bgSize;
        m_tempBground.setScale(bgScale);
#endif


        //text resizes font, sprites scale down...
        const auto Scale = getViewScale();
        const auto SpriteScale = Scale / MaxSpriteScale;
        m_spriteRoot.getComponent<cro::Transform>().setScale(glm::vec2(SpriteScale)); //sprites are 1:1 when the scene scale is 4 

        //send messge to UI elements to reposition them
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action = 
            [size, Scale, SpriteScale](cro::Entity e, float)
            {
                auto s = size;
                if (!e.hasComponent<cro::Text>())
                {
                    s /= SpriteScale;
                }

                const auto& ui = e.getComponent<UIElement>();
                float x = std::floor(s.x * ui.relativePosition.x);
                float y = std::floor(s.y * ui.relativePosition.y);
                
                if (ui.resizeCallback)
                {
                    ui.resizeCallback(e);
                }

                //TODO this should be an enum so we can use
                //small/med/large charsize AND shadow offsets
                if (ui.characterSize)
                {
                    e.getComponent<cro::Text>().setCharacterSize(ui.characterSize * Scale);
                    e.getComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2((ui.absolutePosition * Scale) + glm::vec2(x,y)), ui.depth));
                }
                else
                {
                    e.getComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2((ui.absolutePosition) + glm::vec2(x, y)), ui.depth));
                }
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void ScrubGameState::createCountIn()
{
    const auto& largeFont = m_sharedScrubData.fonts->get(sc::FontID::Title);
    
    glm::vec2 size(cro::App::getWindow().getSize());
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(size / 2.f);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("READY");
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::LargeTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::GarbageCollect;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().characterSize = sc::LargeTextSize;

    struct MessageData final
    {
        float t = 2.f;
        std::int32_t state = 0;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<MessageData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            static constexpr float GoTime = 1.4f;

            auto& [t, state] = e.getComponent<cro::Callback>().getUserData<MessageData>();
            t -= dt;
            if (state == 0)
            {
                //ready
                if (t < 0)
                {
                    t += GoTime;
                    state = 1;
                    e.getComponent<cro::Text>().setString("GO!");
                    m_soundDirector->playSound(AudioID::VOGo, MixerChannel::Voice);
                    m_score.gameRunning = true;
                }
            }
            else
            {
                //go
                if (t < 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }

                const glm::vec2 winSize(cro::App::getWindow().getSize());

                const auto progress = 1.f - std::max(0.f, std::min(1.f, t / (GoTime / 2.f)));
                const float xPos = winSize.x * 2.5f * progress;
                glm::vec2 position = (winSize / 2.f);
                position.x += xPos;

                e.getComponent<cro::Transform>().setPosition(position);

                static constexpr float Stretch = 1.52f;
                e.getComponent<cro::Transform>().setScale({ 1.f + (progress * Stretch), 1.f - (progress * Stretch) });
            }
        };

    attachText(entity);
    m_soundDirector->playSound(AudioID::VOReady, MixerChannel::Voice);
}

void ScrubGameState::handleCallback(cro::Entity e, float dt)
{
    m_handle.progress = std::clamp(m_handle.progress + (m_handle.speed * -m_handle.direction * dt), 0.f, 1.f);

    auto pos = e.getComponent<cro::Transform>().getPosition();
    pos.y = cro::Util::Easing::easeOutSine(m_handle.progress) * -StrokeDistance;
    e.getComponent<cro::Transform>().setPosition(pos);

    const float rotation = m_handle.progress * cro::Util::Const::PI;
    e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, rotation);

    if (m_handle.progress == 0 || m_handle.progress == 1)
    {
        if (m_handle.speed != 0)
        {
            m_ball.scrub(m_handle.calcStroke(m_score.ballsWashed));
        }
        m_handle.speed = 0.f;
    }
}

void ScrubGameState::ballCallback(cro::Entity e, float dt)
{
    switch (m_ball.state)
    {
    default:
    case Ball::State::Idle:
        break;
    case Ball::State::Insert:
    {
        auto pos = m_ball.entity.getComponent<cro::Transform>().getPosition();
        pos.x = std::min(0.f, pos.x + Ball::Speed * dt);
        m_ball.entity.getComponent<cro::Transform>().setPosition(pos);

        if (pos.x == 0)
        {
            m_handle.entity.getComponent<cro::Transform>().addChild(m_ball.entity.getComponent<cro::Transform>());
            m_handle.locked = false;
            m_handle.hasBall = true;
            m_ball.state = Ball::State::Clean;
        }
    }
    break;
    case Ball::State::Clean:
        m_handle.soap.lifeTime += dt;
        break;
    case Ball::State::Extract:
    {
        auto pos = m_ball.entity.getComponent<cro::Transform>().getPosition();
        pos.x += Ball::Speed * dt;

        if (pos.x > BallExitPos)
        {
            if (updateScore())
            {
                m_gameScene.getSystem<ScrubPhysicsSystem>()->spawnBall(CD32::Colours[m_ball.colourIndex]);
            }

            m_ball.state = Ball::State::Idle;
            m_ball.filth = Ball::MaxFilth;
            pos.x = -BallOffsetPos;

            m_ball.animatedEntity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CD32::Colours[m_ball.colourIndex]);
            m_ball.animatedEntity.getComponent<cro::Transform>().setPosition({ BallExitPos, 0.f, 0.f });
            m_ball.animatedEntity.getComponent<cro::Callback>().setUserData<BallAnimationData>(); //resets the animation

            m_ball.colourIndex += cro::Util::Random::value(1, 3);
            m_ball.colourIndex %= CD32::Colours.size();
            m_ball.entity.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CD32::Colours[m_ball.colourIndex]);
        }
        m_ball.entity.getComponent<cro::Transform>().setPosition(pos);
    }
    break;
    }
}

bool ScrubGameState::updateScore()
{
    const float cleanliness = Ball::MaxFilth - m_ball.filth;

    if (cleanliness < m_score.threshold)
    {
        //display notification
        const auto& font = m_sharedScrubData.fonts->get(sc::FontID::Body);
        const auto size = glm::vec2(cro::App::getWindow().getSize());

        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>().setPosition({ size.x / 2.f, 80.f * getViewScale(), sc::TextDepth});
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Text>(font).setCharacterSize(sc::MediumTextSize * getViewScale());
        ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        ent.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Red]);
        ent.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        ent.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
        ent.getComponent<cro::Text>().setString("Premature Ejection!");

        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().setUserData<float>(3.f);
        ent.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;

                std::int32_t flash = static_cast<std::int32_t>(std::ceil(currTime * 2.f)) % 2;
                if (flash)
                {
                    e.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Red]);
                    e.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
                }
                else
                {
                    e.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
                    e.getComponent<cro::Text>().setShadowColour(cro::Colour::Transparent);
                }

                if (currTime < 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            };
        
        m_score.remainingTime -= 0.5f;
        showMessage("-0.5s");

        //don't forget to reset the streak
        if (m_score.bonusRun != 0)
        {
            if (m_score.bonusRun >= Score::bonusRunThreshold)
            {
                showMessage("Soap Multiplier Decreased");
            }

            showMessage("STREAK BROKEN");
            m_soundDirector->playSound(AudioID::FXStreakBroken, MixerChannel::Effects);
        }
        else
        {
            m_soundDirector->playSound(AudioID::FXPremature, MixerChannel::Effects);
        }
        m_score.bonusRun = 0;
        
        return false;
    }

    m_score.ballsWashed++;
    m_score.cleanlinessSum += cleanliness;
    m_score.avgCleanliness = m_score.cleanlinessSum / m_score.ballsWashed;

    const auto ballScore = static_cast<std::int32_t>(std::floor(cleanliness));
    m_score.totalScore += ballScore;
    showMessage("+" + std::to_string(ballScore));


    //track bonus runs of 3x 100%, 5x 100% and 10x 100%
    if (cleanliness == 100.f)
    {
        m_score.bonusRun++;
        switch (m_score.bonusRun)
        {
            //this should never be 0, but just in case...
        case 0: break;
        case 3:
            m_score.totalScore += 300;
            m_score.remainingTime += 0.25f;

            showMessage("3x STREAK!");
            showMessage("+350");
            showMessage("+0.25s");

            m_soundDirector->playSound(AudioID::VOThreeX, MixerChannel::Voice);
            m_soundDirector->playSound(AudioID::FXThreeX, MixerChannel::Effects, 0.6f);
            break;
        case Score::bonusRunThreshold:
            m_score.countAtThreshold = m_score.ballsWashed;
            showMessage("Soap Multiplier Increased");
            break;
        case 5:
            m_score.totalScore += 500;
            m_score.remainingTime += 0.35f;

            showMessage("5x STREAK!");
            showMessage("+550");
            showMessage("+0.35s");

            m_soundDirector->playSound(AudioID::VOFiveX, MixerChannel::Voice);
            m_soundDirector->playSound(AudioID::FXFiveX, MixerChannel::Effects, 0.6f);
            break;
        default: 
            //every 10 after
            if (m_score.bonusRun % 10 == 0)
            {
                m_score.totalScore += 1000;
                m_score.remainingTime += 0.75f;

                showMessage("10x STREAK!");
                showMessage("+1500");
                showMessage("+0.75s");

                m_soundDirector->playSound(AudioID::VOTenX, MixerChannel::Voice);
                m_soundDirector->playSound(AudioID::FXTenX, MixerChannel::Effects, 0.3f);
            }
            else
            {
                showMessage("PERFECT");
            }
            break;
        }
    }
    else
    {
        if (m_score.bonusRun != 0)
        {
            if (m_score.bonusRun >= Score::bonusRunThreshold)
            {
                showMessage("Soap Multiplier Decreased");
            }

            showMessage("STREAK BROKEN");
            m_soundDirector->playSound(AudioID::FXStreakBroken, MixerChannel::Effects);

            m_score.remainingTime -= std::max(1.f, m_score.remainingTime / 2.f);
        }
        m_score.bonusRun = 0;
        //m_score.countAtThreshold = 0;
    }

    m_score.awardLevel = (m_score.bonusRun > Score::bonusRunThreshold) ? 4 : 5;

    //this might happen just as the time runs out - we want to
    //keep the score but not add time in this case
    if (m_score.gameRunning)
    {
        float bonus = Score::TimeBonus * cro::Util::Easing::easeOutQuad(cleanliness / 100.f);
        bonus -= std::min(bonus / 2.f, 0.05f * (m_score.ballsWashed / 10));
        m_score.remainingTime += bonus;
        
        if (bonus != 0)
        {
            std::stringstream ss;
            ss.precision(2);
            ss.setf(std::ios::fixed);
            ss << "+" << bonus << "s";

            showMessage(ss.str());
        }
        const float bonusProgress = static_cast<float>(m_score.bonusRun) / 100.f;
        m_score.remainingTime += Score::TimeBonus * bonusProgress;

        if (bonusProgress != 0)
        {
            std::stringstream ss;
            ss.precision(2);
            ss.setf(std::ios::fixed);
            ss << "+" << bonusProgress * Score::TimeBonus << "s";

            showMessage(ss.str());
        }
    }


    if (((m_score.ballsWashed - m_score.countAtThreshold) % m_score.awardLevel) == 0)
    {
        //new soap in 3.. 2.. 1..
        const auto& font = m_sharedScrubData.fonts->get(sc::FontID::Body);
        const auto size = glm::vec2(cro::App::getWindow().getSize());

        const auto basePos = glm::vec2({ size.x / 2.f, 80.f * getViewScale() });

        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(glm::vec3(basePos, sc::TextDepth));
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Text>(font).setCharacterSize(sc::MediumTextSize * getViewScale());
        ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        ent.getComponent<cro::Text>().setFillColour(CD32::Colours[CD32::Red]);
        ent.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        ent.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
        ent.getComponent<cro::Text>().setString("New Soap Bar In\n3");
        ent.getComponent<cro::Text>().setVerticalSpacing(4.f);
        ent.addComponent<cro::CommandTarget>().ID = CommandID::UI::GarbageCollect; //we want to make sure this is removed on a game reset

        static constexpr float TotalTime = 3.f;
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().setUserData<float>(TotalTime);
        ent.getComponent<cro::Callback>().function =
            [&, basePos](cro::Entity e, float dt)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime -= dt;

                const float count = std::ceil(currTime);
                std::stringstream ss;
                ss << "New Soap Bar In\n" << (int)count;
                e.getComponent<cro::Text>().setString(ss.str());

                const float AnimateTime = 0.5f;
                const auto winSize = glm::vec2(cro::App::getWindow().getSize());

                auto pos = basePos;

                //animate in
                auto iProgress = 1.f - std::max(0.f, std::min(1.f, (TotalTime - currTime) / AnimateTime));
                const auto iTarget = glm::vec2(winSize.x / 2.f, 0.f) - basePos;
                pos += iTarget * cro::Util::Easing::easeInBounce(iProgress);


                //animate out
                const float scale = /*cro::Util::Easing::easeOutCirc*/(std::max(0.f, std::min(1.f, currTime / (AnimateTime/2.f))));
                const float oProgress = 1.f - scale;

                const auto oTarget = (winSize - glm::vec2(20.f)) - basePos;
                pos += (oTarget * oProgress);
                e.getComponent<cro::Transform>().setPosition(pos);

                e.getComponent<cro::Transform>().setScale(glm::vec2(0.1f + (0.9f * scale)));

                if (currTime < 0)
                {
                    m_score.totalScore += 500;
                    m_soundDirector->playSound(AudioID::FXSoapAdded, MixerChannel::Effects);

                    m_handle.soap.count = std::min(MaxSoapBars, m_handle.soap.count + 1);
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            };

        m_score.threshold = std::min(Ball::MaxFilth - 0.5f, m_score.threshold + 4.f);
        //m_score.remainingTime += 0.1f;

        //showMessage("+0.1s");
        m_soundDirector->playSound(AudioID::VONewSoap, MixerChannel::Voice);
    }

    return true;
}

void ScrubGameState::endRound()
{
    m_music.getComponent<cro::AudioEmitter>().pause();
    m_soundDirector->playSound(AudioID::FXStreakBroken, MixerChannel::Effects);
    m_soundDirector->playSound(AudioID::VORoundEnd, MixerChannel::Voice);

    m_score.gameRunning = false;
    m_score.gameFinished = true;

#ifdef CRO_DEBUG_
    /*m_score.avgCleanliness = 100.f;
    m_score.totalRunTime = 68.f;
    m_score.ballsWashed = 14;
    m_score.totalScore = 3900;*/

    
    m_score.avgCleanliness = 64.f;
    m_score.totalRunTime = 71.f;
    m_score.ballsWashed = 24;
    m_score.totalScore = 5600;
    
#endif
    m_score.summary.runtimeBonus = static_cast<std::int32_t>(std::floor(m_score.totalRunTime * 10.f));
    m_score.summary.ballCountBonus = m_score.ballsWashed * 10;
    m_score.summary.cleanAvgBonus = static_cast<std::int32_t>(std::floor(m_score.avgCleanliness * 10.f));


    //game over, show scores.
    const auto& font = m_sharedScrubData.fonts->get(sc::FontID::Title);
    const auto animIn = [](cro::Entity e, float dt)
        {
            const auto Speed = dt * 3.f;
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + Speed);
            const auto scale = cro::Util::Easing::easeOutElastic(std::max(0.f, currTime));
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            if (currTime == 1)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        };

    //Title
    glm::vec2 size(cro::App::getWindow().getSize());
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f, sc::TextDepth });
    entity.getComponent<cro::Transform>().move({ 0.f, 72.f * getViewScale() });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Game Over");
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::LargeTextOffset);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::GarbageCollect;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 72.f };
    entity.getComponent<UIElement>().characterSize = sc::LargeTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = animIn;
    attachText(entity);


    //score text
    const auto& font2 = m_sharedScrubData.fonts->get(sc::FontID::Body);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f, sc::TextDepth });
    entity.getComponent<cro::Transform>().move({ 0.f, -14.f * getViewScale() });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font2).setString("Total Score: " + std::to_string(m_score.totalScore) + "\n\n\nPress Any Button");
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::GarbageCollect;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -14.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(-0.5f);
    entity.getComponent<cro::Callback>().function = animIn;
    attachText(entity);
    auto totalText = entity;

    //animated text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f, sc::TextDepth });
    entity.getComponent<cro::Transform>().move({ 0.f, -44.f * getViewScale() });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font2).setString("Time Bonus: 0\nBall Count Bonus: 0\nAverage Clean Bonus: 0");
    entity.getComponent<cro::Text>().setCharacterSize(sc::SmallTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement | CommandID::UI::GarbageCollect;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -44.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(-1.f);
    entity.getComponent<cro::Callback>().function = animIn;
    attachText(entity);
    auto bonusText = entity;


    //as above entities have callbacks to animate them we create
    //this one here to update the text for each of the above separately
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::GarbageCollect;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(-1.f);
    entity.getComponent<cro::Callback>().function =
        [&, totalText, bonusText](cro::Entity e, float dt) mutable
        {
            auto& c = e.getComponent<cro::Callback>().getUserData<float>();
            c += dt;
            if (c < 0)
            {
                return;
            }

            m_score.summary.active = true;

            //this can take ages so we count up multiple times per frame
            static constexpr std::int32_t UpdatesPerFrame = 8;

            cro::String bonusString;

            for (auto i = 0; i < UpdatesPerFrame; ++i)
            {
                switch (m_score.summary.status)
                {
                default:
                    if (i == UpdatesPerFrame - 1)
                    {
                        bonusString = "Time Bonus: " + std::to_string(m_score.summary.runtimeBonus)
                            + "\nBall Count Bonus: " + std::to_string(m_score.summary.ballCountBonus)
                            + "\nAverage Clean Bonus: " + std::to_string(m_score.summary.cleanAvgBonus);
                    }
                    break;
                case Score::Summary::Time:
                    m_score.summary.counter++;
                    m_score.totalScore++;

                    if (i == UpdatesPerFrame - 1)
                    {
                        bonusString = "Time Bonus: " + std::to_string(m_score.summary.counter)
                            + "\nBall Count Bonus: 0\nAverage Clean Bonus: 0";
                    }

                    if (m_score.summary.counter == m_score.summary.runtimeBonus)
                    {
                        m_score.summary.counter = 0;
                        m_score.summary.status++;
                    }
                    break;
                case Score::Summary::BallCount:
                    m_score.summary.counter++;
                    m_score.totalScore++;

                    if (i == UpdatesPerFrame - 1)
                    {
                        bonusString = "Time Bonus: " + std::to_string(m_score.summary.runtimeBonus)
                            + "\nBall Count Bonus: " + std::to_string(m_score.summary.counter) + "\nAverage Clean Bonus: 0";
                    }

                    if (m_score.summary.counter == m_score.summary.ballCountBonus)
                    {
                        m_score.summary.counter = 0;
                        m_score.summary.status++;
                    }
                    break;
                case Score::Summary::Avg:
                    m_score.summary.counter++;
                    m_score.totalScore++;

                    if (i == UpdatesPerFrame - 1)
                    {
                        bonusString = "Time Bonus: " + std::to_string(m_score.summary.runtimeBonus)
                            + "\nBall Count Bonus: " + std::to_string(m_score.summary.ballCountBonus)
                            + "\nAverage Clean Bonus: " + std::to_string(m_score.summary.counter);
                    }

                    if (m_score.summary.counter >= m_score.summary.cleanAvgBonus)
                    {
                        m_score.summary.counter = 0;
                        m_score.summary.status++;
                    }
                    break;
                }
            }
            //make the button press text flash
            if (static_cast<std::int32_t>(c) % 2 == 0)
            {
                totalText.getComponent<cro::Text>().setString("Total Score: " + std::to_string(m_score.totalScore));
            }
            else
            {
                totalText.getComponent<cro::Text>().setString("Total Score: " + std::to_string(m_score.totalScore) + "\n\n\nPress Any Button");
            }
            bonusText.getComponent<cro::Text>().setString(bonusString);
        };


    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -1.f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 1.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(1.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(1.f, 0.f), cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha))
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::Transform>().setScale(size);
        };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::GarbageCollect;
}

void ScrubGameState::showMessage(const std::string& str)
{
    const auto size = glm::vec2(cro::App::getWindow().getSize());
    const auto& smallFont = m_sharedScrubData.fonts->get(sc::FontID::Body);

    static constexpr float MessageTime = 0.5f;

    auto pos = size / 2.f;
    pos.y -= 40.f;

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize * getViewScale());
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().characterSize = sc::LargeTextSize;
    entity.addComponent<cro::Callback>().setUserData<float>(MessageTime);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::max(0.f, currTime - dt);

            const auto invTime = cro::Util::Easing::easeOutCirc(std::clamp(1.f - (currTime / MessageTime), 0.f, 1.f));
            const float scale = 0.25f + (invTime * 0.75f);

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            auto colour = CD32::Colours[CD32::Yellow];
            colour.setAlpha(cro::Util::Easing::easeOutQuint(1.f - invTime));

            e.getComponent<cro::Text>().setFillColour(colour);

            if (currTime == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };
    m_messageQueue.push_back(entity);

    attachText(entity);
}

void ScrubGameState::showSoapEffect()
{
    if (m_soapAnimationActive)
    {
        return;
    }

    static constexpr glm::vec2 BubbleSize = glm::vec2(100.f, 100.f);
    struct SoapBubble final
    {
        glm::vec2 position = glm::vec2(0.f);
        glm::vec2 velocity = glm::vec2(0.f);
        float scale = 1.f;

        void createVerts(std::vector<cro::Vertex2D>& dest)
        {
            auto offset = (BubbleSize / 2.f) * scale;
            offset.y *= (1.f + scale);

            dest.emplace_back(position + glm::vec2(-offset.x, offset.y), glm::vec2(0.f, 1.f));
            dest.emplace_back(position - offset, glm::vec2(0.f));
            dest.emplace_back(position + offset, glm::vec2(1.f));

            dest.emplace_back(position + offset, glm::vec2(1.f));
            dest.emplace_back(position - offset, glm::vec2(0.f));
            dest.emplace_back(position + glm::vec2(offset.x, -offset.y), glm::vec2(1.f, 0.f));
        }
    };

    struct SoapCallbackData final
    {
        std::vector<SoapBubble> bubbles;
        float emitTime = 2.f;// 1.5f;
        float emitRate = 0.08f;

        float timeToEmission = 1.f; //give this some value so we spawn at least one immediately

        float previousPosition = 0.f; //tracks the height of the first particle so we know when to trigger soap filling
        float triggerPosition = 0.f;

        bool soapRemoved = false;
    };

    SoapCallbackData d;
    d.emitTime *= (1.f - (m_handle.soap.amount / Handle::Soap::MaxSoap)); //so we shorten the anim if soap is partially full
    d.triggerPosition = BarHeight * (m_handle.soap.amount / Handle::Soap::MaxSoap);

    //create an entity which updates the SimpleVertexArray
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::GarbageCollect;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SoapCallbackData>(d);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<SoapCallbackData>();
            data.emitTime -= dt;
            data.timeToEmission += dt;

            if (data.emitTime > 0)
            {
                //check emit time
                if (data.timeToEmission > data.emitRate)
                {
                    data.timeToEmission -= data.emitRate;
                    data.emitRate *= (1.03f * 1.03f);

                    auto& bubble = data.bubbles.emplace_back();
                    bubble.position = { SoapTextureSize.x / 2, SoapTextureSize.y + 20 };
                    bubble.scale = 0.35f;
                    bubble.velocity.y = -1000.f;

                    //add some randomness
                    std::int32_t MaxOffset = static_cast<std::int32_t>(SoapTextureSize.x) / 4;
                    bubble.position.x += cro::Util::Random::value(-MaxOffset, MaxOffset);
                    bubble.scale += cro::Util::Random::value(0.01f, 0.04f);
                }                
            }

            m_soapVertexData.clear();

            //check array, if empty delete entity
            if (data.bubbles.empty())
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);

                m_soapAnimationActive = false;
            }
            else //else update vertices
            {
                //check to see if the first one has hit the soap level
                auto p = data.bubbles[0].position.y;
                if (p < data.triggerPosition
                    && data.previousPosition > data.triggerPosition
                    && !data.soapRemoved)
                {
                    data.soapRemoved = true;

                    m_handle.soap.count--;
                    m_handle.soap.refresh();
                    m_soundDirector->playSound(AudioID::FXFillSoap, MixerChannel::Menu);
                }
                data.previousPosition = p;

                //remove bubbles which have gone off the bottom
                data.bubbles.erase(std::remove_if(data.bubbles.begin(), data.bubbles.end(), [](const SoapBubble& b)
                    {
                        return b.position.y < -20.f;
                    }), data.bubbles.end());

                static constexpr glm::vec2 Gravity(0.f, -1200.f);

                for (auto& b : data.bubbles)
                {
                    b.velocity += Gravity * dt;
                    b.position += b.velocity * dt;
                    b.scale *= 1.013f;
                    b.createVerts(m_soapVertexData);
                }
            }
            m_soapVertices.setVertexData(m_soapVertexData);
        };

    m_soapAnimationActive = true;

    //TODO either create an entity or use an existing
    //entity with bubble particles and start emitting
}

void ScrubGameState::resetCamera()
{
    //create a path of points, convert them with look-at and the animate the camera along them
    static std::array path =
    {
        //glm::inverse(glm::lookAt(glm::vec3(0.14f, 0.02f, 0.f), glm::vec3(0.f, 0.01f, 0.f), cro::Transform::Y_AXIS)),
        glm::inverse(glm::lookAt(glm::vec3(0.f, 0.12f, 0.06f), glm::vec3(0.f, 0.01f, 0.f), cro::Transform::Y_AXIS)),
        glm::inverse(glm::lookAt(glm::vec3(-0.04f, 0.07f, 0.36f), glm::vec3(0.f, -0.04f, 0.f), cro::Transform::Y_AXIS))
    };

    static const auto interpolate =
        [](glm::mat4& mat1, glm::mat4& mat2, float t)
        {
            const glm::quat rot0 = glm::quat_cast(mat1);
            const glm::quat rot1 = glm::quat_cast(mat2);

            const glm::quat finalRot = glm::slerp(rot0, rot1, t);

            glm::mat4 finalMat = glm::mat4_cast(finalRot);
            finalMat[3] = mat1[3] * (1.f - t) + mat2[3] * t;

            return finalMat;
        };

    auto camera = m_gameScene.getActiveCamera();
    camera.getComponent<cro::Transform>().setLocalTransform(path[0]);

    camera.getComponent<cro::Callback>().active = true;
    camera.getComponent<cro::Callback>().setUserData<float>(0.f);
    camera.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime += dt;

            auto idx = static_cast<std::int32_t>(std::floor(currTime));
            float t = currTime - idx;

            if (idx < path.size() - 1)
            {
                glm::mat4 tx = interpolate(path[idx], path[idx + 1], t);
                e.getComponent<cro::Transform>().setLocalTransform(tx);
            }
            else
            {
                e.getComponent<cro::Transform>().setLocalTransform(path.back());
                e.getComponent<cro::Callback>().active = false;

                createCountIn();
            }
        };
}

void ScrubGameState::attachText(cro::Entity entity)
{
    const auto Scale = getViewScale();
    CRO_ASSERT(Scale != 0, "");
    const auto& ui = entity.getComponent<UIElement>();
    entity.getComponent<cro::Text>().setCharacterSize(ui.characterSize * Scale);
}

void ScrubGameState::attachSprite(cro::Entity entity)
{
    const auto screenCoords = entity.getComponent<cro::Transform>().getPosition();
    const auto Scale = m_spriteRoot.getComponent<cro::Transform>().getScale().x;
    CRO_ASSERT(Scale != 0, "");
    entity.getComponent<cro::Transform>().setPosition({ std::floor(screenCoords.x / Scale), std::floor(screenCoords.y / Scale) });
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void ScrubGameState::levelMeterCallback(cro::Entity e)
{
    //recalc the UV coords for the background texture
    const auto globalBounds = e.getComponent<cro::Drawable2D>().getLocalBounds().transform(e.getComponent<cro::Transform>().getWorldTransform());

    //TODO this should be the size of the texture we're setting
    //in the shader (we're just assuming it's window sized)
    const auto windowSize = glm::vec2(cro::App::getWindow().getSize());
    cro::FloatRect uvRect(globalBounds.left / windowSize.x,
        globalBounds.bottom / windowSize.y,
        globalBounds.width / windowSize.x,
        globalBounds.height / windowSize.y);

    //set the coords on the verts
    auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
    verts[0].UV = { uvRect.left, uvRect.bottom + uvRect.height };
    verts[1].UV = { uvRect.left, uvRect.bottom };
    verts[2].UV = { uvRect.left + uvRect.width, uvRect.bottom + uvRect.height };
    verts[3].UV = { uvRect.left + uvRect.width, uvRect.bottom };

    //set u_uvRect to rect left/bottom
    e.getComponent<cro::Drawable2D>().bindUniform("u_uvRect", glm::vec4(uvRect.left, uvRect.bottom, uvRect.width, uvRect.height));

#ifndef HIDE_BACKGROUND
    //reset the texture on the drawable
    e.getComponent<cro::Drawable2D>().setTexture(&m_sharedScrubData.backgroundTexture->getTexture());
#endif
}

//handle funcs
float ScrubGameState::Handle::switchDirection(float d, std::int32_t ballsWashed)
{
    CRO_ASSERT(d == Down || d == Up, "");

    float ret = 0.f;
    if (d != direction
        && !locked)
    {
        //do this first as it uses the current direction
        ret = calcStroke(ballsWashed);

        direction = d;
        speed = MaxSpeed;

        if (hasBall)
        {
            soap.amount = std::max(Soap::MinSoap, soap.amount - soap.getReduction());
        }
    }
    return ret;
}

float ScrubGameState::Handle::calcStroke(std::int32_t ballsWashed)
{
    const float currPos = entity.getComponent<cro::Transform>().getPosition().y;
    stroke = ((currPos - strokeStart) / StrokeDistance) * direction;
    strokeStart = currPos;

    const float difficulty = std::max(0.6f, 1.f - (0.1f * (ballsWashed / 8))); //lower the harder it is to clean

    if (hasBall)
    {
        return cro::Util::Easing::easeInQuad(stroke) * soap.amount * difficulty;
    }
    return 0.f;
}