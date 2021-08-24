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

#include "OptionsState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"

#include <crogine/gui/Gui.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Video, Controls, Dummy
        };
    };
}

OptionsState::OptionsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool OptionsState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (evt.type == SDL_KEYUP)
    {
        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.button == SDL_CONTROLLER_BUTTON_B)
        {
            quitState();
            return false;
        }
    }

    m_scene.getSystem<cro::UISystem>().handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void OptionsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool OptionsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void OptionsState::render()
{
    m_scene.render(cro::App::getWindow());
}

//private
void OptionsState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);

    struct RootCallbackData final
    {
        enum
        {
            FadeIn, FadeOut
        }state = FadeIn;
        float currTime = 0.f;
    };

    auto rootNode = m_scene.createEntity();
    rootNode.addComponent<cro::Transform>();
    rootNode.addComponent<cro::Callback>().active = true;
    rootNode.getComponent<cro::Callback>().setUserData<RootCallbackData>();
    rootNode.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& [state, currTime] = e.getComponent<cro::Callback>().getUserData<RootCallbackData>();

        switch (state)
        {
        default: break;
        case RootCallbackData::FadeIn:
            currTime = std::min(1.f, currTime + (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, rootNode](cro::Entity e, float)
    {
        auto size = glm::vec2(cro::App::getWindow().getSize());
        e.getComponent<cro::Transform>().setScale(size);
        e.getComponent<cro::Transform>().setPosition(size / 2.f);

        auto scale = rootNode.getComponent<cro::Transform>().getScale().x;
        scale = std::min(1.f, scale / m_viewScale.x);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options.spt", m_sharedData.sharedResources->textures);

    //options background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;
    auto bgSize = glm::vec2(bounds.width, bounds.height);

    auto& uiSystem = m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Text>().setFillColour(TextHighlightColour); });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Text>().setFillColour(TextNormalColour); });

    //video options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, 20.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("audio_video");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto videoEnt = entity;

    //control options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, 20.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("input");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto controlEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Controls, selectedID, unselectedID);
    auto controlButtonEnt = entity;

    //tab bar header
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 0.f, bgSize.y - bounds.height, 0.1f });
    entity.addComponent<cro::Callback>();// .active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(1.f, 1);
    entity.getComponent<cro::Callback>().function =
        [videoEnt, controlEnt](cro::Entity e, float dt) mutable
    {
        auto& [currPos, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (direction == 0)
        {
            //grow
            currPos = std::min(1.f, currPos + dt);
            if (currPos == 1)
            {
                direction = 1;
                //TODO set active menu
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            //shrink
            currPos = std::max(0.f, currPos - dt);
            if (currPos == 0)
            {
                direction = 0;
                //TODO set active menu
                e.getComponent<cro::Callback>().active = false;
            }
        }

        auto progress = cro::Util::Easing::easeInOutQuint(currPos);

        auto area = e.getComponent<cro::Sprite>().getTextureBounds();
        area.width *= progress;
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        controlEnt.getComponent<cro::Transform>().setScale({ progress, 1.f });

        videoEnt.getComponent<cro::Transform>().setScale({ 1.f - progress, 1.f });
        auto bounds = videoEnt.getComponent<cro::Sprite>().getTextureBounds();
        auto pos = videoEnt.getComponent<cro::Transform>().getPosition();
        pos.x = (bounds.width * progress) + 4.f; //need to tie the 4 in with the position above
        videoEnt.getComponent<cro::Transform>().setPosition(pos);
    };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto tabEnt = entity;

    selectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::White); });
    unselectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });

    //tab button to switch to video
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 130.f, 0.2f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [tabEnt, controlButtonEnt](cro::Entity e, cro::ButtonEvent evt)  mutable
    {
            tabEnt.getComponent<cro::Callback>().active = true;
            //TODO set control ent off screen somewhere
            //TODO reset position for video button ent
            //TODO set the active group to a dummy until animation is complete
    });
    controlButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    buildAVMenu(videoEnt, spriteSheet);
    buildControlMenu(controlEnt, spriteSheet);


    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(cro::App::getWindow().getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>().sendCommand(cmd);
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());


    //we need to delay setting the active group until all the
    //new entities are processed, so we kludge this with a callback
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        m_scene.getSystem<cro::UISystem>().setActiveGroup(MenuID::Controls);
        e.getComponent<cro::Callback>().active = false;
        m_scene.destroyEntity(e);
    };
}

void OptionsState::buildAVMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{

}

void OptionsState::buildControlMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();

    auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto infoEnt = m_scene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition(glm::vec2(parentBounds.width / 2.f, parentBounds.height - 4.f));
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(infoFont).setString("Info Text");
    infoEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    infoEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);

    parent.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());

    auto& uiSystem = m_scene.getSystem<cro::UISystem>();
    auto unselectID = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            infoEnt.getComponent<cro::Text>().setString(" ");
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    //prev club
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(58.f, 80.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            infoEnt.getComponent<cro::Text>().setString("Previous Club");
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt) 
        {
            if (activated(evt))
            {

            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //next club
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(124.f, 80.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            infoEnt.getComponent<cro::Text>().setString("Next Club");
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {

            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //aim left
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(30.f, 42.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            infoEnt.getComponent<cro::Text>().setString("Aim Left");
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {

            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //aim right
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(30.f, 27.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            infoEnt.getComponent<cro::Text>().setString("Aim Right");
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {

            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //swing
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(152.f, 41.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            infoEnt.getComponent<cro::Text>().setString("Take Shot");
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {

            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void OptionsState::createButtons(cro::Entity parent, std::int32_t menuID, std::uint32_t selectedID, std::uint32_t unselectedID)
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& uiSystem = m_scene.getSystem<cro::UISystem>();

    //advanced
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(8.f, 14.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Advanced");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    auto bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                cro::Console::show();
            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //apply
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(103.f, 14.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Apply");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {

            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //close
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(154.f, 14.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Close");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                quitState();
            }
        });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void OptionsState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
}