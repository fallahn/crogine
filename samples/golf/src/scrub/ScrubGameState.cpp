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

#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
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

    const std::string tempBackground = "assets/arcade/scrub/images/body_mask.png";

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
    : cro::State        (stack, context),
    m_sharedData        (sd),
    m_sharedScrubData   (sc),
    m_soundDirector     (nullptr),
    m_gameScene         (context.appInstance.getMessageBus()),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_axisPosition      (0)
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
                m_ball.scrub(m_handle.switchDirection(Handle::Down));
                m_soundDirector->playSound(AudioID::FXScrubDown, MixerChannel::Effects, 0.4f).getComponent<cro::AudioEmitter>().setPitch(cro::Util::Random::value(0.85f, 1.2f));
            }
        };
    static const auto pumpUp = 
        [&]()
        {
            if (m_score.gameRunning)
            {
                m_ball.scrub(m_handle.switchDirection(Handle::Up));
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
                    m_handle.soap.count--;
                    m_handle.soap.refresh();
                    m_soundDirector->playSound(AudioID::FXFillSoap, MixerChannel::Menu);
                }
                else
                {
                    m_soundDirector->playSound(AudioID::FXNoSoap, MixerChannel::Menu);
                }
            }
        };

    static const auto pause = [&]() 
        {
            //TODO pause music
            requestStackPush(StateID::ScrubPause);
        };

    static const auto quit = [&]() 
        {
            if (!m_score.gameRunning
                && m_score.quitTimeout > 2.f)
            {
                requestStackClear();
                requestStackPush(StateID::ScrubBackground);
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
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        //TODO again do we want to lock the controller ID?
        //will having two controllers somehow be an advantage?
        if (evt.caxis.axis == cro::GameController::AxisRightX)
        {
            static constexpr std::int16_t Threshold = std::numeric_limits<std::int16_t>::max() / 2;
            const auto v = evt.caxis.value;

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
    }
    else if (evt.type == SDL_CONTROLLERDEVICEREMOVED)
    {
        pause();
    }

    else if (evt.type == SDL_KEYUP
        || evt.type == SDL_CONTROLLERBUTTONUP)
    {
        quit();

#ifdef CRO_DEBUG_
        if (evt.type == SDL_KEYUP
            && evt.key.keysym.sym == SDLK_l)
        {
            m_score.remainingTime = 0;
        }
#endif
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


        m_score.totalRunTime += dt;
#ifndef CRO_DEBUG_
        m_score.remainingTime = std::max(m_score.remainingTime - dt, 0.f);
#endif
        m_score.quitTimeout = 0.f;
        if (m_score.remainingTime == 0)
        {
            m_score.gameRunning = false;
            m_score.totalScore += static_cast<std::int32_t>(std::floor(m_score.avgCleanliness));
            m_score.totalScore += static_cast<std::int32_t>(std::floor(m_score.totalRunTime));

            //game over, show scores.
            const auto& font = m_sharedScrubData.fonts->get(sc::FontID::Title);
            const auto animIn = [](cro::Entity e, float dt)
                {
                    const auto Speed = dt * 3.f;
                    auto& currTime =  e.getComponent<cro::Callback>().getUserData<float>();
                    currTime = std::min(1.f, currTime + Speed);
                    const auto scale = cro::Util::Easing::easeOutElastic(std::max(0.f, currTime));
                    e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                    if (currTime == 1)
                    {
                        e.getComponent<cro::Callback>().active = false;
                    }
                };

            glm::vec2 size(cro::App::getWindow().getSize());
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f, sc::TextDepth });
            entity.getComponent<cro::Transform>().move({ 0.f, 64.f * getViewScale()});
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setString("Game Over");
            entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setShadowOffset(sc::LargeTextOffset);
            entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
            entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
            entity.getComponent<UIElement>().absolutePosition = { 0.f, 64.f };
            entity.getComponent<UIElement>().characterSize = sc::LargeTextSize;
            entity.getComponent<UIElement>().depth = sc::TextDepth;

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(0.f);
            entity.getComponent<cro::Callback>().function = animIn;

            attachText(entity);

            const auto& font2 = m_sharedScrubData.fonts->get(sc::FontID::Body);
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y / 2.f, sc::TextDepth });
            entity.getComponent<cro::Transform>().move({ 0.f, -14.f * getViewScale() });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font2).setString("Total Score: " + std::to_string(m_score.totalScore) + "\n\nPress Any Button");
            entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
            entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
            entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
            entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
            entity.getComponent<UIElement>().absolutePosition = { 0.f, -14.f };
            entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
            entity.getComponent<UIElement>().depth = sc::TextDepth;

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().setUserData<float>(-0.5f);
            entity.getComponent<cro::Callback>().function = animIn;

            attachText(entity);

            
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

            m_soundDirector->playSound(AudioID::VORoundEnd, MixerChannel::Voice);
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

    m_gameScene.render();
    m_uiScene.render();
}

