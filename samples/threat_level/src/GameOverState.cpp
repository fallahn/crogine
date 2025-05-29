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
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/UIInput.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
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
        auto bounds = cro::Text::getLocalBounds(entity);
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

    m_uiSystem = m_uiScene.addSystem<cro::UISystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);

    commandSystem = m_uiScene.addSystem<cro::CommandSystem>(mb);

    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);

    //background image
    cro::Image img;
    img.create(2, 2, stateBackgroundColour);
    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData(), false);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ uiRes.x / 2.f, uiRes.y / 2.f, 0.5f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Sprite>(m_backgroundTexture);
    entity.addComponent<cro::Drawable2D>();

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("GAME OVER");
    entity.getComponent<cro::Text>().setCharacterSize(TextXL);
    entity.getComponent<cro::Text>().setFillColour(textColourSelected);
    auto textSize = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, (uiRes.y / 4.f) * 3.f, uiDepth });
    entity.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, -textSize.height / 2.f, 0.f });


    //quit button
    cro::SpriteSheet sprites;
    sprites.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);
    cro::SpriteSheet icons;
    icons.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = sprites.getSprite("button_inactive");
    auto area = entity.getComponent<cro::Sprite>().getTextureBounds();
    auto& buttonTx = entity.addComponent<cro::Transform>();
    buttonTx.setOrigin({ area.width / 2.f, area.height / 2.f, 0.f });
    buttonTx.setPosition({ uiRes.x / 2.f, 144.f, uiDepth });

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    auto& gameText = textEnt.addComponent<cro::Text>(font);
    gameText.setString("OK");
    gameText.setFillColour(textColourNormal);
    gameText.setCharacterSize(TextLarge);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 20.f, 80.f, 0.f });
    entity.getComponent<cro::Transform>().addChild(gameTextTx);
    auto iconEnt = m_uiScene.createEntity();
    entity.getComponent<cro::Transform>().addChild(iconEnt.addComponent<cro::Transform>());
    iconEnt.getComponent<cro::Transform>().setPosition({ area.width - buttonIconOffset, 0.f, 0.f });
    iconEnt.addComponent<cro::Sprite>() = icons.getSprite("menu");
    iconEnt.addComponent<cro::Drawable2D>();

    auto activeArea = sprites.getSprite("button_active").getTextureRect();
    auto& gameControl = entity.addComponent<cro::UIInput>();
    gameControl.callbacks[cro::UIInput::Selected] = m_uiSystem->addCallback(
        [&,activeArea, iconEnt, textEnt](cro::Entity e) mutable
    {
        e.getComponent<cro::Sprite>().setTextureRect(activeArea);
        iconEnt.getComponent<cro::Sprite>().setColour(textColourSelected);
        textEnt.getComponent<cro::Text>().setFillColour(textColourSelected);
    });
    gameControl.callbacks[cro::UIInput::Unselected] = m_uiSystem->addCallback(
        [&, area, iconEnt, textEnt](cro::Entity e) mutable
    {
        e.getComponent<cro::Sprite>().setTextureRect(area);
        iconEnt.getComponent<cro::Sprite>().setColour(textColourNormal);
        textEnt.getComponent<cro::Text>().setFillColour(textColourNormal);
    });
    gameControl.callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback([&]
    (cro::Entity, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            //insert name / score into high score list           
            cro::ConfigFile scores;
            if (scores.loadFromFile(cro::App::getPreferencePath() + highscoreFile))
            {
                scores.addProperty(std::to_string(m_sharedResources.score)).setValue(m_sharedResources.playerName);
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
    scoreEnt.addComponent<cro::Drawable2D>();
    auto& scoreText = scoreEnt.addComponent<cro::Text>(font);
    scoreText.setString("Score: 0000");
    scoreText.setCharacterSize(TextXL);
    scoreText.setFillColour(textColourSelected);
    scoreEnt.addComponent<cro::Transform>().setPosition({uiRes.x / 2.f, (uiRes.y / 2.f) * 0.8f, uiDepth});
    scoreEnt.addComponent<cro::CommandTarget>().ID = UICommand::ScoreText;

    createTextBox(sprites);

    //camera
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.setOrthographic(0.f, uiRes.x, 0.f, uiRes.y, -10.1f, 10.f);
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
    parentEnt.addComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, (uiRes.y / 3.f) * 1.8f, uiDepth });

    auto& font = m_sharedResources.fonts.get(FontID::ScoreboardFont);

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Drawable2D>();
    textEnt.addComponent<cro::Text>(font).setString("Enter Your Name:");
    textEnt.getComponent<cro::Text>().setCharacterSize(TextMedium);
    textEnt.getComponent<cro::Text>().setFillColour(textColourSelected);

    auto textSize = cro::Text::getLocalBounds(textEnt);
    parentEnt.getComponent<cro::Transform>().addChild(textEnt.addComponent<cro::Transform>());
    textEnt.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, -textSize.height, 0.f });

    auto boxEnt = m_uiScene.createEntity();
    boxEnt.addComponent<cro::Drawable2D>();
    boxEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("textbox_inactive");
    auto textArea = boxEnt.getComponent<cro::Sprite>().getTextureBounds();
    parentEnt.getComponent<cro::Transform>().addChild(boxEnt.addComponent<cro::Transform>());
    boxEnt.getComponent<cro::Transform>().setOrigin({ textArea.width / 2.f, textArea.height, 0.f });
    boxEnt.getComponent<cro::Transform>().move({ 0.f, -20, 1.f });
    boxEnt.addComponent<cro::UIInput>().area = textArea;
    inactiveArea = boxEnt.getComponent<cro::Sprite>().getTextureRect();
    auto activeArea = spriteSheet.getSprite("textbox_active").getTextureRect();
    boxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiSystem->addCallback(
        [&, activeArea](cro::Entity ent, const cro::ButtonEvent& evt)
    {
        if (activated(evt))
        {
            SDL_StartTextInput();
            ent.getComponent<cro::Sprite>().setTextureRect(activeArea);
            updateTextBox();
        }
    });
    boxEnt.addComponent<cro::CommandTarget>().ID = UICommand::InputBox;

    auto inputEnt = m_uiScene.createEntity();
    inputEnt.addComponent<cro::Drawable2D>();
    inputEnt.addComponent<cro::Text>(font).setString(names[cro::Util::Random::value(0u, names.size()-1)]);
    inputEnt.getComponent<cro::Text>().setCharacterSize(TextLarge);
    inputEnt.getComponent<cro::Text>().setFillColour(textColourSelected);
    parentEnt.getComponent<cro::Transform>().addChild(inputEnt.addComponent<cro::Transform>());
    inputEnt.getComponent<cro::Transform>().setPosition({ (-textArea.width / 2.f) + 42.f, -52.f, 1.f });
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
        if (cro::Text::getLocalBounds(entity).width < inactiveArea.width - 100.f)
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