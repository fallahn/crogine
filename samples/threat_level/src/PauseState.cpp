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

#include "PauseState.hpp"
#include "Messages.hpp"
#include "MyApp.hpp"
#include "Slider.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/SpriteSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/Image.hpp>

namespace
{
#include "MenuConsts.inl"

    glm::vec2 sceneSize(1920.f, 1080.f);
    cro::CommandSystem* commandSystem = nullptr;

    enum GroupID
    {
        Pause, Options, Confirm
    };
}

PauseState::PauseState(cro::StateStack& stack, cro::State::Context ctx, ResourcePtr& sharedResources)
    : cro::State        (stack, ctx),
    m_uiScene           (ctx.appInstance.getMessageBus()),
    m_sharedResources   (*sharedResources),
    m_uiSystem          (nullptr)
{
    load();
    updateView();
}

//public
bool PauseState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_p:
        case SDLK_PAUSE:
        case SDLK_ESCAPE:
            requestStackPop();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default:break;
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_START:
        case SDL_CONTROLLER_BUTTON_BACK:
            requestStackPop();
            break;
        }
    }
    
    m_uiSystem->handleEvent(evt);
    return false;
}

void PauseState::handleMessage(const cro::Message& msg)
{
    m_uiScene.forwardMessage(msg);

    if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            updateView();
        }
    }
}

bool PauseState::simulate(float dt)
{
    m_uiScene.simulate(dt);

    return false;
}

void PauseState::render()
{
    m_uiScene.render(cro::App::getWindow());
}

//private
void PauseState::load()
{
    auto& mb = getContext().appInstance.getMessageBus();

    //add systems to scene
    m_uiScene.addSystem<SliderSystem>(mb);
    m_uiSystem = &m_uiScene.addSystem<cro::UISystem>(mb);
    commandSystem = &m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);


    cro::Image img;
    img.create(2, 2, stateBackgroundColour);

    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData(), false);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ sceneSize.x / 2.f, sceneSize.y / 2.f, 0.5f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.1f });
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundTexture);
    entity.addComponent<cro::Drawable2D>();

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);

    cro::SpriteSheet iconSheet;
    iconSheet.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);

    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();
    const auto buttonHighlightArea = spriteSheet.getSprite("button_active").getTextureRect();

    auto mouseEnterCallback = m_uiSystem->addCallback([&, buttonHighlightArea](cro::Entity e)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonHighlightArea);
    });
    auto mouseExitCallback = m_uiSystem->addCallback([&, buttonNormalArea](cro::Entity e)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonNormalArea);
    });


    createMenu(spriteSheet, iconSheet, mouseEnterCallback, mouseExitCallback);
    createOptions(spriteSheet, iconSheet, mouseEnterCallback, mouseExitCallback);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projectionMatrix = glm::ortho(0.f, sceneSize.x, 0.f, sceneSize.y, -10.f, 10.f);
    m_uiScene.setActiveCamera(entity);
}

