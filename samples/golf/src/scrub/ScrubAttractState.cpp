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
#include "../golf/SharedStateData.hpp"
#include "../golf/GameConsts.hpp"
#include "../Colordome-32.hpp"

#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/graphics/Font.hpp>
#include <crogine/detail/OpenGL.hpp>
#include <crogine/util/Wavetable.hpp>

namespace
{
    float tabScrollTime = 0.f;
    constexpr float TabDisplayTime = 6.f;

    struct TabData final
    {
        enum
        {
            In, Hold, Out
        };
        std::int32_t state = Hold;

        std::function<void(cro::Entity)> onStartIn;
        std::function<void(cro::Entity)> onEndIn;
        std::function<void(cro::Entity)> onStartOut;
        std::function<void(cro::Entity)> onEndOut;

        //all sprites are parented to this so they can be scaled to fit the screen
        cro::Entity spriteNode; 

        void show(cro::Entity e)
        {
            state = In;
            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

            const auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::Transform>().setPosition({ size.x, 0.f });

            if (onStartIn)
            {
                onStartIn(e);
            }
        }

        void hide(cro::Entity e)
        {
            state = Out;
            if (onStartOut)
            {
                onStartOut(e);
            }
        }
    };

    void processTab(cro::Entity e, float dt)
    {
        const auto size = glm::vec2(cro::App::getWindow().getSize());
        auto& data = e.getComponent<cro::Callback>().getUserData<TabData>();
        const auto Speed = dt * size.x * 8.f;

        switch (data.state)
        {
        default:
            break;
        case TabData::In:
        {
            //Y should always be set, but if for some reason the res
            //changes mid-transition we update that too
            //the UIElement property always expects elements to be relative to 0,0
            //so tabs move from the bottom left of the screen
            const auto target = glm::vec2(0.f);
            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.x = std::max(pos.x - Speed, target.x);
            pos.y = std::max(pos.y - Speed, target.y);
            e.getComponent<cro::Transform>().setPosition(pos);

            if (pos.x == target.x && pos.y == target.y)
            {
                data.state = TabData::Hold;
                if (data.onEndIn)
                {
                    data.onEndIn(e);
                }
            }
        }
            break;
        case TabData::Out:
        {
            const auto target = glm::vec2(-size.x, 0.f);
            auto pos = e.getComponent<cro::Transform>().getPosition();
            pos.x = std::max(pos.x - Speed, target.x);
            pos.y = std::max(pos.y - Speed, target.y);
            e.getComponent<cro::Transform>().setPosition(pos);

            if (pos.x == target.x && pos.y == target.y)
            {
                data.state = TabData::Hold;
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                if (data.onEndOut)
                {
                    data.onEndOut(e);
                }
            }
        }
            break;
        }
    }
}

ScrubAttractState::ScrubAttractState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedScrubData& sc)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_sharedScrubData   (sc),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_gameScene         (ctx.appInstance.getMessageBus()),
    m_currentTab        (0)
{
    addSystems();
    loadAssets();
    buildScene();
}

//public
bool ScrubAttractState::handleEvent(const cro::Event& evt)
{
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
            break;
        case SDLK_RIGHT:
            nextTab();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonA:
            requestStackPop();
            requestStackPush(StateID::ScrubGame);
            break;
        case cro::GameController::ButtonLeftShoulder:
        case cro::GameController::DPadLeft:
            prevTab();
            break;
        case cro::GameController::ButtonRightShoulder:
        case cro::GameController::DPadRight:
            nextTab();
            break;
        }
    }


    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void ScrubAttractState::handleMessage(const cro::Message& msg)
{
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
    m_uiScene.addSystem<cro::AudioSystem>(mb);
}

void ScrubAttractState::loadAssets()
{
    m_environmentMap.loadFromFile("assets/images/hills.hdr");
    m_scrubTexture.create(1500, 1500);

    //load menu music
    auto id = m_resources.audio.load("assets/arcade/scrub/sound/music/menu.ogg", true);
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>().setSource(m_resources.audio.get(id));
    entity.getComponent<cro::AudioEmitter>().setLooped(true);
    entity.getComponent<cro::AudioEmitter>().setMixerChannel(MixerChannel::Music);
    entity.getComponent<cro::AudioEmitter>().setVolume(0.5f);
    m_music = entity;
}

