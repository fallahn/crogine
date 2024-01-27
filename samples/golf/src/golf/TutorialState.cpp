/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2024
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

#include "TutorialState.hpp"
#include "SharedStateData.hpp"
#include "MenuConsts.hpp"
#include "CommonConsts.hpp"
#include "GameConsts.hpp"
#include "CommandIDs.hpp"
#include "MessageIDs.hpp"
#include "../ErrorCheck.hpp"

#include <Input.hpp>

#include <crogine/gui/Gui.hpp>

#include <crogine/core/Mouse.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/util/String.hpp>
#include <crogine/detail/glm/gtx/quaternion.hpp>

namespace
{
    glm::vec2 spinAmount = glm::vec2(0.f);
    bool showSpin = false;

    std::int32_t timeUniform = -1;
    std::uint32_t shaderID = 0;

    std::uint32_t inputMask = 0;

    struct BackgroundCallbackData final
    {
        glm::vec2 targetSize = glm::vec2(0.f);
        float progress = 0.f;
        enum
        {
            In, Out
        }state = In;

        explicit BackgroundCallbackData(glm::vec2 t) : targetSize(t) {}
    };
}

TutorialState::TutorialState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_sharedData    (sd),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_viewScale     (2.f),
    m_mouseVisible  (true),
    m_currentAction (0),
    m_actionActive  (false)
{
    inputMask = 0;
    ctx.mainWindow.setMouseCaptured(true);

    spinAmount = glm::vec2(0.f);
    showSpin = false;

    buildScene();

    //registerWindow([&]()
    //    {
    //        if (ImGui::Begin("flaps"))
    //        {
    //            auto pos = m_backgroundEnt.getComponent<cro::Transform>().getPosition();
    //            ImGui::Text("%3.3f, %3.3f", pos.x, pos.y);
    //        }
    //        ImGui::End();
    //    });
}

//public
bool TutorialState::handleEvent(const cro::Event& evt)
{
    if (cro::ui::wantsKeyboard() || cro::ui::wantsMouse())
    {
        return false;
    }

    auto updateButtonIcon = [&](std::int32_t controllerID)
    {
        std::int32_t animID = 0;
        if (IS_PS(controllerID))
        {
            animID = 1;
        }

        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::PlayerIcon;
        cmd.action = [animID](cro::Entity e, float)
        {
            e.getComponent<cro::SpriteAnimation>().play(animID);
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default:
            /*if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
            {
                doCurrentAction();
            }*/
            break;
        case SDLK_p:
        case SDLK_ESCAPE:
        case SDLK_BACKSPACE:
        case SDLK_PAUSE:
            requestStackPush(StateID::Pause);
            break;
            //make sure system buttons don't do anything
        case SDLK_F1:
        case SDLK_F5:

            break;
#ifdef CRO_DEBUG_
        case SDLK_o:
            quitState();
            break;
#endif
        }

        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::SpinMenu])
        {
            showSpin = false;
        }
    }
    else if (evt.type == SDL_KEYDOWN)
    {
        if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Action])
        {
            doCurrentAction();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::SpinMenu])
        {
            showSpin = true;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
            doCurrentAction();
            break;
        case cro::GameController::ButtonStart:
            requestStackPush(StateID::Pause);
            break;
        case cro::GameController::ButtonX:
            showSpin = false;
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.button == cro::GameController::ButtonA)
        {
            doCurrentAction();
        }
        else if (evt.cbutton.button == cro::GameController::ButtonX)
        {
            showSpin = true;
        }
        updateButtonIcon(cro::GameController::controllerID(evt.cbutton.which));
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
        m_mouseVisible = true;
        m_mouseClock.restart();
    }

    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        updateButtonIcon(cro::GameController::controllerID(evt.caxis.which));
        
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
            m_mouseVisible = false;
        }

        if (evt.caxis.value > TriggerDeadZone)
        {
            if (evt.caxis.axis == cro::GameController::TriggerLeft
                || evt.caxis.axis == cro::GameController::TriggerRight)
            {
                inputMask |= (1 << evt.caxis.axis);
            }
        }
        else
        {
            if (evt.caxis.axis == cro::GameController::TriggerLeft
                || evt.caxis.axis == cro::GameController::TriggerRight)
            {
                inputMask &= ~(1 << evt.caxis.axis);
            }
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            inputMask |= (1 << evt.button.button);
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            inputMask &= ~(1 << evt.button.button);
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void TutorialState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && data.id == StateID::Pause)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (msg.id == cro::Message::SpriteAnimationMessage)
    {
        //used to modify animation speed of thumbstick sprite
        const auto& data = msg.getData<cro::Message::SpriteAnimationEvent>();
        if (data.userType == 10)
        {
            auto e = data.entity;
            e.getComponent<cro::SpriteAnimation>().playbackRate = 2.2f;
        }
        else if(data.userType == 11)
        {
            auto e = data.entity;
            e.getComponent<cro::SpriteAnimation>().playbackRate = 1.f;
        }
    }

    m_scene.forwardMessage(msg);
}

