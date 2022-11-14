/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include "CreditsState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "../GolfGame.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
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
            Main, Confirm
        };
    };
}

CreditsState::CreditsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd, const std::vector<CreditEntry>& credits)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_credits   (credits),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool CreditsState::handleEvent(const cro::Event& evt)
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
            || evt.key.keysym.sym == SDLK_ESCAPE
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.button == cro::GameController::ButtonB
            || evt.cbutton.button == cro::GameController::ButtonStart)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void CreditsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool CreditsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void CreditsState::render()
{
    m_scene.render();
}

//private
void CreditsState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
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
        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
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

   
    glm::vec2 size(GolfGame::getActiveTarget()->getSize());
    auto vpSize = calcVPSize();
    auto scale = std::floor(size.y / vpSize.y);
    auto scrollOffset = ((size.y / scale) / 2.f) + (UITextPosV * 2.f);

    auto scrollNode = m_scene.createEntity();
    scrollNode.addComponent<cro::Transform>().setPosition({ 0.f, -scrollOffset, 0.1f });
    scrollNode.addComponent<cro::Callback>().active = true;
    scrollNode.getComponent<cro::Callback>().function =
        [&, scrollOffset, rootNode](cro::Entity e, float dt)
    {
        const float endPos = e.getComponent<cro::Callback>().getUserData<const float>();
        constexpr float Speed = 20.f;
        auto pos = e.getComponent<cro::Transform>().getPosition();
        pos.y += Speed * dt;
        e.getComponent<cro::Transform>().setPosition(pos);
        e.getComponent<cro::Transform>().setScale(rootNode.getComponent<cro::Transform>().getScale() / glm::vec3(m_viewScale, 1.f));

        if (pos.y > endPos + scrollOffset)
        {
            quitState();
        }
    };
    rootNode.getComponent<cro::Transform>().addChild(scrollNode.getComponent<cro::Transform>());


    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    glm::vec2 position(0.f);
    auto createEntry = [&](const CreditEntry& entry)
    {
        cro::FloatRect bounds;
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.2f));

        auto textEnt = m_scene.createEntity();
        textEnt.addComponent<cro::Transform>().setPosition({0.f, UITextPosV });
        textEnt.addComponent<cro::Drawable2D>();
        textEnt.addComponent<cro::Text>(largeFont).setString(entry.title);
        textEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
        textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        e.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
        bounds = cro::Text::getLocalBounds(textEnt);

        cro::String str;
        for (const auto& name : entry.names)
        {
            str += name + "\n";
        }
        textEnt = m_scene.createEntity();
        textEnt.addComponent<cro::Transform>();
        textEnt.addComponent<cro::Drawable2D>();
        textEnt.addComponent<cro::Text>(smallFont).setString(str);
        textEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        e.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
        bounds.height += cro::Text::getLocalBounds(textEnt).height;

        scrollNode.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        return bounds;
    };
    float width = 0.f;
    for (const auto& entry : m_credits)
    {
        auto bounds = createEntry(entry);
        position.y -= bounds.height;
        position.y -= UITextPosV * 2.f;

        if (bounds.width > width)
        {
            width = bounds.width;
        }
    }
    scrollNode.getComponent<cro::Callback>().setUserData<const float>(-position.y);
    scrollNode.getComponent<cro::Transform>().setOrigin({ width / 2.f, 0.f });

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

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
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void CreditsState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
}