void ScrubAttractState::buildScene()
{
    buildScrubScene(); //loads the 3D model

    const auto& largeFont = m_sharedScrubData.fonts->get(sc::FontID::Title);
    const auto& smallFont = m_sharedScrubData.fonts->get(sc::FontID::Body);

    auto size = glm::vec2(cro::App::getWindow().getSize());


    //title tab
    m_tabs[TabID::Title] = m_uiScene.createEntity();
    m_tabs[TabID::Title].addComponent<cro::Transform>();
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
            }
        };

    auto textEnt = entity;
    m_tabs[TabID::Title].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    static constexpr float Radius = 680.f;
    static constexpr std::int32_t PointCount = 48;
    static constexpr float ArcSize = cro::Util::Const::TAU / PointCount;
    std::vector<cro::Vertex2D> verts;
    verts.emplace_back(glm::vec2(0.f), CD32::Colours[CD32::Brown]);
    for (auto i = 0; i < PointCount; ++i)
    {
        glm::vec2 p = glm::vec2(std::cos(i * ArcSize), std::sin(i * ArcSize)) * Radius;
        verts.emplace_back(p, CD32::Colours[CD32::Brown]);
    }
    verts.push_back(verts[1]);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::UIBackgroundDepth));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setVertexData(verts);
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_FAN);
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


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 0.f));
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_scrubTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
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
        }; 
    //TODO titleData.onStartOut //to play exit anim


    //how to play tab
    m_tabs[TabID::HowTo] = m_uiScene.createEntity();
    m_tabs[TabID::HowTo].addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_tabs[TabID::HowTo].addComponent<cro::Callback>().active = true;
    m_tabs[TabID::HowTo].getComponent<cro::Callback>().setUserData<TabData>();
    m_tabs[TabID::HowTo].getComponent<cro::Callback>().function = std::bind(&processTab, std::placeholders::_1, std::placeholders::_2);

    auto& howToData = m_tabs[TabID::HowTo].getComponent<cro::Callback>().getUserData<TabData>();
    howToData.spriteNode = m_uiScene.createEntity();
    howToData.spriteNode.addComponent<cro::Transform>();
    m_tabs[TabID::HowTo].getComponent<cro::Transform>().addChild(howToData.spriteNode.getComponent<cro::Transform>());

    static const std::string controllerStr =
        R"(
Use right thumb stick to scrub
Use LB to insert and RB to remove a ball
Press A to add more soap

Press Start to Pause the game.
)";

    static const std::string keyboardStr =
        R"(
Use A/D to scrub
Use Q to insert and E to remove a ball
Press SPACE to add more soap

Press ESCAPE to Pause the game.
)";

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(sc::MediumTextSize/* * getViewScale()*/);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.getComponent<cro::Text>().setString(controllerStr); //set this once so we can approximate the local bounsd
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, std::floor(bounds.height / 2.f) + 40.f }; //hm this doesn't account for the fact that the bounds will be bigger with a bigger text size. We need to constify this
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            if (cro::GameController::getControllerCount()
                || Social::isSteamdeck())
            {
                //TODO use controller appropriate icons
                e.getComponent<cro::Text>().setString(controllerStr);
            }
            else
            {
                //TODO read keybinds and update string as necessary
                e.getComponent<cro::Text>().setString(keyboardStr);
            }
        };

    m_tabs[TabID::HowTo].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //high scores tab (or personal best in non-steam)
    m_tabs[TabID::Scores] = m_uiScene.createEntity();
    m_tabs[TabID::Scores].addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    m_tabs[TabID::Scores].addComponent<cro::Callback>().active = true;
    m_tabs[TabID::Scores].getComponent<cro::Callback>().setUserData<TabData>();
    m_tabs[TabID::Scores].getComponent<cro::Callback>().function = std::bind(&processTab, std::placeholders::_1, std::placeholders::_2);

    auto& scoreData = m_tabs[TabID::Scores].getComponent<cro::Callback>().getUserData<TabData>();
    scoreData.spriteNode = m_uiScene.createEntity();
    scoreData.spriteNode.addComponent<cro::Transform>();
    m_tabs[TabID::Scores].getComponent<cro::Transform>().addChild(scoreData.spriteNode.getComponent<cro::Transform>());

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
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 60.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

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
        [](cro::Entity e, float dt)
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
                e.getComponent<cro::Text>().setString("Press A To Start");
            }
            else
            {
                e.getComponent<cro::Text>().setString("Press SPACE To Start");
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

void ScrubAttractState::buildScrubScene()
{
    cro::ModelDefinition md(m_resources, &m_environmentMap);
    if (md.loadFromFile("assets/arcade/scrub/models/body.cmt"))
    {
        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.08f, 0.f });
        md.createModel(entity);

        if (md.loadFromFile("assets/arcade/scrub/models/handle.cmt"))
        {
            auto handleEnt = m_gameScene.createEntity();
            handleEnt.addComponent<cro::Transform>().setPosition({ 0.f, -0.1f, 0.f });
            md.createModel(handleEnt);
            entity.getComponent<cro::Transform>().addChild(handleEnt.getComponent<cro::Transform>());
        }

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [](cro::Entity e, float)
            {
                static const auto wavetable = cro::Util::Wavetable::sine(0.3f);
                static std::size_t idx = 0;
                static constexpr float Rotation = cro::Util::Const::PI / 8.f;

                e.getComponent<cro::Transform>().setRotation(cro::Transform::Y_AXIS, Rotation * wavetable[idx]);

                idx = (idx + 1) % wavetable.size();
            };
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
    //reset to default tab
    prevTab(); //this just tidies up existing tab before forcing the index below
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

            cro::AudioMixer::setPrefadeVolume(ct, MixerChannel::Music);

            if (ct == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(e);
            }
        };

    cro::AudioMixer::setPrefadeVolume(0.f, MixerChannel::Music);
    m_music.getComponent<cro::AudioEmitter>().play();
}

void ScrubAttractState::onCachedPop()
{
    m_music.getComponent<cro::AudioEmitter>().stop();
    m_uiScene.simulate(0.f);
}