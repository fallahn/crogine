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

ScrubAttractState::ScrubAttractState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, SharedScrubData& sc)
    : cro::State        (ss, ctx),
    m_sharedData        (sd),
    m_sharedScrubData   (sc),
    m_uiScene           (ctx.appInstance.getMessageBus())
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
    //title tab - image of ball washer in centre,
    //text SCRUB spins in the middle of it

    //TODO these tabs could have a callback to restart
    //any animation a tab might have when the tab is shown
    //TODO the tabs need a child node for scaling the sprites
    m_tabs[TabID::Title] = m_uiScene.createEntity();
    m_tabs[TabID::Title].addComponent<cro::Transform>();


    //how to play tab
    m_tabs[TabID::HowTo] = m_uiScene.createEntity();
    m_tabs[TabID::HowTo].addComponent<cro::Transform>();

    const std::string str =
        R"(
Use A/D to scrub or right thumb stick
Use Q to insert and E to remove a ball (or LB and RB on the controller)
Press SPACE or Controller A to add more soap

Press ESCAPE or Start to Pause the game.


Press SPACE or Controller A to begin.
)";

    const auto& font = m_sharedScrubData.fonts->get(sc::FontID::Body);

    auto size = glm::vec2(cro::App::getWindow().getSize());
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.getComponent<cro::Transform>().move({ 0.f, 60.f * getViewScale() });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize * getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 2.f, -2.f });
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = glm::vec2(0.5f);
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 60.f };
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.getComponent<UIElement>().depth = sc::TextDepth;

    m_tabs[TabID::HowTo].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //high scores tab (or personal best in non-steam)
    m_tabs[TabID::Scores] = m_uiScene.createEntity();
    m_tabs[TabID::Scores].addComponent<cro::Transform>();




    //TODO we want to scale the sprites down a la game state
    //tho this will be per-tab rather than globally
    auto resize = [&](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -10.f, 10.f);

            //send messge to UI elements to reposition them
            cro::Command cmd;
            cmd.targetFlags = CommandID::UI::UIElement;
            cmd.action =
                [size](cro::Entity e, float)
                {
                    const auto& ui = e.getComponent<UIElement>();
                    float x = std::floor(size.x * ui.relativePosition.x);
                    float y = std::floor(size.y * ui.relativePosition.y);
                    
                    if (ui.characterSize)
                    {
                        e.getComponent<cro::Text>().setCharacterSize(ui.characterSize * getViewScale());
                        e.getComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2((ui.absolutePosition * getViewScale()) + glm::vec2(x, y)), ui.depth));
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