bool TutorialState::simulate(float dt)
{
    static float accum = 0.f;
    accum += dt;

    glCheck(glUseProgram(shaderID));
    glCheck(glUniform1f(timeUniform, accum * 10.f));
    glCheck(glUseProgram(0));

    if (showSpin)
    {
        auto speed = dt * 2.f;
        auto id = activeControllerID(0);
        if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Left])
            || cro::GameController::isButtonPressed(id, cro::GameController::DPadLeft))
        {
            spinAmount.x = std::min(1.f, spinAmount.x + speed);
        }

        if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Right])
            || cro::GameController::isButtonPressed(id, cro::GameController::DPadRight))
        {
            spinAmount.x = std::max(-1.f, spinAmount.x - speed);
        }

        if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Up])
            || cro::GameController::isButtonPressed(id, cro::GameController::DPadUp))
        {
            spinAmount.y = std::min(1.f, spinAmount.y + speed);
        }

        if (cro::Keyboard::isKeyPressed(m_sharedData.inputBinding.keys[InputBinding::Down])
            || cro::GameController::isButtonPressed(id, cro::GameController::DPadDown))
        {
            spinAmount.y = std::max(-1.f, spinAmount.y - speed);
        }
    }

    m_scene.simulate(dt);

    //auto hide the mouse if not paused
    if (m_mouseVisible
        && getStateCount() == 2)
    {
        if (m_mouseClock.elapsed() > MouseHideTime
            && !ImGui::GetIO().WantCaptureMouse)
        {
            m_mouseVisible = false;
            cro::App::getWindow().setMouseCaptured(true);
        }
    }

    return true;
}

void TutorialState::render()
{
    float currWidth = 0.f;
    glGetFloatv(GL_LINE_WIDTH, &currWidth);
    
    glLineWidth(m_viewScale.y);
    glEnable(GL_LINE_SMOOTH);
    m_scene.render();
    glDisable(GL_LINE_SMOOTH);
    glLineWidth(currWidth);
}

//private
void TutorialState::buildScene()
{
    //add systems
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);


    m_audioScape.loadFromFile("assets/golf/sound/tutorial.xas", m_sharedData.sharedResources->audio);

    
    if (cro::GameController::getControllerCount() > 0)
    {
        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);

        //we'll play an aniation for the correct frame based on controller movement
        m_buttonSprites[ButtonID::A] = spriteSheet.getSprite("button_a");
        m_buttonSprites[ButtonID::B] = spriteSheet.getSprite("button_b");
        m_buttonSprites[ButtonID::X] = spriteSheet.getSprite("button_x");
        m_buttonSprites[ButtonID::Y] = spriteSheet.getSprite("button_y");
        m_buttonSprites[ButtonID::Select] = spriteSheet.getSprite("button_select");
        m_buttonSprites[ButtonID::Start] = spriteSheet.getSprite("button_start");
        m_buttonSprites[ButtonID::LT] = spriteSheet.getSprite("button_lt");
        m_buttonSprites[ButtonID::RT] = spriteSheet.getSprite("button_rt");
    }

    //used to animate the slope line
    auto& shader = m_sharedData.sharedResources->shaders.get(ShaderID::TutorialSlope);
    timeUniform = shader.getUniformID("u_time");
    shaderID = shader.getGLHandle();

    //root node is used to scale all attachments
    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();

    //load background
    cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    glm::vec2 size = GolfGame::getActiveTarget()->getSize();

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, size.y), c),
        cro::Vertex2D(glm::vec2(0.f), c),
        cro::Vertex2D(size, c),
        cro::Vertex2D(glm::vec2(size.x, 0.f), c)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<BackgroundCallbackData>(size / m_viewScale);
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float dt) mutable
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<BackgroundCallbackData>();
        data.progress = std::min(1.f, data.progress + (dt * 2.f));

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        verts[0].position.y = data.targetSize.y;
        verts[2].position.y = data.targetSize.y;

        switch (data.state)
        {
        default: break;
        case BackgroundCallbackData::In:
        {
            verts[2].position.x = data.targetSize.x * cro::Util::Easing::easeOutCubic(data.progress);
            verts[3].position.x = verts[2].position.x;

            if (data.progress == 1)
            {
                e.getComponent<cro::Drawable2D>().updateLocalBounds();

                data.state = BackgroundCallbackData::Out;
                data.progress = 0.f;
                e.getComponent<cro::Callback>().active = false;
            }
        }
        break;
        case BackgroundCallbackData::Out:
            //shift the root node out instead
            //so all our items leave.
            auto position = rootNode.getComponent<cro::Transform>().getPosition();
            position.x = (data.targetSize.x * m_viewScale.x) * cro::Util::Easing::easeInCubic(data.progress);
            rootNode.getComponent<cro::Transform>().setPosition(position);

            if (data.progress == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                requestStackPop();
            }
            break;
        }
    };
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_backgroundEnt = entity;

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setFillColour(cro::Colour::Transparent);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 32.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_messageEnt = entity;

    playSound("wipe");

    cro::String str;
    if (cro::GameController::getControllerCount() != 0)
    {
        str = "Press  to Continue";

        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 40.f, -12.f, 0.1f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::A];
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(IS_PS(activeControllerID(0)) ? 1 : 0);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function =
            [entity](cro::Entity e, float)
        {
            auto a = entity.getComponent<cro::Text>().getFillColour().getAlpha();
            cro::Colour c(1.f, 1.f, 1.f, a);
            e.getComponent<cro::Sprite>().setColour(c);
        };
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        str = "Press " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " to Continue";
    }
    entity.getComponent<cro::Text>().setString(str);
    centreText(entity);


    //create the layout depending on the requested tutorial
    //tutorialPutt(rootNode);
    //tutorialSpin(rootNode);
    switch (m_sharedData.tutorialIndex)
    {
    default:
        quitState();
        break;
    case TutorialID::One:
        tutorialOne(rootNode);
        break;
    case TutorialID::Two:
        tutorialTwo(rootNode);
        break;
    case TutorialID::Three:
        tutorialThree(rootNode);
        break;
    case TutorialID::Swing:
        tutorialSwing(rootNode);
        break;
    case TutorialID::Putt:
        tutorialPutt(rootNode);
        break;
    case TutorialID::Spin:
        tutorialSpin(rootNode);
        break;
    }

    //set up camera / resize callback
    auto camCallback = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size = GolfGame::getActiveTarget()->getSize();
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.5f, 5.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);

        size /= m_viewScale;

        //if animation active set target size,
        //otherwise set the size explicitly
        if (m_backgroundEnt.getComponent<cro::Callback>().active)
        {
            m_backgroundEnt.getComponent<cro::Callback>().getUserData<BackgroundCallbackData>().targetSize = size;
        }
        else
        {
            auto& verts = m_backgroundEnt.getComponent<cro::Drawable2D>().getVertexData();
            verts[0].position = { 0.f, size.y };
            verts[2].position = size;
            verts[3].position = { size.x, 0.f };
        }

        //update any UI layout
        cro::Command cmd;
        cmd.targetFlags = CommandID::UI::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };
    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
    camCallback(cam);
    cam.resizeCallback = camCallback;
}

