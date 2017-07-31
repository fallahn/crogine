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
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandID.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/Image.hpp>

namespace
{
#include "MenuConsts.inl"

    glm::vec2 sceneSize(1920.f, 1080.f);
    cro::CommandSystem* commandSystem = nullptr;
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

bool PauseState::simulate(cro::Time dt)
{
    m_uiScene.simulate(dt);

    return false;
}

void PauseState::render()
{
    m_uiScene.render();
}

//private
void PauseState::load()
{
    auto& mb = getContext().appInstance.getMessageBus();

    //add systems to scene
    m_uiScene.addSystem<SliderSystem>(mb);
    m_uiSystem = &m_uiScene.addSystem<cro::UISystem>(mb);
    commandSystem = &m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::SceneGraph>(mb);
    m_uiScene.addSystem<cro::SpriteRenderer>(mb);
    m_uiScene.addSystem<cro::TextRenderer>(mb);

    cro::Image img;
    img.create(2, 2, cro::Colour(0.f, 0.f, 0.f, 0.6f));

    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData(), false);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ sceneSize.x / 2.f, sceneSize.y / 2.f, 0.5f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.1f });
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundTexture);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);

    cro::SpriteSheet iconSheet;
    iconSheet.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);

    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();
    const auto buttonHighlightArea = spriteSheet.getSprite("button_active").getTextureRect();

    auto mouseEnterCallback = m_uiSystem->addCallback([&, buttonHighlightArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonHighlightArea);
        const auto& children = e.getComponent<cro::Transform>().getChildIDs();
        std::size_t i = 0;
        while (children[i] != -1)
        {
            auto c = children[i++];
            auto child = m_uiScene.getEntity(c);
            if (child.hasComponent<cro::Text>())
            {
                child.getComponent<cro::Text>().setColour(textColourSelected);
            }
            else if (child.hasComponent<cro::Sprite>())
            {
                child.getComponent<cro::Sprite>().setColour(textColourSelected);
            }
        }
    });
    auto mouseExitCallback = m_uiSystem->addCallback([&, buttonNormalArea](cro::Entity e, cro::uint64)
    {
        e.getComponent<cro::Sprite>().setTextureRect(buttonNormalArea);
        const auto& children = e.getComponent<cro::Transform>().getChildIDs();
        std::size_t i = 0;
        while (children[i] != -1)
        {
            auto c = children[i++];
            auto child = m_uiScene.getEntity(c);
            if (child.hasComponent<cro::Text>())
            {
                child.getComponent<cro::Text>().setColour(textColourNormal);
            }
            else if (child.hasComponent<cro::Sprite>())
            {
                child.getComponent<cro::Sprite>().setColour(textColourNormal);
            }
        }
    });


    createMenu(spriteSheet, iconSheet, mouseEnterCallback, mouseExitCallback);
    createOptions(spriteSheet, iconSheet, mouseEnterCallback, mouseExitCallback);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projection = glm::ortho(0.f, sceneSize.x, 0.f, sceneSize.y, -10.f, 10.f);
    m_uiScene.setActiveCamera(entity);
}

