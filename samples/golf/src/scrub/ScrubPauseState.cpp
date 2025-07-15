/*-----------------------------------------------------------------------

Matt Marchant 2024 - 2025
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

#include "ScrubPauseState.hpp"
#include "ScrubConsts.hpp"
#include "ScrubSharedData.hpp"
#include "../golf/SharedStateData.hpp"
#include "../golf/GameConsts.hpp"

#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Font.hpp>

ScrubPauseState::ScrubPauseState(cro::StateStack& ss, cro::State::Context ctx, SharedMinigameData& sc)
    : cro::State        (ss, ctx),
    m_sharedScrubData   (sc),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_controllerIndex   (0)
{
    addSystems();
    buildScene();
}

//public
bool ScrubPauseState::handleEvent(const cro::Event& evt)
{
    const auto quit = [&]()
        {
            requestStackPop();
            requestStackPop();

            //game background states detect this via message
            //and push their own attract state so we can
            //recycle the pause state in multple games
        };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_q:
            quit();
            break;
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
            requestStackPop();
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
        case cro::GameController::ButtonStart:
            requestStackPop();
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

void ScrubPauseState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool ScrubPauseState::simulate(float dt)
{
    m_uiScene.simulate(dt);
    return false;
}

void ScrubPauseState::render()
{
    m_uiScene.render();
}

//private
void ScrubPauseState::addSystems()
{
    auto& mb = cro::App::getInstance().getMessageBus();
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void ScrubPauseState::buildScene()
{
    static const cro::String StringPS = "Press Start to continue\n\nPress " + cro::String(ButtonCircle) + " to Quit";
    static const cro::String StringXB = "Press Start to continue\n\nPress " + cro::String(ButtonB) + " to Quit";
    static const cro::String StringKB = "Press ESCAPE to continue\n\nPress Q to Quit";

    auto size = glm::vec2(cro::App::getWindow().getSize());
    const auto& font = m_sharedScrubData.fonts->get(sc::FontID::Title);
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.getComponent<cro::Transform>().move({ 0.f, 60.f * getViewScale() });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("PAUSED");
    entity.getComponent<cro::Text>().setCharacterSize(sc::LargeTextSize * getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::LargeTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 60.f };
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.getComponent<UIElement>().characterSize = sc::LargeTextSize;

    const auto& font2 = m_sharedScrubData.fonts->get(sc::FontID::Body);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, sc::TextDepth));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font2);
    entity.getComponent<cro::Text>().setCharacterSize(sc::MediumTextSize * getViewScale());
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset(sc::MediumTextOffset);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    //entity.getComponent<UIElement>().absolutePosition = { 0.f, 40.f };
    entity.getComponent<UIElement>().depth = sc::TextDepth;
    entity.getComponent<UIElement>().characterSize = sc::MediumTextSize;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (cro::GameController::getControllerCount()
                || Social::isSteamdeck())
            {
                if (cro::GameController::hasPSLayout(m_controllerIndex))
                {
                    e.getComponent<cro::Text>().setString(StringPS);
                }
                else
                {
                    e.getComponent<cro::Text>().setString(StringXB);
                }
            }
            else
            {
                e.getComponent<cro::Text>().setString(StringKB);
            }
        };


    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 1.f), cro::Colour(0.f,0.f,0.f,BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour(0.f,0.f,0.f,BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(1.f), cro::Colour(0.f,0.f,0.f,BackgroundAlpha)),
            cro::Vertex2D(glm::vec2(1.f, 0.f), cro::Colour(0.f,0.f,0.f,BackgroundAlpha))
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            auto size = glm::vec2(cro::App::getWindow().getSize());
            e.getComponent<cro::Transform>().setScale(size);
        };

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