void TutorialState::tutorialOne(cro::Entity root)
{
    //set up layout
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    

    //second tip text
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -200.f };
    entity.getComponent<UIElement>().relativePosition = { 0.1f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;

    if (cro::GameController::getControllerCount() > 0)
    {
        entity.getComponent<cro::Text>().setString("This is your selected club.\nUse      and      to cycle through them\nand find one with an appropriate distance.");
        
        //attach bumper graphics
        auto callback = [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 4.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };

        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);
        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 19.f, -24.f, 0.1f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_lb");
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(IS_PS(activeControllerID(0)) ? 1 : 0);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function = callback;
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

        buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 61.f, -24.f, 0.1f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_rb");
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(IS_PS(activeControllerID(0)) ? 1 : 0);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function = callback;
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str("This is your selected club.\nUse ");
        str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]) + " and ";
        str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) + " to cycle through them\nand find one with an appropriate distance.";
        entity.getComponent<cro::Text>().setString(str);
    }

    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, 0.f, bounds.width * currTime, -bounds.height * currTime);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };

    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text02 = entity;

    //second tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f, -170.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, -170.f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -24.f };
    entity.getComponent<UIElement>().relativePosition = { 0.1f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [text02](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -1.f, 0.f, 2.f, -170.f * currTime };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            text02.getComponent<cro::Callback>().active = true;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto arrow02 = entity;


    //first tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("This is the distance to the hole.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -100.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    float width = cro::Text::getLocalBounds(entity).width;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&,width](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, 0.f, width * currTime, -14.f);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    auto text01 = entity;

    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //first tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f, -70.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, -70.f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -24.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [text01](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -1.f, 0.f, 2.f, -70.f * currTime };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            text01.getComponent<cro::Callback>().active = true;
        }
    };
    auto arrow01 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //moves the background down
    entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().function =
        [&, arrow01](cro::Entity e, float dt) mutable
    {
        auto pos = m_backgroundEnt.getComponent<cro::Transform>().getPosition();
        auto diff = -UIBarHeight - pos.y;
        pos.y += diff * (dt * 3.f);
        m_backgroundEnt.getComponent<cro::Transform>().setPosition(pos);

        if (diff > -1)
        {
            e.getComponent<cro::Callback>().active = false;

            //show first tip
            arrow01.getComponent<cro::Callback>().active = true;
        }
    };
    auto bgMover = entity;


    //welcome title
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/tutorial.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10000.f, 10000.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height / 3.f) });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 0);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        if (!m_backgroundEnt.getComponent<cro::Callback>().active)
        {
            auto size = glm::vec2(GolfGame::getActiveTarget()->getSize()) / m_viewScale;
            auto& [currTime, state] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();

            switch (state)
            {
            default: break;
            case 0:
            {
                //move in
                auto oldTime = currTime;
                currTime = std::min(1.f, currTime + (dt * 2.f));

                auto position = cro::Util::Easing::easeOutBounce(currTime);
                e.getComponent<cro::Transform>().setPosition({ size.x / 2.f, size.y - ((size.y / 2.f) * position) });

                static constexpr float TriggerTime = 0.1f;
                if (oldTime < TriggerTime && currTime >= TriggerTime)
                {
                    playSound("appear");
                }

                if (currTime == 1)
                {
                    showContinue();
                    state = 1;
                }
            }
                break;
            case 1:
                e.getComponent<cro::Transform>().setPosition(size / 2.f);
                break;
            case 2:
            {
                currTime = std::max(0.f, currTime - (dt * 4.f));

                auto scale = cro::Util::Easing::easeOutQuad(currTime);
                e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
                e.getComponent<cro::Transform>().rotate(dt * 13.f * (2.f - scale));

                if (currTime == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(e);
                }
            }
                break;
            }
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto title = entity;

    spriteSheet.loadFromFile("assets/golf/sprites/main_menu.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 207.f, 64.f, 0.001f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, title](cro::Entity e, float)
    {
        if (title.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };
    title.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), 64.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("super");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height * 0.7f) });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds, title](cro::Entity e, float dt)
    {
        auto [_, state] = title.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();

        if (state == 1)
        {
            auto crop = e.getComponent<cro::Drawable2D>().getCroppingArea();
            crop.height = bounds.height;
            crop.width = std::min(bounds.width, crop.width + (bounds.width * dt));
            e.getComponent<cro::Drawable2D>().setCroppingArea(crop);
        }
        else if(title.destroyed())
        {
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };
    title.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //activates first tips
    m_actionCallbacks.push_back([bgMover, title]() mutable
        {
            bgMover.getComponent<cro::Callback>().active = true; 
            title.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 2;
        });

    //shows the next set of messages
    m_actionCallbacks.push_back([arrow02]() mutable { arrow02.getComponent<cro::Callback>().active = true; });

    //quits
    m_actionCallbacks.push_back([&]() { quitState(); });
}