//private
void ScrubGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ShadowMapRenderer>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);
    m_gameScene.addSystem<cro::AudioSystem>(mb);

    m_soundDirector = m_gameScene.addDirector<ScrubSoundDirector>();

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
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
    //TODO load music
}

void ScrubGameState::createScene()
{
    cro::Entity rootNode = m_gameScene.createEntity();
    rootNode.addComponent<cro::Transform>().setScale({ 0.f, 0.f, 0.f });
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<float>(3.f);
    rootNode.getComponent<cro::Callback>().function =
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

    if (md.loadFromFile("assets/arcade/scrub/models/ball.cmt"))
    {
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
        cam.setPerspective(70.f * cro::Util::Const::degToRad, size.x / size.y, 0.1f, 10.f);
    };

    auto camera = m_gameScene.getActiveCamera();
    auto& cam = camera.getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    cam.shadowMapBuffer.create(2048, 2048);
    cam.setMaxShadowDistance(2.f);
    cam.setShadowExpansion(0.5f);

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


    camera.getComponent<cro::Transform>().setLocalTransform(path[0]);
    camera.addComponent<cro::Callback>().active = true;
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

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -1.2f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.6f);
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
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { 12.f, 16.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            std::stringstream ss;
            ss.setf(std::ios::fixed);
            ss.precision(2);
            ss << "Avg Cleanliness: " << m_score.avgCleanliness << "%";
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
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -16.f };
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
    entity.getComponent<UIElement>().absolutePosition = { -212.f, -12.f };
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

    static constexpr float BarHeight = 800.f;
    static constexpr float BarWidth = 180.f;

    //TODO replace this with proper bg
#ifdef HIDE_BACKGROUND
    auto& bgTex = m_resources.textures.get(tempBackground);
#else
    auto& bgTex = m_sharedScrubData.backgroundTexture->getTexture();
    //auto& bgTex = m_resources.textures.get("assets/images/startup.png");
