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
#include "MyApp.hpp"

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/UIInput.hpp>

#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/core/ConfigFile.hpp>

#include <SDL_keyboard.h>

namespace
{
#ifdef PLATFORM_DESKTOP
    glm::vec2 uiRes(1920.f, 1080.f);
    //glm::vec2 uiRes(1280.f, 720.f);
#else
    glm::vec2 uiRes(1280.f, 720.f);
#endif //PLATFORM_DESKTOP

    enum UICommand
    {
        ScoreText = 0x1,
        NameText = 0x2,
        InputBox = 0x4
    };
    cro::CommandSystem* commandSystem = nullptr;
    const float uiDepth = 1.f;
    cro::FloatRect inactiveArea;

    const std::array<std::string, 10u> names = 
    {
        "Bob", "Margaret", "Hannah", "Kafoor", "Roman",
        "Naomi", "Joe", "Violet", "Amy", "Robert"
    };

#include "MenuConsts.inl"
}

GameOverState::GameOverState(cro::StateStack& stack, cro::State::Context context, ResourcePtr& sharedResources)
    : cro::State        (stack, context),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_sharedResources   (*sharedResources),
    m_uiSystem          (nullptr)
{
    load();
    updateView();
}

//public
bool GameOverState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_TEXTINPUT)
    {
        handleTextEvent(evt);
    }
    else if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_RETURN)
        {
            SDL_StopTextInput();

            cro::Command cmd;
            cmd.targetFlags = UICommand::NameText;
            cmd.action = [&](cro::Entity entity, float)
            {
                entity.getComponent<cro::Text>().setString(m_sharedResources.playerName);
            };
            commandSystem->sendCommand(cmd);

            cmd.targetFlags = UICommand::InputBox;
            cmd.action = [](cro::Entity entity, float)
            {
                entity.getComponent<cro::Sprite>().setTextureRect(inactiveArea);
            };
            commandSystem->sendCommand(cmd);
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        //android only produces key down for backspace events :S
        if (evt.key.keysym.sym == SDLK_BACKSPACE && !m_sharedResources.playerName.empty())
        {
            m_sharedResources.playerName.pop_back();
            updateTextBox();
        }
    }
    
    m_uiScene.forwardEvent(evt);
    m_uiSystem->handleEvent(evt);
    return false;
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

bool GameOverState::simulate(float dt)
{
    cro::Command cmd;
    cmd.targetFlags = UICommand::ScoreText;
    cmd.action = [&](cro::Entity entity, float)
    {
        auto& txt = entity.getComponent<cro::Text>();
        txt.setString("Score: " + std::to_string(m_sharedResources.score));
        auto bounds = txt.getLocalBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, 0.f });
    };
    commandSystem->sendCommand(cmd);
    
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
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteRenderer>(mb);
    m_uiScene.addSystem<cro::TextRenderer>(mb);
    commandSystem = &m_uiScene.addSystem<cro::CommandSystem>(mb);

    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);

    //background image
    cro::Image img;
    img.create(2, 2, stateBackgroundColour);
    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData(), false);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ uiRes.x / 2.f, uiRes.y / 2.f, 0.5f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundTexture);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Text>(font).setString("GAME OVER");
    entity.getComponent<cro::Text>().setCharSize(TextXL);
    entity.getComponent<cro::Text>().setColour(textColourSelected);
    auto textSize = entity.getComponent<cro::Text>().getLocalBounds();
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, (uiRes.y / 4.f) * 3.4f, uiDepth });
    entity.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, -textSize.height / 2.f, 0.f });


    //quit button
    cro::SpriteSheet sprites;
    sprites.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);
    cro::SpriteSheet icons;
    icons.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Sprite>() = sprites.getSprite("button_inactive");
    auto area = entity.getComponent<cro::Sprite>().getLocalBounds();
    auto& buttonTx = entity.addComponent<cro::Transform>();
    buttonTx.setOrigin({ area.width / 2.f, area.height / 2.f, 0.f });
    buttonTx.setPosition({ uiRes.x / 2.f, 144.f, uiDepth });

    auto textEnt = m_uiScene.createEntity();
    auto& gameText = textEnt.addComponent<cro::Text>(font);
    gameText.setString("OK");
    gameText.setColour(textColourNormal);
    gameText.setCharSize(TextLarge);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 40.f, 100.f, 0.f });
    entity.getComponent<cro::Transform>().addChild(gameTextTx);
    auto iconEnt = m_uiScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ area.width - buttonIconOffset, 0.f, 0.f });
    iconEnt.addComponent<cro::Sprite>() = icons.getSprite("menu");

    auto activeArea = sprites.getSprite("button_active").getTextureRect();
    auto& gameControl = entity.addComponent<cro::UIInput>();
    gameControl.callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback(
        [&,activeArea, iconEnt, textEnt](cro::Entity e, glm::vec2) mutable
    {
        e.getComponent<cro::Sprite>().setTextureRect(activeArea);
        iconEnt.getComponent<cro::Sprite>().setColour(textColourSelected);
        textEnt.getComponent<cro::Text>().setColour(textColourSelected);
    });
    gameControl.callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback(
        [&, area, iconEnt, textEnt](cro::Entity e, glm::vec2) mutable
    {
        e.getComponent<cro::Sprite>().setTextureRect(area);
        iconEnt.getComponent<cro::Sprite>().setColour(textColourNormal);
        textEnt.getComponent<cro::Text>().setColour(textColourNormal);
    });
    gameControl.callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([&]
    (cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            /*|| flags & cro::UISystem::Finger*/)
        {
            //insert name / score into high score list           
            cro::ConfigFile scores;
            if (scores.loadFromFile(cro::App::getPreferencePath() + highscoreFile))
            {
                scores.addProperty(std::to_string(m_sharedResources.score), m_sharedResources.playerName);
                scores.save(cro::App::getPreferencePath() + highscoreFile);
            }

            SDL_StopTextInput();
            requestStackClear();
            requestStackPush(States::MainMenu);
        }
    });
    gameControl.area.width = area.width;
    gameControl.area.height = area.height;


    auto scoreEnt = m_uiScene.createEntity();
    auto& scoreText = scoreEnt.addComponent<cro::Text>(font);
    scoreText.setString("Score: 0000");
    scoreText.setCharSize(TextXL);
    scoreText.setColour(textColourSelected);
    scoreEnt.addComponent<cro::Transform>().setPosition({uiRes.x / 2.f, (uiRes.y / 2.f) * 1.1f, uiDepth});
    scoreEnt.addComponent<cro::CommandTarget>().ID = UICommand::ScoreText;

    createTextBox(sprites);

    //camera
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projectionMatrix = glm::ortho(0.f, uiRes.x, 0.f, uiRes.y, -10.1f, 10.f);
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