void TutorialState::tutorialTwo(cro::Entity root)
{
    /*
    The wind indicator shows which way the wind is blowing
    and how strong it currently is between 0 - 2 knots

    Use X and X to aim your shot accounting for the wind

    */
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //direction tip text
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<UIElement>().absolutePosition = { -60.f, 8.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;

    if (cro::GameController::getControllerCount() > 0)
    {
        entity.getComponent<cro::Text>().setString("This is your aim.\nUse the     to move it left or right,\nand compensate for the wind.");

        //attach dpad graphic
        auto callback = [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 4.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };

        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);
        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 39.f, -24.f, 0.1f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("dpad");
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function = callback;
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str("This is your aim.\nUse ");
        str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]) + " and ";
        str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]) + " to move it left or right,\nand compensate for the wind.";
        entity.getComponent<cro::Text>().setString(str);
    }

    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, 0.f, bounds.width * currTime, -bounds.height * currTime);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text02 = entity;
    

    //direction tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f, -20.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, -20.f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -30.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [text02](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -1.f, 0.f, 2.f, -20.f * currTime };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            text02.getComponent<cro::Callback>().active = true;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto arrow02 = entity;



    //wind indicator text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    if (m_sharedData.imperialMeasurements)
    {
        entity.addComponent<cro::Text>(font).setString("This is the wind indicator.\nThe strength ranges from\n0 to 2.2 mph.\nThe higher your ball goes\nthe more the wind affects it.");
    }
    else
    {
        entity.addComponent<cro::Text>(font).setString("This is the wind indicator.\nThe strength ranges from\n0 to 3.5 kph.\nThe higher your ball goes\nthe more the wind affects it.");
    }
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().absolutePosition = { /*-(bounds.width + 20.f)*/20.f, 252.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, 0.f, bounds.width * currTime, -bounds.height *  currTime);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    auto text01 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //wind indicator icon
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("wind_dir");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<UIElement>().absolutePosition = { WindIndicatorPosition.x + 3.f, WindIndicatorPosition.y + 24.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 2.f));

        float scale = cro::Util::Easing::easeOutBounce(currTime);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto icon = entity;

    //wind indicator arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f, -84.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, -84.f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { WindIndicatorPosition.x + 3.f, 194.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&,text01,icon](cro::Entity e, float dt) mutable
    {
        if (!m_backgroundEnt.getComponent<cro::Callback>().active)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + (dt * 4.f));
            cro::FloatRect area = { -1.f, 0.f, 2.f, -94.f * currTime };
            e.getComponent<cro::Drawable2D>().setCroppingArea(area);

            if (currTime == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                text01.getComponent<cro::Callback>().active = true;
                icon.getComponent<cro::Callback>().active = true;

                playSound("appear");
            }
        }
    };
    entity.getComponent<cro::Callback>().active = true;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //first action shows the next set of messages
    m_actionCallbacks.push_back([arrow02]() mutable { arrow02.getComponent<cro::Callback>().active = true; });
    m_actionCallbacks.push_back([&]() { quitState(); });
}

