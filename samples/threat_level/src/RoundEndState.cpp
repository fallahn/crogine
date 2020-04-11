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

#include "RoundEndState.hpp"
#include "MyApp.hpp"
#include "Messages.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/SpriteSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>

#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

namespace
{
#ifdef PLATFORM_DESKTOP
    glm::vec2 uiRes(1920.f, 1080.f);
    //glm::vec2 uiRes(1280.f, 720.f);
#else
    glm::vec2 uiRes(1280.f, 720.f);
#endif //PLATFORM_DESKTOP
    const float uiDepth = 1.f;

    enum UICommand
    {
        ScoreText = 0x1
    };
    cro::CommandSystem* commandSystem = nullptr;

#include "MenuConsts.inl"
}

RoundEndState::RoundEndState(cro::StateStack& stack, cro::State::Context context, ResourcePtr& rc)
    : cro::State        (stack, context),
    m_uiScene           (context.appInstance.getMessageBus()),
    m_sharedResources   (*rc)
{
    cro::Image img;
    img.create(2, 2, stateBackgroundColour);

    m_backgroundTexture.create(2, 2);
    m_backgroundTexture.update(img.getPixelData(), false);

    load();
    updateView();
}

//public
bool RoundEndState::handleEvent(const cro::Event& evt)
{
    m_uiSystem->handleEvent(evt);
    m_uiScene.forwardEvent(evt);
    return false;
}

void RoundEndState::handleMessage(const cro::Message& msg)
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

bool RoundEndState::simulate(float dt)
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

void RoundEndState::render()
{
    m_uiScene.render(cro::App::getWindow());
}

//private
void RoundEndState::load()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiSystem = &m_uiScene.addSystem<cro::UISystem>(mb);
    commandSystem = &m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);

    auto& font = m_sharedResources.fonts.get(FontID::MenuFont);

    //background
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ uiRes.x / 2.f, uiRes.y / 2.f, 0.5f });
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    entity.addComponent<cro::Sprite>().setTexture(m_backgroundTexture);
    entity.addComponent<cro::Drawable2D>();

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("ROUND END");
    entity.getComponent<cro::Text>().setCharacterSize(TextXL);
    entity.getComponent<cro::Text>().setFillColour(textColourSelected);
    auto textSize = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Transform>();
    entity.getComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, (uiRes.y / 4.f) * 3.f, uiDepth });
    entity.getComponent<cro::Transform>().setOrigin({ textSize.width / 2.f, -textSize.height / 2.f, 0.f });


    //OK Button
    cro::SpriteSheet sprites;
    sprites.loadFromFile("assets/sprites/ui_menu.spt", m_sharedResources.textures);
    /*cro::SpriteSheet icons;
    icons.loadFromFile("assets/sprites/ui_icons.spt", m_sharedResources.textures);*/

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
    gameText.setString("Continue...");
    gameText.setFillColour(textColourNormal);
    gameText.setCharacterSize(TextLarge);
    auto& gameTextTx = textEnt.addComponent<cro::Transform>();
    gameTextTx.setPosition({ 20.f, 80.f, 0.f });
    entity.getComponent<cro::Transform>().addChild(gameTextTx);
    /*auto iconEnt = m_uiScene.createEntity();
    iconEnt.addComponent<cro::Transform>().setParent(entity);
    iconEnt.getComponent<cro::Transform>().setPosition({ area.width - buttonIconOffset, 0.f, 0.f });
    iconEnt.addComponent<cro::Sprite>() = icons.getSprite("continue");*/

    auto activeArea = sprites.getSprite("button_active").getTextureRect();
    auto& gameControl = entity.addComponent<cro::UIInput>();
    gameControl.callbacks[cro::UIInput::MouseEnter] = m_uiSystem->addCallback(
        [&, activeArea, textEnt](cro::Entity e, glm::vec2) mutable
    {
        e.getComponent<cro::Sprite>().setTextureRect(activeArea);
        textEnt.getComponent<cro::Text>().setFillColour(textColourSelected);
    });
    gameControl.callbacks[cro::UIInput::MouseExit] = m_uiSystem->addCallback(
        [&, area, textEnt] (cro::Entity e, glm::vec2) mutable
    {
        e.getComponent<cro::Sprite>().setTextureRect(area);
        textEnt.getComponent<cro::Text>().setFillColour(textColourNormal);
    });
    gameControl.callbacks[cro::UIInput::MouseUp] = m_uiSystem->addCallback([&]
    (cro::Entity, cro::uint64 flags)
    {
        if ((flags & cro::UISystem::LeftMouse)
            /*|| flags & cro::UISystem::Finger*/)
        {
            //TODO continue to next round or load next map
            //TODO change background colour on round change
            requestStackPop();
            auto* msg = getContext().appInstance.getMessageBus().post<GameEvent>(MessageID::GameMessage);
            msg->type = GameEvent::RoundStart;
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
    scoreEnt.addComponent<cro::Transform>().setPosition({ uiRes.x / 2.f, (uiRes.y / 2.f) * 1.1f, uiDepth });
    scoreEnt.addComponent<cro::CommandTarget>().ID = UICommand::ScoreText;


    //camera
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    auto& cam2D = entity.addComponent<cro::Camera>();
    cam2D.projectionMatrix = glm::ortho(0.f, uiRes.x, 0.f, uiRes.y, -10.1f, 10.f);
    m_uiScene.setActiveCamera(entity);
}

void RoundEndState::updateView()
{
    glm::vec2 size(cro::App::getWindow().getSize());
    size.y = ((size.x / 16.f) * 9.f) / size.y;
    size.x = 1.f;

    auto& cam2D = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam2D.viewport.bottom = (1.f - size.y) / 2.f;
    cam2D.viewport.height = size.y;
}