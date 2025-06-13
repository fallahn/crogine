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

#include "ScrubAttractState.hpp"
#include "ScrubSharedData.hpp"
#include "ScrubConsts.hpp"
#include "TabData.hpp"
#include "../golf/SharedStateData.hpp"
#include "../golf/GameConsts.hpp"
#include "../Colordome-32.hpp"

#include <crogine/core/App.hpp>
#include <crogine/audio/AudioScape.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/ecs/components/ParticleEmitter.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>
#include <crogine/ecs/systems/ParticleSystem.hpp>

#include <crogine/graphics/Font.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/Wavetable.hpp>

namespace
{
    float tabScrollTime = 0.f;
}

ScrubAttractState::ScrubAttractState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedMinigameData& sc)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_sharedScrubData   (sc),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_currentTab        (0),
    m_controllerIndex   (0)
{
    addSystems();
    loadAssets();
    buildScene();
}

//public
bool ScrubAttractState::handleEvent(const cro::Event& evt)
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
            requestStackPush(StateID::ScrubGame);
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
            requestStackPush(StateID::ScrubGame);
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

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void ScrubAttractState::handleMessage(const cro::Message& msg)
{
    if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::ScrubScoresReceived)
        {
            m_highScoreText.getComponent<cro::Text>().setString(Social::getScrubScores());
        }
    }

    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool ScrubAttractState::simulate(float dt)
{
    //automatically scroll through tabs
    tabScrollTime += dt;
    if (tabScrollTime > TabDisplayTime)
    {
        //tabScrollTime -= TabDisplayTime;
        nextTab();
        m_tabs[m_currentTab].getComponent<cro::AudioEmitter>().play();
    }

    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void ScrubAttractState::render()
{
    m_scrubTexture.clear(cro::Colour::Transparent);
    m_gameScene.render();
    m_scrubTexture.display();

    m_uiScene.render();
}

//private
void ScrubAttractState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::ModelRenderer>(mb);


    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::ParticleSystem>(mb);
    m_uiScene.addSystem<cro::AudioSystem>(mb);
}

void ScrubAttractState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
    m_scrubTexture.create(1500, 1500, true, false, 2);
    m_scrubTexture.setSmooth(true);

    //load menu music
    auto id = m_resources.audio.load("assets/arcade/scrub/sound/music/menu.ogg", true);
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>().setSource(m_resources.audio.get(id));
    entity.getComponent<cro::AudioEmitter>().setLooped(true);
    entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::UserMusic);
    entity.getComponent<cro::AudioEmitter>().setVolume(0.6f);
    m_music = entity;
}

void ScrubAttractState::buildScene()
{
    buildScrubScene(); //loads the 3D model

    const auto& largeFont = m_sharedScrubData.fonts->get(sc::FontID::Title);
    const auto& smallFont = m_sharedScrubData.fonts->get(sc::FontID::Body);

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
    entity.addComponent<cro::Text>(largeFont).setString("Scrub!");
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


    //title background - remember sprites are added to the spriteNode of the current tab for correct scaling...
    auto& bgTex = m_resources.textures.get("assets/arcade/scrub/images/attract_bg.png");
    bgTex.setSmooth(true);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("menu_show");
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::UIBackgroundDepth));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(bgTex);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().depth = sc::UIBackgroundDepth;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
            progress = std::min(1.f, progress + dt);

            const auto scale = cro::Util::Easing::easeOutElastic(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            if (progress == 1)
            {
                progress = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        };

    auto bgEnt = entity;
    titleData.spriteNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //scrubber tex
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::AudioEmitter>() = as.getEmitter("menu_text");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_scrubTexture.getTexture());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().depth = 0.f;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            auto& progress = e.getComponent<cro::Callback>().getUserData<float>();
            progress = std::min(1.f, progress + dt);

            const auto scale = cro::Util::Easing::easeInCubic(progress);
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

            if (progress == 1)
            {
                progress = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        };

    auto scrubEnt = entity;
    titleData.spriteNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    titleData.onStartIn = 
        [textEnt, bgEnt, scrubEnt](cro::Entity) mutable
        {
            textEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            textEnt.getComponent<cro::Transform>().setRotation(0.f);
            textEnt.getComponent<cro::Callback>().getUserData<TitleAnimationData>().rotation = 0.f;

            bgEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            bgEnt.getComponent<cro::Callback>().getUserData<float>() = 0.f;

            scrubEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            scrubEnt.getComponent<cro::Callback>().getUserData<float>() = 0.f;
        };
    titleData.onEndIn = 
        [textEnt, bgEnt, scrubEnt](cro::Entity) mutable //to play anim
        {
            textEnt.getComponent<cro::Callback>().active = true;
            bgEnt.getComponent<cro::Callback>().active = true;
            scrubEnt.getComponent<cro::Callback>().active = true;

            scrubEnt.getComponent<cro::AudioEmitter>().play();
            bgEnt.getComponent<cro::AudioEmitter>().play();
        }; 
    //TODO titleData.onStartOut //to play exit anim


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
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 40.f };
    entity.getComponent<UIElement>().depth = sc::UIBackgroundDepth;
    howToData.spriteNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    static const cro::String StringXbox = 
        "Use " + cro::String(ButtonLB) + " to insert and " + cro::String(ButtonRB) 
        + " to remove a ball\nUse " + cro::String(ButtonLT) + "/" + cro::String(ButtonRT) + " or " + cro::String(RightStick)
        + " to scrub the ball\nPress " + cro::String(ButtonA) + " to add more soap\n\n\nPress Start to Pause the game";

    static const cro::String StringPS =
        "Use " + cro::String(ButtonL1) + " to insert and " + cro::String(ButtonR1)
        + " to remove a ball\nUse " + cro::String(ButtonL2) + "/" + cro::String(ButtonR2) + " or " + cro::String(RightStick)
        + " to scrub the ball\nPress " + cro::String(ButtonCross) + " to add more soap\n\n\nPress Start to Pause the game";

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


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(sc::SmallTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::SmallTextOffset);
    entity.getComponent<cro::Text>().setString("Hint: Longer strokes lead to cleaner balls!");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -80.f };
    entity.getComponent<UIElement>().characterSize = sc::SmallTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
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
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize * getViewScale());
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

