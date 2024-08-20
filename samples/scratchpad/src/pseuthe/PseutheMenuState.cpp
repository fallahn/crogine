//Auto-generated source file for Scratchpad Stub 20/08/2024, 12:39:17

#include "PseutheMenuState.hpp"
#include "PseutheConsts.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Constants.hpp>

PseutheMenuState::PseutheMenuState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_uiScene       (context.appInstance.getMessageBus())
{
    addSystems();
    loadAssets();
    createUI();
}

//public
bool PseutheMenuState::handleEvent(const cro::Event& evt)
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

            break;
        }
    }

    m_uiScene.forwardEvent(evt);
    return true;
}

void PseutheMenuState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);
}

bool PseutheMenuState::simulate(float dt)
{
    m_uiScene.simulate(dt);
    return true;
}

void PseutheMenuState::render()
{
    m_uiScene.render();
}

//private
void PseutheMenuState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void PseutheMenuState::loadAssets()
{
}

void PseutheMenuState::createUI()
{

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = std::bind(cameraCallback, std::placeholders::_1);
    cameraCallback(cam);
}