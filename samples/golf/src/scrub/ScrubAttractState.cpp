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
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/graphics/Font.hpp>
#include <crogine/detail/OpenGL.hpp>

namespace
{
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
    m_currentTab        (0)
{
    addSystems();
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
            prevTab();
            break;
        case cro::GameController::ButtonRightShoulder:
            nextTab();
            break;
        }
    }


    m_uiScene.forwardEvent(evt);
    return false;
}

void ScrubAttractState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool ScrubAttractState::simulate(float dt)
{
    //TODO automatically scroll through tabs

    m_uiScene.simulate(dt);
    return true;
}

void ScrubAttractState::render()
{
    m_uiScene.render();
}

//private
void ScrubAttractState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioSystem>(mb);
}

void ScrubAttractState::loadAssets()
{
    //TODO load menu music
}

void ScrubAttractState::buildScene()
{
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
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 60.f };
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

    titleData.onStartIn = 
        [textEnt, bgEnt](cro::Entity) mutable
        {
            textEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            textEnt.getComponent<cro::Transform>().setRotation(0.f);
            textEnt.getComponent<cro::Callback>().getUserData<TitleAnimationData>().rotation = 0.f;

            bgEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            bgEnt.getComponent<cro::Callback>().getUserData<float>() = 0.f;
        };
    titleData.onEndIn = 
        [textEnt, bgEnt](cro::Entity) mutable //to play anim
        {
            textEnt.getComponent<cro::Callback>().active = true;
            bgEnt.getComponent<cro::Callback>().active = true;
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

    const std::string str =
        R"(
Use A/D to scrub or right thumb stick
Use Q to insert and E to remove a ball
Press SPACE or Controller A to add more soap

Press ESCAPE or Start to Pause the game.


Press SPACE or Controller A to begin.
)";


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize * getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, std::floor(bounds.height / 2.f) };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

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
    entity.getComponent<cro::Transform>().move({ 0.f, 60.f * getViewScale() });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("flaps");
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

void ScrubAttractState::prevTab()
{
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().hide(m_tabs[m_currentTab]);
    m_currentTab = (m_currentTab + (m_tabs.size() - 1)) % m_tabs.size();
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().show(m_tabs[m_currentTab]);
}

void ScrubAttractState::nextTab()
{
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().hide(m_tabs[m_currentTab]);
    m_currentTab = (m_currentTab + 1) % m_tabs.size();
    m_tabs[m_currentTab].getComponent<cro::Callback>().getUserData<TabData>().show(m_tabs[m_currentTab]);
}

void ScrubAttractState::onCachedPush()
{
    //TODO reset to default tab
    //TODO reset tab change timer
}