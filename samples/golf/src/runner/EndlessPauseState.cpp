/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "../golf/SharedStateData.hpp"
#include "../golf/MenuConsts.hpp"
#include "../golf/CommonConsts.hpp"

#include "EndlessPauseState.hpp"
#include "EndlessConsts.hpp"
#include "EndlessShared.hpp"

#include <crogine/audio/AudioMixer.hpp>

#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioSystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>

EndlessPauseState::EndlessPauseState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd, els::SharedStateData& esd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_sharedGameData(esd),
    m_uiScene       (context.appInstance.getMessageBus())
{
    context.mainWindow.loadResources([this]() {
        addSystems();
        loadAssets();
        createUI();
    });
}

//public
bool EndlessPauseState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    const auto quitGame = 
        [&]()
        {
            requestStackClear();
            requestStackPush(StateID::Clubhouse);
        };
    const auto restartGame = 
        [&]()
        {
            requestStackPop();
            requestStackPush(StateID::EndlessAttract);
        };
    const auto updateTextPrompt =
        [&](bool controller)
        {
            auto old = m_sharedGameData.lastInput;
            if (controller)
            {
                m_sharedGameData.lastInput = cro::GameController::hasPSLayout(0)
                    ? els::SharedStateData::PS : els::SharedStateData::Xbox;
            }
            else
            {
                m_sharedGameData.lastInput = els::SharedStateData::Keyboard;
            }

            if (old != m_sharedGameData.lastInput)
            {
                for (auto e : m_textPrompt)
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                m_textPrompt[m_sharedGameData.lastInput].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }

            cro::App::getWindow().setMouseCaptured(true);
        };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
        case SDLK_p:
            requestStackPop();
            break;
        case SDLK_c:
            quitGame();
            break;
        case SDLK_q:
            restartGame();
            break;
        }
        updateTextPrompt(false);
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonStart:
        case cro::GameController::ButtonA:
            requestStackPop();
            break;
        case cro::GameController::ButtonB:
            restartGame();
            break;
        case cro::GameController::ButtonBack:
            quitGame();
            break;
        }

        updateTextPrompt(true);
    }

    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > cro::GameController::LeftThumbDeadZone
            || evt.caxis.value < -cro::GameController::LeftThumbDeadZone)
        {
            updateTextPrompt(true);
        }
    }


    m_uiScene.forwardEvent(evt);
    return false;
}

void EndlessPauseState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Pushed
            && data.id == StateID::EndlessPause)
        {
            //HAX
            //refresh the text cos edge cases cause it to garble
            m_pausedText.getComponent<cro::Text>().setString("PAUSED");

            //update controller icons/text
            for (auto e : m_textPrompt)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            m_textPrompt[m_sharedGameData.lastInput].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
    }

    m_uiScene.forwardMessage(msg);
}

bool EndlessPauseState::simulate(float dt)
{
    m_uiScene.simulate(dt);
    return false;
}

void EndlessPauseState::render()
{
    m_uiScene.render();
}

//private
void EndlessPauseState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
    m_uiScene.addSystem<cro::AudioSystem>(mb);
}

void EndlessPauseState::loadAssets()
{
}

void EndlessPauseState::createUI()
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    m_rootNode = entity;

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //paused message
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("PAUSE");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 10);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 40.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_pausedText = entity;

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/runner_menu.spt", m_resources.textures);

    //text prompt
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Esc: Continue\nQ: Quit to Menu\nC: Quit to Clubhouse");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -48.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_textPrompt[PromptID::Keyboard] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Continue\nQuit to Menu\nQuit to Clubhouse");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { -84.f, -48.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_textPrompt[PromptID::Xbox] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -24.f, -64.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("pause_xbox");
    m_textPrompt[PromptID::Xbox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Continue\nQuit to Menu\nQuit to Clubhouse");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { -84.f, -48.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_textPrompt[PromptID::PS] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -24.f, -64.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("pause_ps");
    m_textPrompt[PromptID::PS].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //scaled independently of root node (see callback, below)
    cro::Colour bgColour(0.f, 0.f, 0.f, BackgroundAlpha);
    auto bgEnt = m_uiScene.createEntity();
    bgEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -9.f });
    bgEnt.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, 1.f), bgColour),
            cro::Vertex2D(glm::vec2(0.f), bgColour),
            cro::Vertex2D(glm::vec2(1.f), bgColour),
            cro::Vertex2D(glm::vec2(1.f, 0.f), bgColour),
        }
    );

    auto resize = [&, bgEnt](cro::Camera& cam) mutable
    {
        glm::vec2 size(cro::App::getWindow().getSize());
        cam.viewport = {0.f, 0.f, 1.f, 1.f};
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -1.f, 10.f);

        bgEnt.getComponent<cro::Transform>().setScale(size);

        const float scale = getViewScale(size);
        m_rootNode.getComponent<cro::Transform>().setScale(glm::vec2(scale));

        size /= scale;

        cro::Command cmd;
        cmd.targetFlags = CommandID::UIElement;
        cmd.action =
            [size](cro::Entity e, float)
            {
                const auto& element = e.getComponent<UIElement>();
                auto pos = size * element.relativePosition;
                pos.x = std::round(pos.x);
                pos.y = std::round(pos.y);

                pos += element.absolutePosition;
                e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
            };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    auto& cam = m_uiScene.getActiveCamera().getComponent<cro::Camera>();
    cam.resizeCallback = resize;
    resize(cam);
}