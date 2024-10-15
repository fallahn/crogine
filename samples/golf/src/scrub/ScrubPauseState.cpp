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

#include "ScrubPauseState.hpp"

#include <crogine/core/App.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>

#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/Font.hpp>

ScrubPauseState::ScrubPauseState(cro::StateStack& ss, cro::State::Context ctx)
    :cro::State (ss, ctx),
    m_uiScene   (ctx.appInstance.getMessageBus())
{
    addSystems();
    buildScene();
}

//public
bool ScrubPauseState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_q:
            requestStackClear();
            requestStackPush(StateID::ScrubBackground);
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
            requestStackClear();
            requestStackPush(StateID::ScrubBackground);
            break;
        case cro::GameController::ButtonStart:
            requestStackPop();
            break;
        }
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
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void ScrubPauseState::buildScene()
{
    const std::string str =
        R"(
Press ESCAPE or Start to Continue.

Press Q or Controller B to Quit.
)";

    //TODO use shared resources
    const std::int32_t fontID = 1;
    m_resources.fonts.load(fontID, "assets/golf/fonts/IBM_CGA.ttf");

    const auto& font = m_resources.fonts.get(fontID);

    auto size = glm::vec2(cro::App::getWindow().getSize());
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(size / 2.f);
    entity.getComponent<cro::Transform>().move({ 0.f, 60.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(16);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);


    auto resize = [&](cro::Camera& cam)
        {
            glm::vec2 size(cro::App::getWindow().getSize());
            cam.viewport = { 0.f, 0.f, 1.f, 1.f };
            cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.1f, 10.f);

            //send messge to UI elements to reposition them
            //cro::Command cmd;
            //cmd.targetFlags = CommandID::UI::UIElement;
            //cmd.action =
            //    [size](cro::Entity e, float)
            //    {
            //        const auto& ui = e.getComponent<UIElement>();
            //        float x = std::floor(size.x * ui.relativePosition.x);
            //        float y = std::floor(size.y * ui.relativePosition.y);
            //        e.getComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2(ui.absolutePosition + glm::vec2(x, y)), ui.depth));

            //        //TODO probably want to rescale downwards too?
            //    };
            //m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
        };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}