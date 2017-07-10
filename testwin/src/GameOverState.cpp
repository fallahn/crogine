/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "GameOverState.hpp"
#include "Messages.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Transform.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>

namespace
{
#ifdef PLATFORM_DESKTOP
    glm::vec2 uiRes(1920.f, 1080.f);
#else
    glm::vec2 uiRes(1280.f, 720.f);
#endif //PLATFORM_DESKTOP

#include "MenuConsts.inl"
}

GameOverState::GameOverState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_uiSystem      (nullptr)
{
    load();
    updateView();
}

//public
bool GameOverState::handleEvent(const cro::Event& evt)
{
    m_uiScene.forwardEvent(evt);
    m_uiSystem->handleEvent(evt);
    return true;
}

void GameOverState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }

    m_uiScene.forwardMessage(msg);
}

bool GameOverState::simulate(cro::Time dt)
{
    m_uiScene.simulate(dt);
    return true;
}

void GameOverState::render()
{
    m_uiScene.render();
}

//private
void GameOverState::load()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiSystem = &m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::SceneGraph>(mb);
    m_uiScene.addSystem<cro::SpriteRenderer>(mb);
    m_uiScene.addSystem<cro::TextRenderer>(mb);

    auto& font = m_resources.fonts.get(FontID::MenuFont);
    font.loadFromFile("assets/fonts/Audiowide-Regular.ttf");

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, uiRes.y / 2.f, 2.f });
    entity.addComponent<cro::Text>(font).setString("GAME OVER");
    entity.getComponent<cro::Text>().setCharSize(80);
    entity.getComponent<cro::Text>().setColour(textColourSelected);

    //camera
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projection = glm::ortho(0.f, uiRes.x, 0.f, uiRes.y, -10.1f, 10.f);
    m_uiScene.setActiveCamera(entity);
}

void GameOverState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport.bottom = (1.f - size.y) / 2.f;
    cam2D.viewport.height = size.y;
}