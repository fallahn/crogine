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

#include "../golf/GameConsts.hpp"
#include "../scrub/ScrubConsts.hpp"
#include "../scrub/ScrubSharedData.hpp"
#include "../scrub/TabData.hpp"
#include "SBallAttractState.hpp"
#include "SBallSoundDirector.hpp"

#include <crogine/audio/AudioScape.hpp>

#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/util/Easings.hpp>

namespace
{
    float tabScrollTime = 0.f;
    std::int32_t lastInput = InputType::Keyboard;
}

SBallAttractState::SBallAttractState(cro::StateStack& ss, cro::State::Context ctx, const SharedStateData& ssd, SharedMinigameData& sd)
    : cro::State        (ss, ctx),
    m_sharedData        (ssd),
    m_sharedGameData    (sd),
    m_uiScene           (cro::App::getInstance().getMessageBus(), 512),
    m_controllerIndex   (0),
    m_currentTab        (0)
{
    loadAssets();
    addSystems();
    buildScene();
}

//public
bool SBallAttractState::handleEvent(const cro::Event& evt)
{
    const auto quit = [&]()
        {
            requestStackClear();
            requestStackPush(StateID::Clubhouse);
        };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_SPACE:
            requestStackPop();
            requestStackPush(StateID::SBallGame);
            break;
        case SDLK_LEFT:
            prevTab();
            m_tabs[m_currentTab].getComponent<cro::AudioEmitter>().play();
            break;
        case SDLK_RIGHT:
            nextTab();
            m_tabs[m_currentTab].getComponent<cro::AudioEmitter>().play();
            break;
        case SDLK_BACKSPACE:
#ifndef CRO_DEBUG_
        case SDLK_ESCAPE:
#endif
            quit();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
            quit();
            break;
        case cro::GameController::ButtonA:
            requestStackPop();
            requestStackPush(StateID::SBallGame);
            break;
        case cro::GameController::ButtonLeftShoulder:
        case cro::GameController::DPadLeft:
            prevTab();
            m_tabs[m_currentTab].getComponent<cro::AudioEmitter>().play();
            break;
        case cro::GameController::ButtonRightShoulder:
        case cro::GameController::DPadRight:
            nextTab();
            m_tabs[m_currentTab].getComponent<cro::AudioEmitter>().play();
            break;
        }
        m_controllerIndex = cro::GameController::controllerID(evt.cbutton.which);
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value < -cro::GameController::LeftThumbDeadZone
            || evt.caxis.value > cro::GameController::LeftThumbDeadZone)
        {
            m_controllerIndex = cro::GameController::controllerID(evt.caxis.which);
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }


    m_uiScene.forwardEvent(evt);
    return false;
}

void SBallAttractState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::SBallScoresReceived)
        {
            m_highScoreText.getComponent<cro::Text>().setString(Social::getSBallScores());
        }
    }

    m_uiScene.forwardMessage(msg);
}

bool SBallAttractState::simulate(float dt)
{
    //automatically scroll through tabs
    tabScrollTime += dt;
    if (tabScrollTime > TabDisplayTime)
    {
        //tabScrollTime -= TabDisplayTime;
        nextTab();
        m_tabs[m_currentTab].getComponent<cro::AudioEmitter>().play();
    }

    m_uiScene.simulate(dt);
    return true;
}

void SBallAttractState::render()
{
    m_uiScene.render();
}

//private
void SBallAttractState::loadAssets()
{

}

void SBallAttractState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();

    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    //m_uiScene.addSystem<cro::UIElementSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::ParticleSystem>(mb);
    m_uiScene.addSystem<cro::AudioSystem>(mb);

    m_uiScene.addDirector<SBallSoundDirector>();
}

