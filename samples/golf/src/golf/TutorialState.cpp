/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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
#include "CommandIDs.hpp"

#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>

#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/core/GameController.hpp>

namespace
{
    struct BackgroundCallbackData final
    {
        glm::vec2 targetSize = glm::vec2(0.f);
        float progress = 0.f;
        enum
        {
            In, Out
        }state = In;

        BackgroundCallbackData(glm::vec2 t) : targetSize(t) {}
    };
}

TutorialState::TutorialState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_sharedData    (sd),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_viewScale     (2.f),
    m_currentAction (0),
    m_actionActive  (false)
{
    ctx.mainWindow.setMouseCaptured(false);

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

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: 
            doCurrentAction();
            break;
        case SDLK_o:
            quitState();
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(0))
        {
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonA:
            case cro::GameController::ButtonB:
            case cro::GameController::ButtonStart:
                doCurrentAction();
                break;
            }
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void TutorialState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool TutorialState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void TutorialState::render()
{
    m_scene.render(cro::App::getWindow());
}

//private
void TutorialState::buildScene()
{
    //add systems
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);


    //root node is used to scale all attachments
    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();

    //load background
    cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    glm::vec2 size = cro::App::getWindow().getSize();

    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.1f });
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
        data.progress = std::min(1.f, data.progress + (dt* 2.f));

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
            position.x = data.targetSize.x * cro::Util::Easing::easeInCubic(data.progress);
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
    entity.addComponent<cro::Text>(font).setString("Press Any Button");
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(entity);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 32.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.f };
    entity.getComponent<UIElement>().depth = 0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_messageEnt = entity;

    //create the layout depending on the requested tutorial
    switch (m_sharedData.tutorialIndex)
    {
    default:
        quitState();
        break;
    case 0:
        tutorialOne(rootNode);
        break;
    case 1:
        tutorialTwo(rootNode);
        break;
    case 2:
        tutorialThree(rootNode);
        break;
    case 3:
        tutorialFour(rootNode);
        break;
    }


    //set up camera / resize callback
    auto camCallback = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size = cro::App::getWindow().getSize();
        cam.setOrthographic(0.f, size.x, 0.f, size.y, -0.5f, 5.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();
        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
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
        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
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
        entity.getComponent<cro::Text>().setString("This is your selected club.\nUse    and    to cycle through them\nand find one with an appropriate distance.");
        //TODO attach bumper graphics
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
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, arrow01](cro::Entity e, float dt) mutable
    {
        if (!m_backgroundEnt.getComponent<cro::Callback>().active)
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
        }
    };


    //first action shows the next set of messages
    m_actionCallbacks.push_back([arrow02]() mutable { arrow02.getComponent<cro::Callback>().active = true; });

    //second action quits
    m_actionCallbacks.push_back([&]() { quitState(); });
}

void TutorialState::tutorialTwo(cro::Entity root)
{
    /*
    The wind indicator shows which way the wind is blowing
    and how strong it currently is between 0 - 2 knots

    Use X and X to aim your shot accounting for the wind

    */
}

void TutorialState::tutorialThree(cro::Entity root)
{
    /*
    Press A to start  your swing and A again to choose the power

    Finally press A when the hook/slice indicator is in the centre
    */
}

void TutorialState::tutorialFour(cro::Entity root)
{
    /*
    Blue lines on the ground indicate the direction of the slope

    The long the lines and the bluer they are the strong the effect of the slope.
    */
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
            m_messageEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
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
    }
}

void TutorialState::quitState()
{
    //trigger background callback to pop state
    auto& data = m_backgroundEnt.getComponent<cro::Callback>().getUserData<BackgroundCallbackData>();
    data.progress = 0.f;
    data.state = BackgroundCallbackData::Out;
    data.targetSize = cro::App::getWindow().getSize();
    data.targetSize /= m_viewScale;
    m_backgroundEnt.getComponent<cro::Callback>().active = true;
}