void PauseState::createMenu(const cro::SpriteSheet& spriteSheet, const cro::SpriteSheet& iconSheet, cro::uint32 mouseEnterCallback, cro::uint32 mouseExitCallback)
{
    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);
    const auto buttonNormalArea = spriteSheet.getSprite("button_inactive").getTextureRect();

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Text>(font).setCharSize(TextXL);
    entity.getComponent<cro::Text>().setString("PAUSED");
    entity.getComponent<cro::Text>().setColour(textColourSelected);
    auto textSize = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::Transform>().setPosition({ sceneSize.x / 2.f, 900.f, 0.f });
    entity.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, 0.f, 0.f });

    //control to make slidey woo
    auto controlEntity = m_uiScene.createEntity();
    controlEntity.addComponent<cro::Transform>().setPosition({ sceneSize.x / 2.f, 624.f, 0.f });
    controlEntity.addComponent<cro::CommandTarget >().ID = CommandID::MenuController;
    controlEntity.addComponent<Slider>();

    //options
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(controlEntity);
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 60.f, 0.f });
    entity.addComponent<cro::UIInput>();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([this]
    (cro::Entity e, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || (flags & cro::UISystem::Finger))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(-sceneSize.x, 0.f, 0.f);
            };
            commandSystem->sendCommand(cmd);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    auto textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Text>(font);
    textEntity.getComponent<cro::Text>().setString("Options");
    textEntity.getComponent<cro::Text>().setCharSize(TextLarge);
    textEntity.getComponent<cro::Text>().setColour(textColourNormal);
    textEntity.addComponent<cro::Transform>().setParent(entity);
    textEntity.getComponent<cro::Transform>().move({ 40.f, 100.f, 0.f });

    auto iconEntity = m_uiScene.createEntity();
    iconEntity.addComponent<cro::Transform>().setParent(entity);
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("settings");

    //main menu
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(controlEntity);
    entity.getComponent<cro::Transform>().setPosition({ 0.f, -120.f, 0.f });
    entity.addComponent<cro::UIInput>();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || flags & cro::UISystem::Finger)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, sceneSize.y, 0.f);
            };
            commandSystem->sendCommand(cmd);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Text>(font);
    textEntity.getComponent<cro::Text>().setString("Quit");
    textEntity.getComponent<cro::Text>().setCharSize(TextLarge);
    textEntity.getComponent<cro::Text>().setColour(textColourNormal);
    textEntity.addComponent<cro::Transform>().setParent(entity);
    textEntity.getComponent<cro::Transform>().move({ 40.f, 100.f, 0.f });

    iconEntity = m_uiScene.createEntity();
    iconEntity.addComponent<cro::Transform>().setParent(entity);
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("menu");

    //resume
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(controlEntity);
    entity.getComponent<cro::Transform>().setPosition({ 0.f, -300.f, 0.f });
    entity.addComponent<cro::UIInput>();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback(
        [&](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse) //gets raised even on touch
            /*|| flags & cro::UISystem::Finger*/)
        {
            requestStackPop();
            //LOG("POP!", cro::Logger::Type::Info);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Text>(font);
    textEntity.getComponent<cro::Text>().setString("Continue");
    textEntity.getComponent<cro::Text>().setCharSize(TextLarge);
    textEntity.getComponent<cro::Text>().setColour(textColourNormal);
    textEntity.addComponent<cro::Transform>().setParent(entity);
    textEntity.getComponent<cro::Transform>().move({ 40.f, 100.f, 0.f });

    iconEntity = m_uiScene.createEntity();
    iconEntity.addComponent<cro::Transform>().setParent(entity);
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("play");

    //quit confirm
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("menu");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setParent(controlEntity);
    entity.getComponent<cro::Transform>().setPosition({ 0.f, -1180.f, 0.1f });
    entity.getComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y / 2.f, 0.2f });

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Text>(font).setString("Exit?");
    textEntity.getComponent<cro::Text>().setColour(textColourSelected);
    textEntity.getComponent<cro::Text>().setCharSize(TextMedium);
    textEntity.addComponent<cro::Transform>().setParent(entity);
    textEntity.getComponent<cro::Transform>().move({ (size.x / 2.f) - 56.f, size.y - 30.f, 0.2f });

    //ok button
    auto buttonEntity = m_uiScene.createEntity();
    buttonEntity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    buttonEntity.addComponent<cro::Transform>().setParent(entity);
    buttonEntity.getComponent<cro::Transform>().setPosition({ size.x / 4.f, 82.f, 1.f });
    buttonEntity.getComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.2f });

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Text>(font).setString("OK");
    textEntity.getComponent<cro::Text>().setColour(textColourNormal);
    textEntity.getComponent<cro::Text>().setCharSize(TextXL);
    textEntity.addComponent<cro::Transform>().setParent(buttonEntity);
    textEntity.getComponent<cro::Transform>().setPosition({ 60.f, 110.f, 0.2f });

    iconEntity = m_uiScene.createEntity();
    iconEntity.addComponent<cro::Transform>().setParent(buttonEntity);
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("exit");

    buttonEntity.addComponent<cro::UIInput>();
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseDown] = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            /*|| flags & cro::UISystem::Finger*/)
        {
            requestStackClear();
            requestStackPush(States::MainMenu);
        }
    });
    buttonEntity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    buttonEntity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;

    //cancel button
    buttonEntity = m_uiScene.createEntity();
    buttonEntity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    buttonEntity.addComponent<cro::Transform>().setParent(entity);
    buttonEntity.getComponent<cro::Transform>().setPosition({ (size.x / 4.f) + (size.x / 2.f), 82.f, 1.f });
    buttonEntity.getComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, 0.f, 0.2f });

    textEntity = m_uiScene.createEntity();
    textEntity.addComponent<cro::Text>(font).setString("Cancel");
    textEntity.getComponent<cro::Text>().setColour(textColourNormal);
    textEntity.getComponent<cro::Text>().setCharSize(TextXL);
    textEntity.addComponent<cro::Transform>().setParent(buttonEntity);
    textEntity.getComponent<cro::Transform>().setPosition({ 60.f, 110.f, 0.2f });

    iconEntity = m_uiScene.createEntity();
    iconEntity.addComponent<cro::Transform>().setParent(buttonEntity);
    iconEntity.getComponent<cro::Transform>().setPosition({ buttonNormalArea.width - buttonIconOffset, 0.f, 0.2f });
    iconEntity.addComponent<cro::Sprite>() = iconSheet.getSprite("back");

    buttonEntity.addComponent<cro::UIInput>();
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    buttonEntity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseDown] = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse))
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(0.f, -sceneSize.y, 0.f);
            };
            commandSystem->sendCommand(cmd);
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
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("menu");
    auto size = entity.getComponent<cro::Sprite>().getSize();
    entity.addComponent<cro::Transform>().setOrigin({ size.x / 2.f, size.y, 0.f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 140.f, -0.01f });
    entity.getComponent<cro::Transform>().setParent(controlEntity);

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Text>(font).setString("Options");
    textEnt.getComponent<cro::Text>().setCharSize(TextMedium);
    textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    textEnt.addComponent<cro::Transform>().setParent(controlEntity);
    textEnt.getComponent<cro::Transform>().setPosition({ -86.f, 110.f, 0.f });
    
    //back button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_inactive");
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -480.f, 0.f });
    entity.getComponent<cro::Transform>().setParent(controlEntity);
    entity.getComponent<cro::Transform>().setOrigin({ buttonNormalArea.width / 2.f, buttonNormalArea.height / 2.f, 0.f });
    entity.addComponent<cro::UIInput>();
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseEnter] = mouseEnterCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseExit] = mouseExitCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([this](cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || flags & cro::UISystem::Finger)
        {
            cro::Command cmd;
            cmd.targetFlags = CommandID::MenuController;
            cmd.action = [](cro::Entity e, cro::Time)
            {
                auto& slider = e.getComponent<Slider>();
                slider.active = true;
                slider.destination = e.getComponent<cro::Transform>().getPosition() + glm::vec3(sceneSize.x, 0.f, 0.f);
            };
            commandSystem->sendCommand(cmd);
        }
    });
    entity.getComponent<cro::UIInput>().area.width = buttonNormalArea.width;
    entity.getComponent<cro::UIInput>().area.height = buttonNormalArea.height;


    textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Text>(font).setString("Back");
    textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    textEnt.getComponent<cro::Text>().setCharSize(TextLarge);
    textEnt.addComponent<cro::Transform>().setParent(entity);
    textEnt.getComponent<cro::Transform>().setPosition({ 40.f, 100.f, 0.f });

    auto iconEnt = m_uiScene.createEntity();
    iconEnt.addComponent<cro::Sprite>() = iconSheet.getSprite("back");
    iconEnt.addComponent<cro::Transform>().setParent(entity);
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