//    std::string temp =
//R"(Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//Buns                 123248974
//
//
//Personal Best: 34590723
//)";

    //score list
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("No Scores Yet");
    entity.getComponent<cro::Text>().setCharacterSize(sc::SmallTextSize * getViewScale());
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
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize * getViewScale());
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




    const auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 6.f, 14.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(infoFont).setString("Music by David KBD");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<UIElement>().absolutePosition = { 6.f, 14.f };
    entity.getComponent<UIElement>().characterSize = InfoTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;



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

void ScrubAttractState::buildScrubScene()
{
    cro::Entity rootEnt = m_gameScene.createEntity();
    rootEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.08f, 0.f });
    rootEnt.addComponent<cro::Callback>().active = true;
    rootEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            static const auto wavetable = cro::Util::Wavetable::sine(0.3f);
            static std::size_t idx = 0;
            static constexpr float Rotation = cro::Util::Const::PI / 8.f;

            e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, Rotation * wavetable[idx]);

            idx = (idx + 1) % wavetable.size();
        };


    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/arcade/scrub/models/body.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        rootEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_body0 = entity;
    }

    if (md.loadFromFile("assets/arcade/scrub/models/body_v2.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);
        rootEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_body1 = entity;
    }

    if (md.loadFromFile("assets/arcade/scrub/models/handle.cmt"))
    {
        auto handleEnt = m_gameScene.createEntity();
        handleEnt.addComponent<cro::Transform>().setPosition({ 0.f, -0.1f, 0.f });
        md.createModel(handleEnt);
        rootEnt.getComponent<cro::Transform>().addChild(handleEnt.getComponent<cro::Transform>());
    }

    if (md.loadFromFile("assets/arcade/scrub/models/gauge_inner.cmt"))
    {
        auto gaugeEnt = m_gameScene.createEntity();
        gaugeEnt.addComponent<cro::Transform>().setPosition({ -0.10836f, -0.24753f, 0.008162f });
        md.createModel(gaugeEnt);
        gaugeEnt.getComponent<cro::Model>().setMaterialProperty(0, "u_colour", CD32::Colours[CD32::GreenLight]);
        rootEnt.getComponent<cro::Transform>().addChild(gaugeEnt.getComponent<cro::Transform>());
    }

    if (md.loadFromFile("assets/arcade/scrub/models/gauge_outer.cmt"))
    {
        auto gaugeEnt = m_gameScene.createEntity();
        gaugeEnt.addComponent<cro::Transform>();
        md.createModel(gaugeEnt);

        rootEnt.getComponent<cro::Transform>().addChild(gaugeEnt.getComponent<cro::Transform>());
    }    

    auto resize = [](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setPerspective(65.f * cro::Util::Const::degToRad, 1.f, 0.1f, 10.f);
        };

    auto camera = m_gameScene.getActiveCamera();
    auto& cam = camera.getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);

    camera.getComponent<cro::Transform>().setLocalTransform(glm::inverse(glm::lookAt(glm::vec3(-0.04f, 0.07f, 0.36f), glm::vec3(0.f, -0.04f, 0.f), cro::Transform::Y_AXIS)));

    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::X_AXIS, -1.2f);
    m_gameScene.getSunlight().getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, -0.6f);
}

void ScrubAttractState::prevTab()
{
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().hide(m_tabs[m_currentTab]);
    m_currentTab = (m_currentTab + (m_tabs.size() - 1)) % m_tabs.size();
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().show(m_tabs[m_currentTab]);

    tabScrollTime = std::max(0.f, tabScrollTime - TabDisplayTime);
}

void ScrubAttractState::nextTab()
{
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().hide(m_tabs[m_currentTab]);
    m_currentTab = (m_currentTab + 1) % m_tabs.size();
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().show(m_tabs[m_currentTab]);

    tabScrollTime = std::max(0.f, tabScrollTime - TabDisplayTime);
}

void ScrubAttractState::onCachedPush()
{
    Social::refreshScrubScore();
    
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
    m_music.getComponent<cro::AudioEmitter>().play();

    //update the keyboard help string with current key binds
    m_keyboardHelpString = "Use " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]) 
        + " to insert and " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub])
        + " to remove a ball\nUse " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left])
        + "/" + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right])
        + " to scrub the ball\nPress " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) 
        + " to add more soap\n\n\nPress ESCAPE to Pause the game";


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

void ScrubAttractState::onCachedPop()
{
    m_music.getComponent<cro::AudioEmitter>().stop();
    m_uiScene.simulate(0.f);
}