void SBallAttractState::buildScene()
{
    const auto& largeFont = m_sharedGameData.fonts->get(sc::FontID::Title);
    const auto& smallFont = m_sharedGameData.fonts->get(sc::FontID::Body);

    auto size = glm::vec2(cro::App::getWindow().getSize());

    cro::AudioScape as;
    as.loadFromFile("assets/arcade/scrub/sound/menu.xas", m_resources.audio);


    //title tab
    m_tabs[TabID::Title] = m_uiScene.createEntity();
    m_tabs[TabID::Title].addComponent<cro::Transform>();
    m_tabs[TabID::Title].addComponent<cro::AudioEmitter>() = as.getEmitter("next");
    m_tabs[TabID::Title].addComponent<cro::Callback>().active = true;
    m_tabs[TabID::Title].getComponent<cro::Callback>().setUserData<TabData>();
    m_tabs[TabID::Title].getComponent<cro::Callback>().function = std::bind(&processTab, std::placeholders::_1, std::placeholders::_2);

    auto& titleData = m_tabs[TabID::Title].getComponent<cro::Callback>().getUserData<TabData>();
    titleData.spriteNode = m_uiScene.createEntity();
    titleData.spriteNode.addComponent<cro::Transform>();
    m_tabs[TabID::Title].getComponent<cro::Transform>().addChild(titleData.spriteNode.getComponent<cro::Transform>());

    struct TitleAnimationData final
    {
        float rotation = 0.f;
        const float TargetRotation = cro::Util::Const::TAU * 5.02f;
    };

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("menu_boink");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Sports Ball");
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize * getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::LargeTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { -60.f, 160.f };
    entity.getComponent<UIElement>().characterSize = sc::LargeTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<TitleAnimationData>();
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<TitleAnimationData>();
            data.rotation = std::min(data.TargetRotation, data.rotation + (dt * 30.f));

            e.getComponent<cro::Transform>().setRotation(data.rotation);

            const auto progress = std::clamp(data.rotation / data.TargetRotation, 0.f, 1.f);
            const auto scale = cro::Util::Easing::easeInElastic(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            if (data.rotation == data.TargetRotation)
            {
                e.getComponent<cro::Callback>().active = false;
                e.getComponent<cro::AudioEmitter>().play();
            }
        };

    auto textEnt = entity;
    m_tabs[TabID::Title].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    titleData.onStartIn =
        [textEnt/*, bgEnt, scrubEnt*/](cro::Entity) mutable
        {
            textEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            textEnt.getComponent<cro::Transform>().setRotation(0.f);
            textEnt.getComponent<cro::Callback>().getUserData<TitleAnimationData>().rotation = 0.f;

            /*bgEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            bgEnt.getComponent<cro::Callback>().getUserData<float>() = 0.f;

            scrubEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            scrubEnt.getComponent<cro::Callback>().getUserData<float>() = 0.f;*/
        };
    titleData.onEndIn =
        [textEnt/*, bgEnt, scrubEnt*/](cro::Entity) mutable //to play anim
        {
            textEnt.getComponent<cro::Callback>().active = true;
            /*bgEnt.getComponent<cro::Callback>().active = true;
            scrubEnt.getComponent<cro::Callback>().active = true;

            scrubEnt.getComponent<cro::AudioEmitter>().play();
            bgEnt.getComponent<cro::AudioEmitter>().play();*/
        };




    //how to play tab
    m_tabs[TabID::HowTo] = m_uiScene.createEntity();
    m_tabs[TabID::HowTo].addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_tabs[TabID::HowTo].addComponent<cro::AudioEmitter>() = as.getEmitter("next");
    m_tabs[TabID::HowTo].addComponent<cro::Callback>().active = true;
    m_tabs[TabID::HowTo].getComponent<cro::Callback>().setUserData<TabData>();
    m_tabs[TabID::HowTo].getComponent<cro::Callback>().function = std::bind(&processTab, std::placeholders::_1, std::placeholders::_2);

    auto& howToData = m_tabs[TabID::HowTo].getComponent<cro::Callback>().getUserData<TabData>();
    howToData.spriteNode = m_uiScene.createEntity();
    howToData.spriteNode.addComponent<cro::Transform>();
    m_tabs[TabID::HowTo].getComponent<cro::Transform>().addChild(howToData.spriteNode.getComponent<cro::Transform>());


    //help background
    auto& helpTex = m_resources.textures.get("assets/arcade/scrub/images/rules_bg.png");
    helpTex.setSmooth(true);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::UIBackgroundDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(helpTex);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 40.f };
    entity.getComponent<UIElement>().depth = sc::UIBackgroundDepth;
    howToData.spriteNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    static const cro::String StringXbox =
        "Use " + cro::String(LeftStick) + " to Aim.\nPress " + cro::String(ButtonA) + " to drop the ball.\n"
        + "Match two balls to evolve them.\nMatch two Beach balls to level up.\nThe game ends when any\nball reaches the top!!";
        

    static const cro::String StringPS =
        "Use " + cro::String(LeftStick) + " to Aim.\nPress " + cro::String(ButtonCross) + " to drop the ball.\n"
        + "Match two balls to evolve them.\nMatch two Beach balls to level up.\nThe game ends when any\nball reaches the top!!";

    //help text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(sc::MediumTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.getComponent<cro::Text>().setString(StringXbox); //set this once so we can approximate the local bounsd
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, std::floor(bounds.height / 2.f) + 20.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (cro::GameController::getControllerCount()
                || Social::isSteamdeck())
            {
                e.getComponent<cro::Text>().setString(cro::GameController::hasPSLayout(m_controllerIndex) ? StringPS : StringXbox);
            }
            else
            {
                //TODO read keybinds and update string as necessary
                e.getComponent<cro::Text>().setString(m_keyboardHelpString);
            }
        };

    m_tabs[TabID::HowTo].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //high scores tab (or personal best in non-steam)
    m_tabs[TabID::Scores] = m_uiScene.createEntity();
    m_tabs[TabID::Scores].addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_tabs[TabID::Scores].addComponent<cro::AudioEmitter>() = as.getEmitter("next");
    m_tabs[TabID::Scores].addComponent<cro::Callback>().active = true;
    m_tabs[TabID::Scores].getComponent<cro::Callback>().setUserData<TabData>();
    m_tabs[TabID::Scores].getComponent<cro::Callback>().function = std::bind(&processTab, std::placeholders::_1, std::placeholders::_2);

    auto& scoreData = m_tabs[TabID::Scores].getComponent<cro::Callback>().getUserData<TabData>();
    scoreData.spriteNode = m_uiScene.createEntity();
    scoreData.spriteNode.addComponent<cro::Transform>();
    m_tabs[TabID::Scores].getComponent<cro::Transform>().addChild(scoreData.spriteNode.getComponent<cro::Transform>());


    //score background
    auto& scoreTex = m_resources.textures.get("assets/arcade/scrub/images/highscore_bg.png");
    scoreTex.setSmooth(true);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::UIBackgroundDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(scoreTex);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 76.f };
    entity.getComponent<UIElement>().depth = sc::UIBackgroundDepth;
    scoreData.spriteNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //score text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("High Scores");
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize* getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 150.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

    m_tabs[TabID::Scores].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //score list
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("No Scores Yet");
    entity.getComponent<cro::Text>().setCharacterSize(sc::SmallTextSize* getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 100.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    m_highScoreText = entity;
    m_tabs[TabID::Scores].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //start message
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Press Space To Start");
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize* getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f, 0.f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 50.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& t = e.getComponent<cro::Callback>().getUserData<float>();
            t += dt;

            auto f = static_cast<std::int32_t>(std::floor(t));
            if (f % 2 == 0)
            {
                e.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
                e.getComponent<cro::Text>().setShadowColour(cro::Colour::Transparent);
            }
            else
            {
                e.getComponent<cro::Text>().setFillColour(TextNormalColour);
                e.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            }

            if (cro::GameController::getControllerCount()
                || Social::isSteamdeck())
            {
                if (cro::GameController::hasPSLayout(m_controllerIndex))
                {
                    e.getComponent<cro::Text>().setString("Press " + cro::String(ButtonCross) + " To Start, " + cro::String(ButtonCircle) + " To Quit");
                }
                else
                {
                    e.getComponent<cro::Text>().setString("Press " + cro::String(ButtonA) + " To Start, " + cro::String(ButtonB) + " To Quit");
                }
            }
            else
            {
                e.getComponent<cro::Text>().setString("Press SPACE To Start, ESCAPE To Quit");
            }
        };



    auto resize = [&](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);

            const auto Scale = getViewScale();
            const auto SpriteScale = Scale / MaxSpriteScale;
            for (auto t : m_tabs)
            {
                auto& data = t.getComponent<cro::Callback>().getUserData<TabData>();
                data.spriteNode.getComponent<cro::Transform>().setScale(glm::vec2(SpriteScale));
            }

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

                    if (ui.characterSize)
                    {
                        e.getComponent<cro::Text>().setCharacterSize(ui.characterSize * Scale);
                        e.getComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2((ui.absolutePosition * Scale) + glm::vec2(x, y)), ui.depth));
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