void TutorialState::tutorialThree(cro::Entity root)
{
    /*
    power bar indicator appears at bottom and moves to middle of screen
    upward motion is continued with first arrow / tip in the middle:

    This is the stroke indicatior


    next tip moves left to right, text then arrow

    Press A to activate the swing

    as the power bar reaches the right, draw next tip, left to right,
    starting with arrow.

    Press A to select power.

    start the hook/slice indicator moving, then move down, arrow then text
    aligned centre.

    Press A when the hook/slice bar is centred.

    */
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    std::int32_t animID = 0;
    if (IS_PS(activeControllerID(0)))
    {
        animID = 1;
    }

    cro::SpriteSheet uiSprites;
    uiSprites.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);


    //cancel tip text
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f,0.f,0.f,0.f });
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    if (cro::GameController::getControllerCount() != 0)
    {
        entity.getComponent<cro::Text>().setString("Press     at any time to cancel the shot");
        centreText(entity);

        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 32.f, -13.f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::B];
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(animID);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function =
            [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 4.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str = "Press " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::CancelShot]) + " at any time to cancel the shot";
        entity.getComponent<cro::Text>().setString(str);
        centreText(entity);
    }
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, std::floor(bounds.height / 2.f) - 110.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        auto area = bounds;
        area.width *= currTime;
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };

    auto cancelEnt = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //fourth tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -50.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;

    if (cro::GameController::getControllerCount() != 0)
    {
        entity.getComponent<cro::Text>().setString("Press     when the hook/slice\nindicator is as close\nto the centre as possible.");

        //attach button graphics
        auto callback = [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 4.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };

        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 32.f, -12.f, 0.1f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::A];
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(animID);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function = callback;
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str("Press ");
        str += cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " when the hook/slice\nindicator is as close\nto the centre as possible.";
        entity.getComponent<cro::Text>().setString(str);
    }

    centreText(entity);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, 0.f, bounds.width * currTime, -bounds.height * currTime);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };

    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text04 = entity;

    //fourth tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f, -20.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, -20.f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -16.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [text04](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -1.f, 0.f, 2.f, -20.f * currTime };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            text04.getComponent<cro::Callback>().active = true;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto arrow04 = entity;



    //used as a 'ping' by the hook/slice indicator
    auto pingEnt = m_scene.createEntity();
    pingEnt.addComponent<cro::Transform>();
    pingEnt.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-6.f, 24.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-6.f, -24.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(6.f, 24.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(6.f, -24.f), TextNormalColour)
    };
    pingEnt.getComponent<cro::Drawable2D>().updateLocalBounds();
    pingEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    pingEnt.getComponent<UIElement>().depth = -0.06f;
    pingEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;


    //hook/slice indicator
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = uiSprites.getSprite("hook_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    
    struct HookData final
    {
        float currentTime = 0.5f; //item starts at centre
        float direction = 1.f;
        const float Width = 0.f;
        explicit HookData(float w) : Width(w) {}
    };
    entity.addComponent<cro::Callback>().function =
        [&,pingEnt](cro::Entity e, float dt) mutable
    {
        //it's important that the callback data here is
        //set when the entity is parented to the power bar
        //at the bottom of this function!
        auto& data = e.getComponent<cro::Callback>().getUserData<HookData>();


        bool prev = ((0.5f - data.currentTime) * data.direction > 0);
        data.currentTime = std::min(1.f, std::max(0.f, data.currentTime + (dt * data.direction)));
        bool current = ((0.5f - data.currentTime) * data.direction < 0);

        auto& tx = e.getComponent<cro::Transform>();
        auto position = tx.getPosition();
        position.x = data.Width * data.currentTime;
        tx.setPosition(position);

        if (prev && current)
        {
            //we crossed the centre
            pingEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        }

        if (std::fmod(data.currentTime, 1.f) == 0)
        {
            data.direction *= -1.f;
        }
    };
    //this ent is parented to power bar, below
    auto hookBar = entity;

    pingEnt.addComponent<cro::Callback>().active = true;
    pingEnt.getComponent<cro::Callback>().setUserData<float>(1.f);
    pingEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 1.5f));

        float alpha = 1.f - currTime;
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }

        e.getComponent<cro::Transform>().setScale(glm::vec2(currTime));
    };
    root.getComponent<cro::Transform>().addChild(pingEnt.getComponent<cro::Transform>());


    //third tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f,0.f,0.f,0.f });
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    if (cro::GameController::getControllerCount() != 0)
    {
        entity.getComponent<cro::Text>().setString("Press     to\nselect the power");

        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 32.f, -11.f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::A];
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(animID);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function =
            [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 4.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str = "Press " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " to\nselect the power";
        entity.getComponent<cro::Text>().setString(str);
    }
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 80.f, std::floor(bounds.height / 2.f) };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        auto area = bounds;
        area.width *= currTime;
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };

    auto text03 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //third tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.f, -0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(20.f, 0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(20.f, -0.5f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 48.f, 0.5f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [text03](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -0.f, -1.f, 20.f * currTime, 2.f };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            text03.getComponent<cro::Callback>().active = true;
        }
    };
    auto arrow03 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //power bar inner
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f,0.f,0.f,0.f });
    entity.addComponent<cro::Sprite>() = uiSprites.getSprite("power_bar_inner");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().setUserData<std::pair<float, float>>(0.f, 1.f);
    entity.getComponent<cro::Callback>().function =
        [bounds, arrow03](cro::Entity e, float dt) mutable
    {
        auto& [currTime, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, float>>();
        currTime = std::max(0.f, std::min(currTime + (dt * direction), 1.f));

        auto area = bounds;
        area.width *= cro::Util::Easing::easeInSine(currTime);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            arrow03.getComponent<cro::Callback>().active = true;
        }

        if (std::fmod(currTime, 1.f) == 0)
        {
            direction *= -1.f;
        }
    };

    //this isn't parented to the root, rather the power bar graphic, below
    auto powerbarInner = entity;

    //second tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.f, -0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(20.f, 0.5f), TextNormalColour),
        cro::Vertex2D(glm::vec2(20.f, -0.5f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { -68.f, 0.5f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [powerbarInner](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -0.f, -1.f, 20.f * currTime, 2.f };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            powerbarInner.getComponent<cro::Callback>().active = true;
        }
    };
    auto arrow02 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //second tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f,0.f,0.f,0.f });
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    if (cro::GameController::getControllerCount() != 0)
    {
        entity.getComponent<cro::Text>().setString("Press     to\nactivate the swing");

        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 32.f, -11.f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::A];
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(animID);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function = 
            [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 4.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str = "Press " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Action]) + " to\nactivate the swing";
        entity.getComponent<cro::Text>().setString(str);
    }
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { -(bounds.width + 80.f), std::floor(bounds.height / 2.f) };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [bounds, arrow02](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        auto area = bounds;
        area.width *= currTime;
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            arrow02.getComponent<cro::Callback>().active = true;
        }
    };

    auto text02 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //first tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("This is the stroke indicator.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 52.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    auto text01 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //first tip arrow
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 20.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(-0.5f, 0.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 20.f), TextNormalColour),
        cro::Vertex2D(glm::vec2(0.5f, 0.f), TextNormalColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 16.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [text01](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 4.f));
        cro::FloatRect area = { -1.f, 0.f, 2.f, 20.f * currTime };
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            text01.getComponent<cro::Callback>().active = true;
        }
    };
    auto arrow01 = entity;
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //power bar graphic - other parts of this are parented to this
    //though they are created above so they may be captured in callbacks.
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({-40.f, 0.f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = uiSprites.getSprite("power_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    
    struct PowerbarData final
    {
        float currentTime = 0.f;
        enum
        {
            Wait, In, Hold
        }state = Wait;
    };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<PowerbarData>();
    entity.getComponent<cro::Callback>().function =
        [&, arrow01](cro::Entity e, float dt) mutable
    {
        auto& data = e.getComponent<cro::Callback>().getUserData<PowerbarData>();
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize()) / m_viewScale;
        switch (data.state)
        {
        default: break;
        case PowerbarData::Wait:

            if (!m_backgroundEnt.getComponent<cro::Callback>().active)
            {
                data.state = PowerbarData::In;
            }
            break;
        case PowerbarData::In:
            data.currentTime = std::min(1.f, data.currentTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setPosition({ size.x / 2.f, data.currentTime * (size.y / 2.f) });

            if (data.currentTime == 1)
            {
                //activate first label
                data.state = PowerbarData::Hold;
                arrow01.getComponent<cro::Callback>().active = true;
            }

            break;
        case PowerbarData::Hold:
            e.getComponent<cro::Transform>().setPosition(size / 2.f);
            break;
        }
    };
    entity.getComponent<cro::Transform>().addChild(powerbarInner.getComponent<cro::Transform>());
    
    //apply some properties to the hook bar
    hookBar.getComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, 0.2f });
    hookBar.getComponent<cro::Callback>().setUserData<HookData>(bounds.width);

    entity.getComponent<cro::Transform>().addChild(hookBar.getComponent<cro::Transform>());
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    m_actionCallbacks.push_back([text02]() mutable {text02.getComponent<cro::Callback>().active = true; });
    m_actionCallbacks.push_back([arrow04, hookBar, powerbarInner]() mutable 
        {
            powerbarInner.getComponent<cro::Callback>().active = false;
            hookBar.getComponent<cro::Callback>().active = true;
            arrow04.getComponent<cro::Callback>().active = true;
        });
    m_actionCallbacks.push_back([cancelEnt, hookBar]() mutable
        {
            hookBar.getComponent<cro::Callback>().active = false;
            cancelEnt.getComponent<cro::Callback>().active = true;
        });
    m_actionCallbacks.push_back([&]() mutable
        {
            auto e = m_scene.createEntity();
            e.addComponent<cro::Callback>().active = true;
            e.getComponent<cro::Callback>().setUserData<float>(0.f);
            e.getComponent<cro::Callback>().function =
                [&](cro::Entity d, float dt)
            {
                auto& currTime = d.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + dt);
                if (currTime == 1)
                {
                    d.getComponent<cro::Callback>().active = false;
                    m_scene.destroyEntity(d);
                    quitState();
                }
            };
        });
}

