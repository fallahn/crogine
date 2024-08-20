//Auto-generated source file for Scratchpad Stub 20/08/2024, 12:39:17

#include "PseutheBackgroundState.hpp"
#include "PseutheConsts.hpp"
#include "PoissonDisk.hpp"
#include "PseutheBallSystem.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Random.hpp>

namespace
{
    constexpr std::size_t BallCount = 19;
    constexpr std::int32_t MinBallSize = 40;
    constexpr std::int32_t MaxBallSize = 92;
    constexpr float BallSize = 128.f; //this is the sprite size
}

PseutheBackgroundState::PseutheBackgroundState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createScene();
        
        //TODO push menu on load
        cacheState(States::ScratchPad::PseutheGame);
        cacheState(States::ScratchPad::PseutheMenu);
    });
}

//public
bool PseutheBackgroundState::handleEvent(const cro::Event& evt)
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
            cro::App::quit();
            break;
        }
    }

    m_gameScene.forwardEvent(evt);
    return true;
}

void PseutheBackgroundState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
}

bool PseutheBackgroundState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    return true;
}

void PseutheBackgroundState::render()
{
    m_gameScene.render();
}

//private
void PseutheBackgroundState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<PseutheBallSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem2D>(mb);
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);
}

void PseutheBackgroundState::loadAssets()
{
}

void PseutheBackgroundState::createScene()
{
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("pseuthe/assets/sprites/ball.spt", m_resources.textures);


    //for the sake of simplicity we'll use 1px/m - so max size is 128m
    std::vector<std::array<float, 2u>> p = pd::PoissonDiskSampling(256.f, std::array{0.f, 0.f}, std::array{SceneSizeFloat.x, SceneSizeFloat.y});
    for (auto i = 0u; i < BallCount && i < p.size(); ++i)
    {
        const glm::vec2 pos(p[i][0], p[i][1]);
        const float radius = static_cast<float>(cro::Util::Random::value(MinBallSize, MaxBallSize));

        auto entity = m_gameScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(pos);
        entity.getComponent<cro::Transform>().setScale(glm::vec2(radius / BallSize));
        entity.getComponent<cro::Transform>().setOrigin(glm::vec2(BallSize) / 2.f);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("ball");
        entity.addComponent<cro::SpriteAnimation>().play(0);
        entity.addComponent<PseutheBall>(radius);
    }


    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = std::bind(cameraCallback, std::placeholders::_1);
    cameraCallback(cam);

    m_gameScene.getActiveCamera().getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 2.f });
}