void SBallAttractState::prevTab()
{
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().hide(m_tabs[m_currentTab]);
    m_currentTab = (m_currentTab + (m_tabs.size() - 1)) % m_tabs.size();
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().show(m_tabs[m_currentTab]);

    tabScrollTime = std::max(0.f, tabScrollTime - TabDisplayTime);
}

void SBallAttractState::nextTab()
{
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().hide(m_tabs[m_currentTab]);
    m_currentTab = (m_currentTab + 1) % m_tabs.size();
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().show(m_tabs[m_currentTab]);

    tabScrollTime = std::max(0.f, tabScrollTime - TabDisplayTime);
}

void SBallAttractState::onCachedPush()
{
    Social::refreshSBallScore();
    m_sharedGameData.score.personalBest = Social::getSBallPB();

    //reset to default tab
    prevTab(); //this just tidies up existing tab before forcing the index below
    m_tabs[TabID::HowTo].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_tabs[TabID::Scores].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_currentTab = m_tabs.size() - 1;
    nextTab();

    //reset tab change timer
    tabScrollTime = 0.f;

    //reset the music volume
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& ct = e.getComponent<cro::Callback>().getUserData<float>();
            ct = std::min(1.f, ct + dt);

            cro::AudioMixer::setPrefadeVolume(ct, MixerChannel::UserMusic);

            if (ct == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };

    cro::AudioMixer::setPrefadeVolume(0.f, MixerChannel::UserMusic);
    //m_music.getComponent<cro::AudioEmitter>().play();
    //LogI << "Implment onCachedPush" << std::endl;
    //update the keyboard help string with current key binds
    m_keyboardHelpString = "Use " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left])
        + " and " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]) + " to Aim.\nPress "
        + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " to drop the ball.\n"
        + "Match two balls to evolve them.\nMatch two Beach balls to level up.\nThe game ends when any\nball reaches the top!!";
}

void SBallAttractState::onCachedPop()
{
    //LogI << "Implement onCachedPop" << std::endl;
    //m_music.getComponent<cro::AudioEmitter>().stop();
    //m_uiScene.simulate(0.f);
}