void TutorialState::tutorialPutt(cro::Entity root)
{
    /*
    Blue lines on the ground indicate the direction of the slope

    The longer the lines and the bluer they are the strong the effect of the slope.
    */
    m_sharedData.showPuttingPower = true;

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);


    //first tip text
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("The coloured lines indicate the direction of the green's slope.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 66.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (!m_backgroundEnt.getComponent<cro::Callback>().active)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + (dt * 3.f));
            cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
            e.getComponent<cro::Drawable2D>().setCroppingArea(area);

            if (currTime == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                showContinue();
            }
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //slope line
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-60.f, 5.f), cro::Colour(1.f, 0.f, 0.9f, 0.f)),
        cro::Vertex2D(glm::vec2(60.f, -5.f), cro::Colour(0.f, 0.05f, 1.f, 0.f))
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_LINES);
    entity.getComponent<cro::Drawable2D>().setShader(&m_sharedData.sharedResources->shaders.get(ShaderID::TutorialSlope));
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 10.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + dt);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        verts[0].colour.setAlpha(currTime * 0.5f);
        verts[1].colour.setAlpha(currTime);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto lineEnt = entity;

    //line background
    auto colour = cro::Colour(0.576f,0.671f,0.322f, 0.8f);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({0.f, 0.f, -0.1f});
    entity.getComponent<cro::Transform>().setScale({ 1.f, 0.f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-64.f, 16.f), colour),
        cro::Vertex2D(glm::vec2(-64.f, -16.f), colour),
        cro::Vertex2D(glm::vec2(64.f, 16.f), colour),
        cro::Vertex2D(glm::vec2(64.f, -16.f), colour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLE_STRIP);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto scale = e.getComponent<cro::Transform>().getScale();
        scale.y = std::min(1.f, scale.y + dt);
        e.getComponent<cro::Transform>().setScale(scale);

        if (scale.y == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    lineEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //second tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    if (cro::GameController::getControllerCount() != 0)
    {
        entity.addComponent<cro::Text>(font).setString("You can move the camera up and down with the right thumbstick.");
    }
    else
    {
        auto strUp = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]);
        auto strDown = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]);
        entity.addComponent<cro::Text>(font).setString("You can move the camera up and down with " + strUp + " and " + strDown);
    }
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -44.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text02 = entity;

    m_actionCallbacks.push_back([text02]() mutable { text02.getComponent<cro::Callback>().active = true; });


    //third tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("With Putting Assist enabled in the Options you can\nuse the flag on the power bar to judge the distance to the hole.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    //centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, /*UIBarHeight*5.5f*/-94.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(bounds.left, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text03 = entity;

    //and flag icon
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("putt_flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -90.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto flag = entity;

    m_actionCallbacks.push_back([text03, flag]() mutable 
        {
            text03.getComponent<cro::Callback>().active = true; 
            flag.getComponent<cro::Callback>().active = true; 
        });

    m_actionCallbacks.push_back([&]() { quitState(); });
}