void GameOverState::createTextBox(const cro::SpriteSheet& spriteSheet)
{
    auto parentEnt = m_uiScene.createEntity();
    parentEnt.addComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, (uiRes.y / 3.f) * 2.f, uiDepth });

    auto& font = m_sharedResources.fonts.get(FontID::ScoreboardFont);

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Text>(font).setString("Enter Your Name:");
    textEnt.getComponent<cro::Text>().setCharSize(TextMedium);
    textEnt.getComponent<cro::Text>().setColour(textColourSelected);

    auto textSize = textEnt.getComponent<cro::Text>().getLocalBounds();
    parentEnt.getComponent<cro::Transform>().addChild(textEnt.addComponent<cro::Transform>());
    textEnt.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, -textSize.height, 0.f });

    auto boxEnt = m_uiScene.createEntity();
    boxEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("textbox_inactive");
    auto textArea = boxEnt.getComponent<cro::Sprite>().getLocalBounds();
    parentEnt.getComponent<cro::Transform>().addChild(boxEnt.addComponent<cro::Transform>());
    boxEnt.getComponent<cro::Transform>().setOrigin({ textArea.width / 2.f, textArea.height, 0.f });
    boxEnt.addComponent<cro::UIInput>().area = textArea;
    inactiveArea = boxEnt.getComponent<cro::Sprite>().getTextureRect();
    auto activeArea = spriteSheet.getSprite("textbox_active").getTextureRect();
    boxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback(
        [&, activeArea](cro::Entity ent, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || flags & cro::UISystem::Finger)
        {
            SDL_StartTextInput();
            ent.getComponent<cro::Sprite>().setTextureRect(activeArea);
            updateTextBox();
        }
    });
    boxEnt.addComponent<cro::CommandTarget>().ID = UICommand::InputBox;

    auto inputEnt = m_uiScene.createEntity();
    inputEnt.addComponent<cro::Text>(font).setString(names[cro::Util::Random::value(0, names.size()-1)]);
    inputEnt.getComponent<cro::Text>().setCharSize(TextLarge);
    inputEnt.getComponent<cro::Text>().setColour(textColourSelected);
    parentEnt.getComponent<cro::Transform>().addChild(inputEnt.addComponent<cro::Transform>());
    inputEnt.getComponent<cro::Transform>().setPosition({ (-textArea.width / 2.f) + 32.f, -32.f, 0.f });
    inputEnt.addComponent<cro::CommandTarget>().ID = UICommand::NameText;
}

void GameOverState::handleTextEvent(const cro::Event& evt)
{
    auto text = *evt.text.text;

    //send command to text to update it
    cro::Command cmd;
    cmd.targetFlags = UICommand::NameText;
    cmd.action = [&, text](cro::Entity entity, float)
    {
        if (entity.getComponent<cro::Text>().getLocalBounds().width < inactiveArea.width - 100.f)
        {
            m_sharedResources.playerName += text;
            entity.getComponent<cro::Text>().setString(m_sharedResources.playerName + "_");
        }
    };
    commandSystem->sendCommand(cmd);
}

void GameOverState::updateTextBox()
{
    cro::Command cmd;
    cmd.targetFlags = UICommand::NameText;
    cmd.action = [&](cro::Entity entity, float)
    {
        entity.getComponent<cro::Text>().setString(m_sharedResources.playerName + "_");
    };
    commandSystem->sendCommand(cmd);
}