void PauseState::createMenu(const cro::SpriteSheet& spriteSheet, const cro::SpriteSheet& iconSheet, cro::uint32 mouseEnterCallback, cro::uint32 mouseExitCallback)
{
    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);
    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(TextXL);
    entity.getComponent<cro::Text>().setString("PAUSED");
    entity.getComponent<cro::Text>().setFillColour(textColourSelected);
    auto textSize = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Transform>().setPosition({ sceneSize.x / 2.f, 900.f, 0.f });
    entity.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, 0.f, 0.f });

    //control to make slidey woo
    auto controlEntity = m_uiScene.createEntity();
    controlEntity.addComponent<cro::Transform>().setPosition({ sceneSize.x / 2.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget >().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    //options
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 60.f, 0.f });
    entity.addComponent<cro::UIInput>().setGroup(GroupID::Pause);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this]
    (cro::Entity e, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(-sceneSize.x, 0.f, 0.f);
            };
            commandSystem->sendCommand(cmd);
            m_uiScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Options);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    auto textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Drawable2D>();
    textEntity.addComponent<cro::Text>(font);
    textEntity.getComponent<cro::Text>().setString("Options");
    textEntity.getComponent<cro::Text>().setCharacterSize(TextLarge);
    textEntity.getComponent<cro::Text>().setFillColour(textColourNormal);
    entity.getComponent<cro::Transform>().addChild(textEntity.addComponent<cro::Transform>());
    textEntity.getComponent<cro::Transform>().move({ 20.f, 80.f, 1.f });

    auto iconEntity = m_uiScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEntity.addComponent<cro::Transform>());
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("settings");
    iconEntity.addComponent<cro::Drawable2D>();

    //main menu
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setPosition({ 0.f, -120.f, 0.f });
    entity.addComponent<cro::UIInput>().setGroup(GroupID::Pause);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, sceneSize.y, 0.f);
            };
            commandSystem->sendCommand(cmd);

            m_uiScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Confirm);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Drawable2D>();
    textEntity.addComponent<cro::Text>(font);
    textEntity.getComponent<cro::Text>().setString("Quit");
    textEntity.getComponent<cro::Text>().setCharacterSize(TextLarge);
    textEntity.getComponent<cro::Text>().setFillColour(textColourNormal);
    entity.getComponent<cro::Transform>().addChild(textEntity.addComponent<cro::Transform>());
    textEntity.getComponent<cro::Transform>().move({ 20.f, 80.f, 1.f });

    iconEntity = m_uiScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEntity.addComponent<cro::Transform>());
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("menu");
    iconEntity.addComponent<cro::Drawable2D>();

    //resume
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setPosition({ 0.f, -300.f, 0.f });
    entity.addComponent<cro::UIInput>().setGroup(GroupID::Pause);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            requestStackPop();
            //LOG("POP!", cro::Logger::Type::Info);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Drawable2D>();
    textEntity.addComponent<cro::Text>(font);
    textEntity.getComponent<cro::Text>().setString("Continue");
    textEntity.getComponent<cro::Text>().setCharacterSize(TextLarge);
    textEntity.getComponent<cro::Text>().setFillColour(textColourNormal);
    entity.getComponent<cro::Transform>().addChild(textEntity.addComponent<cro::Transform>());
    textEntity.getComponent<cro::Transform>().move({ 20.f, 80.f, 0.f });

    iconEntity = m_uiScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEntity.addComponent<cro::Transform>());
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("play");
    iconEntity.addComponent<cro::Drawable2D>();

    //quit confirm
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("menu");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    controlEntity.getComponent<cro::Transform>().addChild(entity.addComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setPosition({ 0.f, -1180.f, 0.1f });
    entity.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.2f });

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Drawable2D>();
    textEntity.addComponent<cro::Text>(font).setString("Exit?");
    textEntity.getComponent<cro::Text>().setFillColour(textColourSelected);
    textEntity.getComponent<cro::Text>().setCharacterSize(TextMedium);
    entity.getComponent<cro::Transform>().addChild(textEntity.addComponent<cro::Transform>());
    textEntity.getComponent<cro::Transform>().move({ (size.x / 2.f) - 56.f, size.y - 30.f, 0.2f });

    //ok button
    auto buttonEntity = m_uiScene.createEntity();
    buttonEntity.addComponent<cro::Drawable2D>();
    buttonEntity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.getComponent<cro::Transform>().addChild(buttonEntity.addComponent<cro::Transform>());
    buttonEntity.getComponent<cro::Transform>().setPosition({ size.x / 4.f, 82.f, 1.f });
    buttonEntity.getComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.2f });

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Drawable2D>();
    textEntity.addComponent<cro::Text>(font).setString("OK");
    textEntity.getComponent<cro::Text>().setFillColour(textColourNormal);
    textEntity.getComponent<cro::Text>().setCharacterSize(TextXL);
    buttonEntity.getComponent<cro::Transform>().addChild(textEntity.addComponent<cro::Transform>());
    textEntity.getComponent<cro::Transform>().setPosition({ 40.f, 80.f, 0.2f });

    iconEntity = m_uiScene.createEntity();
    buttonEntity.getComponent<cro::Transform>().addChild(iconEntity.addComponent<cro::Transform>());
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("exit");
    iconEntity.addComponent<cro::Drawable2D>();

    buttonEntity.addComponent<cro::UIInput>().setGroup(GroupID::Confirm);
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            requestStackClear();
            requestStackPush(States::MainMenu);
        }
    });
    buttonEntity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    buttonEntity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    //cancel button
    buttonEntity = m_uiScene.createEntity();
    buttonEntity.addComponent<cro::Drawable2D>();
    buttonEntity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.getComponent<cro::Transform>().addChild(buttonEntity.addComponent<cro::Transform>());
    buttonEntity.getComponent<cro::Transform>().setPosition({ (size.x / 4.f) + (size.x / 2.f), 82.f, 1.f });
    buttonEntity.getComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.2f });

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Drawable2D>();
    textEntity.addComponent<cro::Text>(font).setString("Cancel");
    textEntity.getComponent<cro::Text>().setFillColour(textColourNormal);
    textEntity.getComponent<cro::Text>().setCharacterSize(TextXL);
    buttonEntity.getComponent<cro::Transform>().addChild(textEntity.addComponent<cro::Transform>());
    textEntity.getComponent<cro::Transform>().setPosition({ 40.f, 80.f, 0.2f });

    iconEntity = m_uiScene.createEntity();
    buttonEntity.getComponent<cro::Transform>().addChild(iconEntity.addComponent<cro::Transform>());
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("back");
    iconEntity.addComponent<cro::Drawable2D>();

    buttonEntity.addComponent<cro::UIInput>().setGroup(GroupID::Confirm);
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, -sceneSize.y, 0.f);
            };
            commandSystem->sendCommand(cmd);

            m_uiScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Pause);
        }
    });
    buttonEntity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    buttonEntity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

}

