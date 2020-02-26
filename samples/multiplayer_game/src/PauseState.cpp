/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include "PauseState.hpp"
#include "SharedStateData.hpp"
#include "MenuConsts.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/graphics/Image.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/UIInput.hpp>

#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

PauseState::PauseState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene(ctx.appInstance.getMessageBus())
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool PauseState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_p:
        case SDLK_ESCAPE:
        case SDLK_PAUSE:
            requestStackPop();
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }

    m_scene.getSystem<cro::UISystem>().handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void PauseState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }

    m_scene.forwardMessage(msg);
}

bool PauseState::simulate(float dt)
{
    m_scene.simulate(dt);
    //must return true to make sure the underlying state
    //keeps the network connection updated.
    return true;
}

void PauseState::render()
{
    m_scene.render(cro::App::getWindow());
}

//private
void PauseState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::SpriteRenderer>(mb);
    m_scene.addSystem<cro::TextRenderer>(mb);


    //background image
    cro::Image img;
    img.create(2, 2, cro::Colour(std::uint8_t(0), 0u, 0u, 120u));

    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData());

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ cro::DefaultSceneSize.x / 2.f, cro::DefaultSceneSize.y / 2.f, 1.f });
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundTexture);

    auto mouseEnter = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e, glm::vec2) 
        {
            e.getComponent<cro::Text>().setColour(TextHighlightColour);
        });
    auto mouseExit = m_scene.getSystem<cro::UISystem>().addCallback(
        [](cro::Entity e, glm::vec2)
        {
            e.getComponent<cro::Text>().setColour(TextNormalColour);
        });

    //menu items
    m_font.loadFromFile("assets/fonts/VeraMono.ttf");
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 900.f });
    entity.addComponent<cro::Text>(m_font).setString("PAUSED");
    entity.getComponent<cro::Text>().setCharSize(120);
    entity.getComponent<cro::Text>().setColour(TextNormalColour);

    //options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 500.f });
    entity.addComponent<cro::Text>(m_font).setString("Options");
    entity.getComponent<cro::Text>().setCharSize(50);
    entity.getComponent<cro::Text>().setColour(TextNormalColour);
    auto bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&](cro::Entity e, std::uint64_t flags)
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    cro::Console::show();
                }
            });

    //back
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 440.f });
    entity.addComponent<cro::Text>(m_font).setString("Back");
    entity.getComponent<cro::Text>().setCharSize(50);
    entity.getComponent<cro::Text>().setColour(TextNormalColour);
    bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&](cro::Entity e, std::uint64_t flags)
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    cro::App::getWindow().setMouseCaptured(true);
                    requestStackPop();
                }
            });

    //quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 120.f, 380.f });
    entity.addComponent<cro::Text>(m_font).setString("Quit");
    entity.getComponent<cro::Text>().setCharSize(50);
    entity.getComponent<cro::Text>().setColour(TextNormalColour);
    bounds = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] =
        m_scene.getSystem<cro::UISystem>().addCallback(
            [&](cro::Entity e, std::uint64_t flags)
            {
                if (flags & cro::UISystem::LeftMouse)
                {
                    //TODO some sort of OK/Cancel box
                    cro::App::getWindow().setMouseCaptured(false);
                    requestStackClear();
                    requestStackPush(States::ID::MainMenu);
                }
            });

    //create a custom camera to allow or own update function
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>();

    m_scene.setActiveCamera(entity);

    updateView();
}

void PauseState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    cam.projectionMatrix = glm::ortho(0.f, static_cast<float>(cro::DefaultSceneSize.x), 0.f, static_cast<float>(cro::DefaultSceneSize.y), -2.f, 100.f);
    cam.viewport.bottom = (1.f - size.y) / 2.f;
    cam.viewport.height = size.y;
}