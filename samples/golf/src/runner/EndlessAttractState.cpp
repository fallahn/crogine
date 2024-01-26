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
#include "Social.hpp"

#ifdef USE_GNS
#include "ArcadeLeaderboard.hpp"
#endif

#include "EndlessAttractState.hpp"
#include "EndlessConsts.hpp"
#include "EndlessShared.hpp"

#include <crogine/core/ConfigFile.hpp>

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

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/Constants.hpp>

#include <iomanip>

namespace
{
    const std::string ScoreFile = "endless.scores";
    const cro::Time CycleTime = cro::seconds(8.f);

    struct CycleCallback final
    {
        void operator() (cro::Entity e, float dt)
        {
            static constexpr float Speed = 700.f;
            auto origin = e.getComponent<cro::Transform>().getOrigin();
            if (origin.x < 0)
            {
                origin.x += Speed * dt;
            }
            else
            {
                origin.x = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
            e.getComponent<cro::Transform>().setOrigin(origin);
        }
    };
}

EndlessAttractState::EndlessAttractState(cro::StateStack& stack, cro::State::Context context, SharedStateData& sd, els::SharedStateData& esd)
    : cro::State    (stack, context),
    m_sharedData    (sd),
    m_sharedGameData(esd),
    m_uiScene       (context.appInstance.getMessageBus()),
    m_cycleIndex    (0)
{
    const auto path = Social::getBaseContentPath() + ScoreFile;
    cro::ConfigFile cfg;
    if (cfg.loadFromFile(path))
    {
        if (auto* p = cfg.findProperty("best"); p)
        {
            float best = p->getValue<float>();
            if (best > m_sharedGameData.bestScore)
            {
                m_sharedGameData.bestScore = best;
            }
        }
    }
    
    addSystems();
    loadAssets();
    createUI();
}

//public
bool EndlessAttractState::handleEvent(const cro::Event& evt)
{
    if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    if (cro::ui::wantsMouse() || cro::ui::wantsKeyboard())
    {
        return true;
    }

    const auto startGame = [&]()
        {
            requestStackClear();
            requestStackPush(StateID::EndlessRunner);
        };
    const auto quitGame = [&]()
        {
            requestStackClear();
            requestStackPush(StateID::Clubhouse);
        };

    const auto nextPage = [&]()
        {
            refreshCycle((m_cycleIndex + 1) % CycleEnt::Count);
        };
    const auto prevPage = [&]()
        {
            refreshCycle((m_cycleIndex + (CycleEnt::Count - 1)) % CycleEnt::Count);        
        };

    const auto updateTextPrompt = [&](bool controller)
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
                //do actual update
                refreshPrompt();
            }

            cro::App::getWindow().setMouseCaptured(true);
        };

    if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_LEFT:
            prevPage();
            break;
        case SDLK_RIGHT:
            nextPage();
            break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
#ifndef CRO_DEBUG_
            quitGame();
#endif
            break;
        }

        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            startGame();
        }

        updateTextPrompt(false);
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftShoulder:
            prevPage();
            break;
        case cro::GameController::ButtonRightShoulder:
            nextPage();
            break;
        case cro::GameController::ButtonA:
            startGame();
            break;
        case cro::GameController::ButtonB:
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

void EndlessAttractState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Pushed
            && data.id == StateID::EndlessAttract)
        {
#ifdef USE_GNS
            ArcadeLeaderboard::insertScore(m_sharedGameData.bestScore, "cart");
#endif

            //update controller icons/text
            refreshPrompt();

            refreshHighScores();

            //reset to game over message
            refreshCycle(0);

            //such unicode
            cro::String s(std::uint32_t(0x26D0));
            auto uc = s.toUtf8();
            std::vector<const char*> v;
            v.push_back(reinterpret_cast<const char*>(uc.data()));

            Social::setStatus(Social::InfoID::Menu, v);
        }
    }
#ifdef USE_GNS
    else if (msg.id == Social::MessageID::StatsMessage)
    {
        const auto& data = msg.getData<Social::StatEvent>();
        if (data.type == Social::StatEvent::ArcadeScoreReceived)
        {
            refreshHighScores();
        }
    }