void PauseState::createOptions(const cro::SpriteSheet& spriteSheet, const cro::SpriteSheet& iconSheet, cro::uint32 mouseEnterCallback, cro::uint32 mouseExitCallback)
{
    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);
    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();

    //control to make slidey woo
    auto controlEntity = m_uiScene.createEntity();
    controlEntity.addComponent<cro::Transform>().setPosition({ 2880.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget >().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    //panel background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("menu");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y, 0.f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 140.f, -0.01f });
    controlEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setString("Options");
    textEnt.getComponent<cro::Text>().setCharacterSize(TextMedium);
    textEnt.getComponent<cro::Text>().setFillColour(textColourSelected);
    controlEntity.getComponent<cro::Transform>().addChild(textEnt.addComponent<cro::Transform>());
    textEnt.getComponent<cro::Transform>().setPosition({ -86.f, 110.f, 0.f });
    
    //back button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -480.f, 0.f });
    controlEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    entity.addComponent<cro::UIInput>().setGroup(GroupID::Options);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([this](cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, float)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(sceneSize.x, 0.f, 0.f);
            };
            commandSystem->sendCommand(cmd);

            m_uiScene.getSystem<cro::UISystem>().setActiveGroup(GroupID::Pause);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;


    textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setString("Back");
    textEnt.getComponent<cro::Text>().setFillColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharacterSize(TextLarge);
    entity.getComponent<cro::Transform>().addChild(textEnt.addComponent<cro::Transform>());
    textEnt.getComponent<cro::Transform>().setPosition({ 20.f, 80.f, 0.f });

    auto iconEnt = m_uiScene.createEntity();
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>() = iconSheet.getSprite("back");
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
}

void PauseState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport.bottom = (1.f - size.y) / 2.f;
    cam2D.viewport.height = size.y;
}