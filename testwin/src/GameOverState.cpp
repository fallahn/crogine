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
#include <crogine/ecs/components/CommandID.hpp>
#include <crogine/ecs/components/UIInput.hpp>

#include <crogine/ecs/systems/SceneGraph.hpp>
#include <crogine/ecs/systems/SpriteRenderer.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/TextRenderer.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Random.hpp>

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
        NameText = 0x2
    };
    cro::CommandSystem* commandSystem = nullptr;
    const float uiDepth = 1.f;

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

bool GameOverState::simulate(cro::Time dt)
{
    cro::Command cmd;
    cmd.targetFlags = UICommand::ScoreText;
    cmd.action = [&](cro::Entity entity, cro::Time)
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
    m_uiScene.addSystem<cro::SceneGraph>(mb);
    m_uiScene.addSystem<cro::SpriteRenderer>(mb);
    m_uiScene.addSystem<cro::TextRenderer>(mb);
    commandSystem = &m_uiScene.addSystem<cro::CommandSystem>(mb);

    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);
    //font.loadFromFile("assets/fonts/Audiowide-Regular.ttf");

    //background image
    cro::Image img;
    img.create(2, 2, cro::Colour(0.f, 0.f, 0.f, 0.8f));
    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData(), false);

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ uiRes.x / 2.f, uiRes.y / 2.f, 0.5f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundTexture);

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Text>(font).setString("GAME OVER");
    entity.getComponent<cro::Text>().setCharSize(90);
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
    gameText.setCharSize(60);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 40.f, 100.f, 0.f });
    gameTextTx.setParent(entity);
    auto iconEnt = m_uiScene.createEntity();
    iconEnt.addComponent<cro::Transform>().setParent(entity);
    iconEnt.getComponent<cro::Transform>().setPosition({ area.width - buttonIconOffset, 0.f, 0.f });
    iconEnt.addComponent<cro::Sprite>() = icons.getSprite("menu");

    auto activeArea = sprites.getSprite("button_active").getTextureRect();
    auto& gameControl = entity.addComponent<cro::UIInput>();
    gameControl.callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback([&,activeArea]
    (cro::Entity e, cro::uint64 flags)
    {
        e.getComponent<cro::Sprite>().setTextureRect(activeArea);
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
    gameControl.callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback([&, area]
    (cro::Entity e, cro::uint64 flags)
    {
        e.getComponent<cro::Sprite>().setTextureRect(area);
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
    gameControl.callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([this]
    (cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            || flags & cro::UISystem::Finger)
        {
            requestStackClear();
            requestStackPush(States::MainMenu);
        }
    });
    gameControl.area.width = area.width;
    gameControl.area.height = area.height;


    auto scoreEnt = m_uiScene.createEntity();
    auto& scoreText = scoreEnt.addComponent<cro::Text>(font);
    scoreText.setString("Score: 0000");
    scoreText.setCharSize(80);
    scoreText.setColour(textColourSelected);
    scoreEnt.addComponent<cro::Transform>().setPosition({uiRes.x / 2.f, (uiRes.y / 3.f) * 2.3f, uiDepth});
    scoreEnt.addComponent<cro::CommandTarget>().ID = UICommand::ScoreText;

    createTextBox(sprites);

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

void GameOverState::createTextBox(const cro::SpriteSheet& spriteSheet)
{
    auto parentEnt = m_uiScene.createEntity();
    parentEnt.addComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, uiRes.y / 2.f, uiDepth });

    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);

    auto textEnt = m_uiScene.createEntity();
    textEnt.addComponent<cro::Text>(font).setString("Enter Your Name:");
    textEnt.getComponent<cro::Text>().setCharSize(50);
    textEnt.getComponent<cro::Text>().setColour(textColourSelected);

    auto textSize = textEnt.getComponent<cro::Text>().getLocalBounds();
    textEnt.addComponent<cro::Transform>().setParent(parentEnt);
    textEnt.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, -textSize.height, 0.f });

    auto boxEnt = m_uiScene.createEntity();
    boxEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("textbox_inactive");
    auto textArea = boxEnt.getComponent<cro::Sprite>().getLocalBounds();
    boxEnt.addComponent<cro::Transform>().setParent(parentEnt);
    boxEnt.getComponent<cro::Transform>().setOrigin({ textArea.width / 2.f, textArea.height, 0.f });

    auto inputEnt = m_uiScene.createEntity();
    inputEnt.addComponent<cro::Text>(font).setString(names[cro::Util::Random::value(0, names.size())]);
    inputEnt.getComponent<cro::Text>().setCharSize(60);
    inputEnt.getComponent<cro::Text>().setColour(textColourSelected);
    inputEnt.addComponent<cro::Transform>().setParent(parentEnt);
    inputEnt.getComponent<cro::Transform>().setPosition({ (-textArea.width / 2.f) + 32.f, -22.f, 0.f });
    inputEnt.addComponent<cro::CommandTarget>().ID = UICommand::NameText;
}