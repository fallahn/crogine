//Auto-generated source file for Scratchpad Stub 08/01/2024, 12:14:56

#include "EndlessDrivingState.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/ModelRenderer.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>

namespace
{
    //TODO move these to some Const header
    const glm::uvec2 RenderSize = glm::uvec2(640, 448);
}

EndlessDrivingState::EndlessDrivingState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        createUI();
    });
}

//public
bool EndlessDrivingState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            requestStackClear();
            requestStackPush(0);
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void EndlessDrivingState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool EndlessDrivingState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void EndlessDrivingState::render()
{
    m_gameTexture.clear(cro::Colour::Magenta);
    m_gameScene.render();
    m_gameTexture.display();

    m_uiScene.render();
}

//private
void EndlessDrivingState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem2D>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void EndlessDrivingState::loadAssets()
{
}

void EndlessDrivingState::createScene()
{
    auto resize = [](cro::Camera& cam)
    {
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };
        cam.setOrthographic(0.f, static_cast<float>(RenderSize.x), 0.f, static_cast<float>(RenderSize.y), -0.1f, 10.f);
    };

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}

void EndlessDrivingState::createUI()
{
    m_gameTexture.create(RenderSize.x, RenderSize.y, false);

    cro::Entity entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin(glm::vec2(RenderSize / 2u));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_gameTexture.getTexture());
    m_gameEntity = entity;

    //TODO add border overlay (recycle GvG?)

    auto resize = 
        [&](cro::Camera& cam)
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);

        const float scale = std::max(1.f, std::floor(size.y / RenderSize.y));
        m_gameEntity.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        m_gameEntity.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -0.1f));
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}