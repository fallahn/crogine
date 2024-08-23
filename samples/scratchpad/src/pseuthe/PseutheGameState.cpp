//Auto-generated source file for Scratchpad Stub 20/08/2024, 12:39:17

#include "PseutheGameState.hpp"
#include "PseutheConsts.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>

#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Maths.hpp>

PseutheGameState::PseutheGameState(cro::StateStack& stack, cro::State::Context context)
    : cro::State    (stack, context),
    m_gameScene     (context.appInstance.getMessageBus()),
    m_uiScene       (context.appInstance.getMessageBus())
{
    addSystems();
    loadAssets();
    createScene();
    createUI();
}

//public
bool PseutheGameState::handleEvent(const cro::Event& evt)
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

    m_gameScene.forwardEvent(evt);
    m_uiScene.forwardEvent(evt);
    return true;
}

void PseutheGameState::handleMessage(const cro::Message& msg)
{
    m_gameScene.forwardMessage(msg);
    m_uiScene.forwardMessage(msg);
}

bool PseutheGameState::simulate(float dt)
{
    m_gameScene.simulate(dt);
    m_uiScene.simulate(dt);
    return true;
}

void PseutheGameState::render()
{
    m_gameScene.render();
    m_uiScene.render();
}

//private
void PseutheGameState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_gameScene.addSystem<cro::CallbackSystem>(mb);
    m_gameScene.addSystem<cro::SpriteSystem2D>(mb);
    m_gameScene.addSystem<cro::SpriteAnimator>(mb);
    m_gameScene.addSystem<cro::CameraSystem>(mb);
    m_gameScene.addSystem<cro::RenderSystem2D>(mb);

    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::SpriteAnimator>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void PseutheGameState::loadAssets()
{
}

void PseutheGameState::createScene()
{
    createPlayer();

    auto& cam = m_gameScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = std::bind(cameraCallback, std::placeholders::_1);
    cameraCallback(cam);
}

void PseutheGameState::createUI()
{


    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = std::bind(cameraCallback, std::placeholders::_1);
    cameraCallback(cam);
}

void PseutheGameState::createPlayer()
{
    cro::SpriteSheet spriteSheet;

    //head part
    spriteSheet.loadFromFile("pseuthe/assets/sprites/head.spt", m_resources.textures);

    auto entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(SceneSizeFloat / 2.f);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("head");
    entity.getComponent<cro::Sprite>().setColour(PlayerColour); //TODO this could be part of the sprite sheet
    entity.addComponent<cro::SpriteAnimation>().play(0);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    auto headEnt = entity;


    //mouth part
    spriteSheet.loadFromFile("pseuthe/assets/sprites/mouth.spt", m_resources.textures);

    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({0.f, 0.f, -0.2f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("mouth");
    entity.getComponent<cro::Sprite>().setColour(PlayerColour);
    entity.addComponent<cro::SpriteAnimation>();// .play(0);
    headEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //antennae root
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, 0.1f });
    headEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto rootEnt = entity;


    constexpr float WigglerOffset = 24.f;

    struct WigglerCallback final
    {
        const float Rotation = 35.f * cro::Util::Const::degToRad;
        float direction = 1.f;
        cro::Entity head;

        WigglerCallback(float d, cro::Entity h)
            : direction(d), head(h) {}

        void operator()(cro::Entity e, float dt)
        {
            const auto rot = head.getComponent<cro::Transform>().getRotation2D();
            const auto amt = cro::Util::Maths::shortestRotation(e.getComponent<cro::Transform>().getRotation2D(), rot + (Rotation * direction));
            e.getComponent<cro::Transform>().rotate(amt * dt);
        }
    };

    //left wiggler
    spriteSheet.loadFromFile("pseuthe/assets/sprites/wiggler.spt", m_resources.textures);
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("wiggler");
    entity.getComponent<cro::Sprite>().setColour(PlayerColour);
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width + WigglerOffset, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = WigglerCallback(1.f, headEnt);
    rootEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //right wiggler
    entity = m_gameScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("wiggler");
    entity.getComponent<cro::Sprite>().setColour(PlayerColour);
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width + WigglerOffset, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = WigglerCallback(-1.f, headEnt);
    rootEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //TODO implement the 'tail' part
}