void TutorialState::tutorialSwing(cro::Entity root)
{
    /*
    If you prefer you can use either thumbstick to swing your club

    Press and hold either trigger to show the Swingput display

    With the display active draw back on either thumbstick then
    press forward to take a swing

    */

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //first tip text
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("If you prefer you can use the thumbsticks to swing your club");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -40.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (!m_backgroundEnt.getComponent<cro::Callback>().active)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + (dt * 3.f));
            cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
            e.getComponent<cro::Drawable2D>().setCroppingArea(area);

            if (currTime == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                showContinue();
            }
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //swingput bar activated in second tip callback
    static constexpr float Width = 4.f;
    static constexpr float Height = 40.f;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {

            cro::Vertex2D(glm::vec2(Width,  Height), SwingputDark),
            cro::Vertex2D(glm::vec2(-Width, Height), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(-Width,  0.5f), SwingputDark),

            cro::Vertex2D(glm::vec2(Width,  0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(-Width,  0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(Width,  -0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(-Width,  -0.5f), TextNormalColour),

            cro::Vertex2D(glm::vec2(Width,  -0.5f), SwingputLight),
            cro::Vertex2D(glm::vec2(-Width, -0.5f), SwingputLight),
            cro::Vertex2D(glm::vec2(Width,  -Height), SwingputLight),
            cro::Vertex2D(glm::vec2(-Width, -Height), SwingputLight)
        });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        float targetAlpha = 0.25f;

        if (inputMask != 0)
        {
            targetAlpha = 1.f;
        }

        auto& currentAlpha = e.getComponent<cro::Callback>().getUserData<float>();
        const float InSpeed = dt * 6.f;

        if (currentAlpha <= targetAlpha)
        {
            currentAlpha = std::min(1.f, currentAlpha + InSpeed);
        }
        else
        {
            currentAlpha = std::max(0.251f, currentAlpha - InSpeed);
        }

        for (auto& v : verts)
        {
            v.colour.setAlpha(currentAlpha);
        }

        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    };

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { -10.f, 50.f + UITextPosV };

    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto swingput = entity;

    //second tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("Press and hold either     or     to show the Swingput display");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().absolutePosition = { -18.f, 66.f};
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds, swingput](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(bounds.left, bounds.bottom/* - (bounds.height * (1.f - currTime))*/, bounds.width * currTime, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();

            //activate swingput bar
            swingput.getComponent<cro::Callback>().active = true;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text02 = entity;

    //lt sprite
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -193.f, -12.f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::LT];
    entity.addComponent<cro::SpriteAnimation>().play(IS_PS(activeControllerID(0)) ? 1 : 0);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
    text02.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    auto lt = entity;

    //rt sprite
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -160.f, -12.f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::RT];
    entity.addComponent<cro::SpriteAnimation>().play(IS_PS(activeControllerID(0)) ? 1 : 0);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
    text02.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    auto rt = entity;

    m_actionCallbacks.push_back([text02, lt, rt]() mutable 
        {
            text02.getComponent<cro::Callback>().active = true; 
            lt.getComponent<cro::Callback>().active = true; 
            rt.getComponent<cro::Callback>().active = true;         
        });


    //third tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("With the display active\ndraw back on either one of the thumbsticks\nthen press forward to take a swing");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 86.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(bounds.left, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto text03 = entity;

    //and thumbstick icon
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/swingput.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("swingput");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 18.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto thumbstick = entity;

    m_actionCallbacks.push_back([text03, thumbstick]() mutable
        {
            text03.getComponent<cro::Callback>().active = true;
            thumbstick.getComponent<cro::Callback>().active = true;
        });

    m_actionCallbacks.push_back([&]() { quitState(); });
}