#endif

    m_uiScene.forwardMessage(msg);
}

bool EndlessAttractState::simulate(float dt)
{
    m_uiScene.simulate(dt);
    return true;
}

void EndlessAttractState::render()
{
    m_uiScene.render();
}

//private
void EndlessAttractState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_uiScene.addSystem<cro::CommandSystem>(mb);
    m_uiScene.addSystem<cro::CallbackSystem>(mb);
    m_uiScene.addSystem<cro::TextSystem>(mb);
    m_uiScene.addSystem<cro::SpriteSystem2D>(mb);
    m_uiScene.addSystem<cro::CameraSystem>(mb);
    m_uiScene.addSystem<cro::RenderSystem2D>(mb);
}

void EndlessAttractState::loadAssets()
{
}

void EndlessAttractState::createUI()
{
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            if (m_cycleClock.elapsed() > CycleTime)
            {
                refreshCycle((m_cycleIndex + 1) % m_cycleEnts.size());
            }
        };
    m_rootNode = entity;

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/runner_menu.spt", m_resources.textures);

    //text prompt to start
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, -26.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = TextFlashCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto startTextPrompt = entity;

    const auto TextColour = TextNormalColour;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " to Start");
    entity.getComponent<cro::Text>().setFillColour(TextColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    startTextPrompt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_startPrompts[PromptID::Keyboard] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press  to Start");
    entity.getComponent<cro::Text>().setFillColour(TextColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    startTextPrompt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_startPrompts[PromptID::Xbox] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(2.f));
    entity.getComponent<cro::Transform>().setPosition(glm::vec2(-38.f, -22.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("xbox_a");
    m_startPrompts[PromptID::Xbox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press  to Start");
    entity.getComponent<cro::Text>().setFillColour(TextColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    startTextPrompt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_startPrompts[PromptID::PS] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(2.f));
    entity.getComponent<cro::Transform>().setPosition(glm::vec2(-38.f, -22.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("ps_x");
    m_startPrompts[PromptID::PS].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //text prompt to quit
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 42.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = TextFlashCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto quitTextPrompt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press Escape to Quit");
    entity.getComponent<cro::Text>().setFillColour(TextColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    quitTextPrompt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_quitPrompts[PromptID::Keyboard] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press  to Quit");
    entity.getComponent<cro::Text>().setFillColour(TextColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    quitTextPrompt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_quitPrompts[PromptID::Xbox] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(2.f));
    entity.getComponent<cro::Transform>().setPosition(glm::vec2(-30.f, -24.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("xbox_b");
    m_quitPrompts[PromptID::Xbox].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Press  to Quit");
    entity.getComponent<cro::Text>().setFillColour(TextColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    quitTextPrompt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_quitPrompts[PromptID::PS] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(2.f));
    entity.getComponent<cro::Transform>().setPosition(glm::vec2(-30.f, -24.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("ps_o");
    m_quitPrompts[PromptID::PS].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //Game Over
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("GAME\nOVER");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 10);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 80.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().function = CycleCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_cycleEnts[CycleEnt::GameOver] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -180.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Your Score:");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_cycleEnts[CycleEnt::GameOver].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_gameOverScore = entity;

    //high scores
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("High Scores");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 120.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().function = CycleCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_cycleEnts[CycleEnt::HighScore] = entity;
    
    
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -200.f, -30.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font);// .setString("asd\nasd\nasdsadasd\nasdsad\nwqweqw\nweq\nasddf\nrtytg\nefwerg\ndertg");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    m_cycleEnts[CycleEnt::HighScore].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_highScoreEnts[0] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 200.f, -30.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font);// .setString("12321\n12313\n12313\n1231313\n123313\n12313\n121313\n231313\n123321\n123213");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    m_cycleEnts[CycleEnt::HighScore].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_highScoreEnts[1] = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -200.f, 0.f });;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font);// .setString("3. sadsfsdfd  - 3m45s");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_cycleEnts[CycleEnt::HighScore].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_highScoreEnts[2] = entity;

    //refreshHighScores();

    //how to play
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("How To Play\n\n\n\n\n\n\n\n\nDrive For As Long As Possible!");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 110.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    entity.addComponent<cro::Callback>().function = CycleCallback();
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_cycleEnts[CycleEnt::Rules] = entity;


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(-80.f, -60.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Collect These\nAvoid These");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
    entity.getComponent<cro::Text>().setVerticalSpacing(4.f);
    m_cycleEnts[CycleEnt::Rules].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -50.f, -36.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("how_to");
    textEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //music credits
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Music By retroindiejosh");
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<UIElement>().depth = 0.1f;
    entity.getComponent<UIElement>().absolutePosition = { 4.f, -8.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 1.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UIElement;
    m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


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

void EndlessAttractState::refreshHighScores()
{
    const auto path = Social::getBaseContentPath() + ScoreFile;
    cro::ConfigFile cfg;
    cfg.addProperty("best").setValue(m_sharedGameData.bestScore);
    cfg.save(path);

    
    CRO_ASSERT(m_cycleEnts[CycleEnt::HighScore].isValid(), "");

    const auto toTimeFormat = [](float score)
        {
            std::int32_t mins = static_cast<std::int32_t>(std::floor(score / 60.f));
            auto sec = score - (mins * 60);

            return std::make_pair(mins, sec);
        };


    if (m_sharedGameData.lastScore != 0)
    {
        auto [mins, sec] = toTimeFormat(m_sharedGameData.lastScore);
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2);
        ss << "Your Score: " << mins << "m " << sec << "s\n";
        m_gameOverScore.getComponent<cro::Text>().setString(ss.str());
    }
    else
    {
        m_gameOverScore.getComponent<cro::Text>().setString(" ");
    }

    

#ifdef USE_GNS
    const auto& scores = ArcadeLeaderboard::getScores("cart");
    if (!scores[0].empty())
    {
        m_cycleEnts[CycleEnt::HighScore].getComponent<cro::Text>().setString("High Scores");
        for (auto i = 0u; i < scores.size(); ++i)
        {
            m_highScoreEnts[i].getComponent<cro::Text>().setString(scores[i]);
        }
    }
    else
#endif
    {
        for (auto e : m_highScoreEnts)
        {
            e.getComponent<cro::Text>().setString(" ");
        }

        auto [mins, sec] = toTimeFormat(m_sharedGameData.lastScore);
        auto [mins2, sec2] = toTimeFormat(m_sharedGameData.bestScore);

        std::stringstream ss;
        ss << "High Scores\n";
        ss << std::fixed << std::setprecision(2);

        if (m_sharedGameData.lastScore != 0)
        {
            ss << "\nYour Score: " << mins << "m " << sec << "s\n";
        }
        else
        {
            ss << "...\n\n";
        }

        if (m_sharedGameData.bestScore != 0)
        {
            ss << "All Time Best: " << mins2 << "m " << sec2 << "s";
        }
        m_cycleEnts[CycleEnt::HighScore].getComponent<cro::Text>().setString(ss.str());
    }
}

void EndlessAttractState::refreshCycle(std::size_t newIndex)
{
    m_cycleEnts[m_cycleIndex].getComponent<cro::Transform>().setScale(glm::vec2(0.f));

    m_cycleClock.restart();
    m_cycleIndex = newIndex;

    m_cycleEnts[m_cycleIndex].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    m_cycleEnts[m_cycleIndex].getComponent<cro::Transform>().setOrigin({ -RenderSizeFloat.x, 0.f, 0.f });
    m_cycleEnts[m_cycleIndex].getComponent<cro::Callback>().active = true;
}

void EndlessAttractState::refreshPrompt()
{
    for (auto e : m_startPrompts)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
    m_startPrompts[m_sharedGameData.lastInput].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

    for (auto e : m_quitPrompts)
    {
        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
    m_quitPrompts[m_sharedGameData.lastInput].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
}