#endif
    //100% streak
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BarWidth), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(0.f, 0.f), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(BarHeight, BarWidth), cro::Colour::Red),
            cro::Vertex2D(glm::vec2(BarHeight, 0.f), cro::Colour::Red)
        });
    /*entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(sc::ShaderID::LevelMeter));
    entity.getComponent<cro::Drawable2D>().setTexture(&bgTex);*/
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 1.f);
    entity.getComponent<UIElement>().absolutePosition = { -BarHeight / 2.f, -(BarWidth + 12.f) };
    //entity.getComponent<UIElement>().resizeCallback = std::bind(&ScrubGameState::levelMeterCallback, this, std::placeholders::_1);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
            const float target = static_cast<float>(std::min(10, m_score.bonusRun)) / 10.f;

            const float Speed = dt * 2.f;
            if (progress < target)
            {
                progress = std::min(target, progress + Speed);
            }
            else if (progress > target)
            {
                progress = std::max(target, progress - Speed);
            }
            
            cro::FloatRect cropping = { 0.f, 0.f, BarHeight * progress, BarWidth };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);

            auto colour = cro::Colour::Red;
            colour.setGreen(progress);
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour = colour;
            }
        };
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //soap level
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BarHeight), SoapMeterColour),
            cro::Vertex2D(glm::vec2(0.f), SoapMeterColour),
            cro::Vertex2D(glm::vec2(BarWidth, BarHeight), SoapMeterColour),
            cro::Vertex2D(glm::vec2(BarWidth, 0.f), SoapMeterColour)
        }
    );
    entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(sc::ShaderID::LevelMeter));
    entity.getComponent<cro::Drawable2D>().setTexture(&bgTex);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(1.f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { -(BarWidth + 12.f), 12.f};
    entity.getComponent<UIElement>().resizeCallback = std::bind(&ScrubGameState::levelMeterCallback, this, std::placeholders::_1);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(1.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            const float speed = dt;
            auto& currPos = e.getComponent<cro::Callback>().getUserData<float>();
            const auto target = std::clamp((m_handle.soap.amount - Handle::Soap::MinSoap) / (Handle::Soap::MaxSoap - Handle::Soap::MinSoap), 0.f, 1.f);

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
            cro::Colour c = glm::mix(glm::vec4(1.f), SoapMeterColour.getVec4(), currPos);
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour = c;
            }

            //so let's crop the drawable instead
            cro::FloatRect cropping = { 0.f, 0.f, BarWidth, BarHeight * currPos };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        };
    m_spriteRoot.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //current ball cleanliness - TODO make this a generic level meter so we can
    //recycle it with other levels on screen (and use the glass tube effect)
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, BarHeight), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(BarWidth, BarHeight), cro::Colour::Blue),
            cro::Vertex2D(glm::vec2(BarWidth, 0.f), cro::Colour::Blue)
        }
    );
    entity.getComponent<cro::Drawable2D>().setShader(&m_resources.shaders.get(sc::ShaderID::LevelMeter));
    entity.getComponent<cro::Drawable2D>().setTexture(&bgTex);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(1.f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { -((BarWidth + 12.f) * 2.f), 12.f };
    entity.getComponent<UIElement>().resizeCallback = std::bind(&ScrubGameState::levelMeterCallback, this, std::placeholders::_1);
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
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
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

    if (m_handle.progress == 0 || m_handle.progress == 1)
    {
        if (m_handle.speed != 0)
        {
            m_ball.scrub(m_handle.calcStroke());
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
            updateScore();

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

void ScrubGameState::updateScore()
{
    const float cleanliness = Ball::MaxFilth - m_ball.filth;

    if (cleanliness < m_score.threshold)
    {
        //display notification
        const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
        const auto size = glm::vec2(cro::App::getWindow().getSize());

        auto ent = m_uiScene.createEntity();
        ent.addComponent<cro::Transform>().setPosition({ size.x / 2.f, 40.f });
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Text>(font).setCharacterSize(8 * 3);
        ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        ent.getComponent<cro::Text>().setFillColour(cro::Colour::Red);
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
                    e.getComponent<cro::Text>().setFillColour(cro::Colour::Red);
                }
                else
                {
                    e.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
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
            showMessage("STREAK BROKEN");
            m_soundDirector->playSound(AudioID::FXStreakBroken, MixerChannel::Effects);
        }
        else
        {
            m_soundDirector->playSound(AudioID::FXPremature, MixerChannel::Effects);
        }
        m_score.bonusRun = 0;
        
        return;
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
        default: 
            //every 10 after
            if (m_score.bonusRun % 10 == 0)
            {
                m_score.totalScore += 10000;
                m_score.remainingTime += 10.f;

                showMessage("10x STREAK!");
                showMessage("+10000");
                showMessage("+10s");

                m_soundDirector->playSound(AudioID::VOTenX, MixerChannel::Voice);
                m_soundDirector->playSound(AudioID::FXTenX, MixerChannel::Effects, 0.6f);
            }
            else
            {
                showMessage("PERFECT");
            }
            break;
        case 3:
            m_score.totalScore += 3000;
            m_score.remainingTime += 0.5f;

            showMessage("3x STREAK!");
            showMessage("+3000");

            m_soundDirector->playSound(AudioID::VOThreeX, MixerChannel::Voice);
            m_soundDirector->playSound(AudioID::FXThreeX, MixerChannel::Effects, 0.6f);
            break;
        case 5:
            m_score.totalScore += 5000;
            m_score.remainingTime += 2.f;

            showMessage("5x STREAK!");
            showMessage("+5000");
            showMessage("+2s");

            m_soundDirector->playSound(AudioID::VOFiveX, MixerChannel::Voice);
            m_soundDirector->playSound(AudioID::FXFiveX, MixerChannel::Effects, 0.6f);
            break;
        }
    }
    else
    {
        if (m_score.bonusRun != 0)
        {
            showMessage("STREAK BROKEN");
            m_soundDirector->playSound(AudioID::FXStreakBroken, MixerChannel::Effects);
        }
        m_score.bonusRun = 0;
    }

    //this might happen just as the time runs out - we want to
    //keep the score but not add time in this case
    if (m_score.gameRunning)
    {
        float bonus = Score::TimeBonus * (cleanliness / 100.f);
        m_score.remainingTime += bonus;
        
        if (bonus != 0)
        {
            std::stringstream ss;
            ss.precision(2);
            ss.setf(std::ios::fixed);
            ss << "+" << bonus << "s";

            showMessage(ss.str());
        }
        const float bonusProgress = static_cast<float>(m_score.bonusRun) / 10.f;
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



    if (m_score.ballsWashed % /*(m_score.bonusRun > 4 ? 3 : 5)*/4 == 0) //TODO we need to fix this so we can't get gaps if we break a streak
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
                const float scale = cro::Util::Easing::easeInElastic(std::max(0.f, std::min(1.f, currTime / (AnimateTime*2.f))));
                const float oProgress = 1.f - scale;

                const auto oTarget = (winSize - glm::vec2(20.f)) - basePos;
                pos += (oTarget * oProgress);
                e.getComponent<cro::Transform>().setPosition(pos);

                e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                if (currTime < 0)
                {
                    m_score.totalScore += 500;
                    m_soundDirector->playSound(AudioID::FXSoapAdded, MixerChannel::Effects);

                    m_handle.soap.count = std::min(MaxSoapBars, m_handle.soap.count + 1);
                    e.getComponent<cro::Callback>().active = false;
                    m_uiScene.destroyEntity(e);
                }
            };

        m_score.threshold = std::min(Ball::MaxFilth, m_score.threshold + 4.f);
        m_score.remainingTime += 0.5f;

        showMessage("+0.5s");
        m_soundDirector->playSound(AudioID::VONewSoap, MixerChannel::Voice);
    }
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
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize);
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

    //TODO set the texture on the drawable
}

//handle funcs
float ScrubGameState::Handle::switchDirection(float d)
{
    CRO_ASSERT(d == Down || d == Up, "");

    float ret = 0.f;
    if (d != direction
        && !locked)
    {
        //do this first as it uses the current direction
        ret = calcStroke();

        direction = d;
        speed = MaxSpeed;

        if (hasBall)
        {
            soap.amount = std::max(Soap::MinSoap, soap.amount - soap.getReduction());
        }
    }
    return ret;
}

float ScrubGameState::Handle::calcStroke()
{
    const float currPos = entity.getComponent<cro::Transform>().getPosition().y;
    stroke = ((currPos - strokeStart) / StrokeDistance) * direction;
    strokeStart = currPos;

    if (hasBall)
    {
        return cro::Util::Easing::easeInQuad(stroke) * soap.amount;
    }
    return 0.f;
}