void TutorialState::tutorialSpin(cro::Entity root)
{
    /*
    Press and hold <BUTTON> to open the spin menu

    Use <BUTTONS> to adjust the spin direction


    Backspin and top spin affects wedges the most


    Side spin affects Irons and, to a lesser extent, Wood

    */

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_sharedData.sharedResources->textures);

    std::int32_t animID = 0;
    if (IS_PS(activeControllerID(0)))
    {
        animID = 1;
    }

    //first tip text
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font);

    if (cro::GameController::getControllerCount() != 0)
    {
        entity.getComponent<cro::Text>().setString("Hold    to show the Spin icon");
        centreText(entity);

        auto buttonEnt = m_scene.createEntity();
        buttonEnt.addComponent<cro::Transform>().setPosition({ 21.f, -12.f });
        buttonEnt.addComponent<cro::Drawable2D>();
        buttonEnt.addComponent<cro::Sprite>() = m_buttonSprites[ButtonID::X];
        buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        buttonEnt.addComponent<cro::SpriteAnimation>().play(animID);
        buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        buttonEnt.addComponent<cro::Callback>().active = true;
        buttonEnt.getComponent<cro::Callback>().setUserData<float>(0.f);
        buttonEnt.getComponent<cro::Callback>().function =
            [entity](cro::Entity e, float dt)
        {
            if (entity.getComponent<cro::Callback>().active)
            {
                auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
                currTime = std::min(1.f, currTime + (dt * 2.f));

                cro::Colour c(1.f, 1.f, 1.f, currTime);
                e.getComponent<cro::Sprite>().setColour(c);
            }
        };
        entity.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
    }
    else
    {
        cro::String str = "Hold " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::SpinMenu]) + " to show the Spin icon";
        entity.getComponent<cro::Text>().setString(str);
        centreText(entity);
    }

    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -40.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (!m_backgroundEnt.getComponent<cro::Callback>().active)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime = std::min(1.f, currTime + (dt * 3.f));
            cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
            e.getComponent<cro::Drawable2D>().setCroppingArea(area);

            if (currTime == 1)
            {
                e.getComponent<cro::Callback>().active = false;
                showContinue();
            }
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 32.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("spin_bg");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto spinput = entity;

    const auto fgOffset = glm::vec2(spinput.getComponent<cro::Transform>().getOrigin());
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(fgOffset);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("spin_fg");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, spinput, fgOffset](cro::Entity e, float dt) mutable
    {
        auto& scale = e.getComponent<cro::Callback>().getUserData<float>();
        const float speed = dt * 10.f;
        if (showSpin)
        {
            scale = std::min(1.f, scale + speed);

            auto size = spinput.getComponent<cro::Sprite>().getTextureBounds().width / 2.f;

            auto spin = spinAmount;
            if (auto len2 = glm::length2(spin); len2 != 0)
            {
                auto q = glm::rotate(cro::Transform::QUAT_IDENTITY, -spin.y, cro::Transform::X_AXIS);
                q = glm::rotate(q, spin.x, cro::Transform::Y_AXIS);
                auto r = q * cro::Transform::Z_AXIS;

                spin.x = -r.x;
                spin.y = r.y;
            }
            spin *= size;
            e.getComponent<cro::Transform>().setPosition(fgOffset + spin);
        }
        else
        {
            scale = std::max(0.1f, scale - speed);
        }
        auto s = scale;
        spinput.getComponent<cro::Transform>().setScale({ s,s });

        auto pos = spinput.getComponent<cro::Transform>().getPosition();
        pos.y = spinput.getComponent<UIElement>().absolutePosition.y + std::round(64.f * s);
        spinput.getComponent<cro::Transform>().setPosition(pos);

        cro::Colour c = cro::Colour::White;
        c.setAlpha(s);
        spinput.getComponent<cro::Sprite>().setColour(c);
    };
    spinput.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //second tip text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("Use the directional controls to adjust the spin");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -66.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(bounds.left, bounds.bottom, bounds.width * currTime, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    auto text02 = entity;


    //direction indicator
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), -22.f});
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });

    if (cro::GameController::getControllerCount() != 0)
    {
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("dpad");
        entity.addComponent<cro::SpriteAnimation>().play(IS_PS(activeControllerID(0)) ? 1 : 0);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::PlayerIcon;
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -0.01f });
    }
    else
    {
        std::string str = cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Left]);
        str += ", " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Right]);
        str += ", " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Up]);
        str += ", " + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::Down]);

        entity.addComponent<cro::Text>(font).setString(str);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        bounds = cro::Text::getLocalBounds(entity);
        centreText(entity);
    }
    text02.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(0.f, bounds.bottom - (bounds.height * (1.f - currTime)), bounds.width, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
        }
    };
    auto icon = entity;

    m_actionCallbacks.push_back([text02, icon]() mutable
        {
            text02.getComponent<cro::Callback>().active = true;
            icon.getComponent<cro::Callback>().active = true;
        });



    //third text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("Backspin and topspin affect wedges the most");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -106.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(bounds.left, bounds.bottom, bounds.width * currTime, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    auto text03 = entity;

    m_actionCallbacks.push_back([text03]() mutable
        {
            text03.getComponent<cro::Callback>().active = true;
        });


    //forth text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 0.f, 0.f });
    entity.addComponent<cro::Text>(font).setString("Sidespin will affect irons and, to a lesser extent, woods");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, -140.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 1.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + (dt * 3.f));
        cro::FloatRect area(bounds.left, bounds.bottom, bounds.width * currTime, bounds.height);
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        if (currTime == 1)
        {
            e.getComponent<cro::Callback>().active = false;
            showContinue();
        }
    };
    root.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    centreText(entity);
    auto text04 = entity;

    m_actionCallbacks.push_back([text04]() mutable
        {
            text04.getComponent<cro::Callback>().active = true;
        });



    m_actionCallbacks.push_back([&]() { quitState(); });
}

void TutorialState::showContinue()
{
    //pop this in an ent callback for time delay
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt) mutable
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime = std::min(1.f, currTime + dt);

        if(currTime == 1)
        {
            m_messageEnt.getComponent<cro::Text>().setFillColour(TextGoldColour);
            m_actionActive = true;

            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };
    
}

void TutorialState::doCurrentAction()
{
    if (m_actionActive)
    {
        //hide message
        m_messageEnt.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);

        m_actionActive = false;
        m_actionCallbacks[m_currentAction]();
        m_currentAction++;

        //play button sound
        playSound("accept");
    }
}

void TutorialState::playSound(const std::string& sound)
{
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::AudioEmitter>() = m_audioScape.getEmitter(sound);
    entity.getComponent<cro::AudioEmitter>().play();

    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (e.getComponent<cro::AudioEmitter>().getState() == cro::AudioEmitter::State::Stopped)
        {
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };
}

void TutorialState::quitState()
{
    //trigger background callback to pop state
    auto& data = m_backgroundEnt.getComponent<cro::Callback>().getUserData<BackgroundCallbackData>();
    data.progress = 0.f;
    data.state = BackgroundCallbackData::Out;
    data.targetSize = GolfGame::getActiveTarget()->getSize();
    data.targetSize /= m_viewScale;
    m_backgroundEnt.getComponent<cro::Callback>().active = true;
}