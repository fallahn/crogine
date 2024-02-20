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

#include "CareerState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "Utility.hpp"
#include "TextAnimCallback.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float PadlockOffset = 58.f;
}

CareerState::CareerState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool CareerState::handleEvent(const cro::Event& evt)
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
    else if (evt.type == SDL_KEYDOWN)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
            cro::App::getWindow().setMouseCaptured(true);
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        cro::App::getWindow().setMouseCaptured(true);
        if (evt.cbutton.button == cro::GameController::ButtonB)
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
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (evt.caxis.value > LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void CareerState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && getStateCount() == 2) //hmm we really want to be able to check the top state is this one
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        }
    }

    m_scene.forwardMessage(msg);
}

bool CareerState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void CareerState::render()
{
    m_scene.render();
}

//private
void CareerState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::UISystem>(false);

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

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

                m_scene.setSystemActive<cro::UISystem>(true);
                Social::setStatus(Social::InfoID::Menu, { "Making Career Decisions" });

                if (!m_sharedData.unlockedItems.empty())
                {
                    requestStackPush(StateID::Unlock);
                }
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();            

                state = RootCallbackData::FadeIn;

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

   
    //background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-253.f, 129.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(-253.f, -129.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(253.f, 129.f), CD32::Colours[CD32::Brown]),

            cro::Vertex2D(glm::vec2(253.f, 129.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(-253.f, -129.f), CD32::Colours[CD32::Brown]),
            cro::Vertex2D(glm::vec2(253.f, -129.f), CD32::Colours[CD32::Brown]),

            cro::Vertex2D(glm::vec2(1.f, 128.f), CD32::Colours[CD32::GreyMid]),
            cro::Vertex2D(glm::vec2(1.f, -128.f), CD32::Colours[CD32::GreyMid]),
            cro::Vertex2D(glm::vec2(252.f, 128.f), CD32::Colours[CD32::GreyMid]),

            cro::Vertex2D(glm::vec2(252.f, 128.f), CD32::Colours[CD32::GreyMid]),
            cro::Vertex2D(glm::vec2(1.f, -128.f), CD32::Colours[CD32::GreyMid]),
            cro::Vertex2D(glm::vec2(252.f, -128.f), CD32::Colours[CD32::GreyMid]),
        });
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    //league items
    for (auto i = 0u; i < Career::MaxLeagues; ++i)
    {
        //league titles, listed on left
        //if unlocked white text with UI input to start playing
        //else red text, probably padlock icon?

        //activating updates preview
    }
    
    //right box
    //info entities - round x/6, course title, hole count, current league position, best finishing position, lock status(?)
    //probably course thumbnail
    //maybe weather selection/gimme selection
    //probably want to be able to reset the career too

    //entity with confirmation for starting round




    //banner across bottom

    //banner buttons - quit, options, profile editor, club set, league browser, start




    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
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

void CareerState::quitState()
{
    Social::setStatus(Social::InfoID::Menu, { "Main Menu" });

    m_scene.setSystemActive<cro::UISystem>(false);

    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}