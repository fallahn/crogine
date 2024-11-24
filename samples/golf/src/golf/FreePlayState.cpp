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

#include "FreePlayState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "Utility.hpp"
#include "TextAnimCallback.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"

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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    constexpr float PadlockOffset = 58.f;

    struct MenuID final
    {
        enum
        {
            GameMode, Quickplay,

            Count
        };
    };
}

FreePlayState::FreePlayState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool FreePlayState::handleEvent(const cro::Event& evt)
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

void FreePlayState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool FreePlayState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void FreePlayState::render()
{
    m_scene.render();
}

//private
void FreePlayState::buildScene()
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
    //cro::SpriteSheet spriteSheet;
    //spriteSheet.loadFromFile("assets/golf/sprites/facilities_menu.spt", m_sharedData.sharedResources->textures);

    const auto& tex = m_sharedData.sharedResources->textures.get("assets/golf/images/freeplay_menu.png");

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(tex);// = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    rootNode.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());


    //decription text
    float descPos = -36.f;
    cro::String desc = 
R"(Play on any course, in 10 different game modes including: 
Stroke Play, Match Play, Skins, Stableford, Elimination, Multi-Target,
Nearest the Pin and more.)";

#ifdef USE_GNS
    descPos = -24.f;
    desc += R"(

And...
When playing Stroke Play you'll automatically compete in the Global
League as well as the online global leaderboards for the Monthly
and All Time best scores.)";
#endif

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto descText = m_scene.createEntity();
    descText.addComponent<cro::Transform>().setPosition({ 0.f, descPos });
    descText.addComponent<cro::Drawable2D>();
    descText.addComponent<cro::Text>(smallFont);
    descText.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    descText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    descText.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    descText.getComponent<cro::Text>().setVerticalSpacing(-2.f);
    descText.getComponent<cro::Text>().setString(desc);
    menuEntity.getComponent<cro::Transform>().addChild(descText.getComponent<cro::Transform>());



    auto helpText = m_scene.createEntity();
    helpText.addComponent<cro::Transform>().setOrigin({0.f, 0.f, -0.2f});
    helpText.addComponent<cro::Drawable2D>();
    helpText.addComponent<cro::Text>(smallFont);
    helpText.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    helpText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    helpText.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    helpText.getComponent<cro::Text>().setString("Host a new game to play solo,\nagainst CPU or with friends");
    menuEntity.getComponent<cro::Transform>().addChild(helpText.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [helpText](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            switch (e.getComponent<cro::UIInput>().getSelectionIndex())
            {
            default:
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                break;
            case 1:
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                helpText.getComponent<cro::Text>().setString("Host a new game to play solo,\nagainst CPU or with friends");
                break;
            case 2:
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                helpText.getComponent<cro::Text>().setString("Join an existing network game");
                break;
            case 3:
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                helpText.getComponent<cro::Text>().setString("Start a round of 9 random\nholes against virtual opponents");
                break;
            case 4:
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                helpText.getComponent<cro::Text>().setString("Return to Main Menu");
                break;
            case 10:
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                helpText.getComponent<cro::Text>().setString("Set the opponent's difficulty");
                break;
            }
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    
    auto createItem = [&](glm::vec2 position, const std::string& label, cro::Entity parent) 
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(position);
        e.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        e.getComponent<cro::Text>().setString(label);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        centreText(e);
        e.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(e);
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
        e.getComponent<cro::UIInput>().setGroup(MenuID::GameMode);

        e.addComponent<cro::Callback>().function = MenuTextCallback();

        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        return e;
    };

    auto gameMenuEnt = m_scene.createEntity();
    gameMenuEnt.addComponent<cro::Transform>();
    menuEntity.getComponent<cro::Transform>().addChild(gameMenuEnt.getComponent<cro::Transform>());

    auto qpMenuEnt = m_scene.createEntity();
    qpMenuEnt.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.getComponent<cro::Transform>().addChild(qpMenuEnt.getComponent<cro::Transform>());

    //--------game menu------//
    static constexpr float MenuStartHeight = 77.f;
    glm::vec2 position(0.f, MenuStartHeight);
    static constexpr float ItemHeight = 10.f;
    //create game
    entity = createItem(position, "Create Game", gameMenuEnt);
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) 
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_sharedData.hosting = true;
                    m_sharedData.clubSet = m_sharedData.preferredClubSet;
                    m_sharedData.holeCount = 0;

                    //hack so this doesn't happen when quitting from a previous game
                    static bool wasDone = false;
                    if (!wasDone)
                    {
                        m_sharedData.courseIndex = courseOfTheMonth();
                        wasDone = true;
                    }

                    auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::MenuRequest;
                    msg->data = StateID::FreePlay;

                    quitState();
                }            
            });
    position.y -= ItemHeight;

    //join game
    entity = createItem(position, "Join Game", gameMenuEnt);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_sharedData.hosting = false;
                    m_sharedData.clubSet = m_sharedData.preferredClubSet;

                    auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::MenuRequest;
                    msg->data = StateID::FreePlay;

                    quitState();
                }
            });
    position.y -= ItemHeight;

    //quick game
    entity = createItem(position, "Quick Game", gameMenuEnt);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, gameMenuEnt, qpMenuEnt]
        (cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    //switch menu layout
                    gameMenuEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    qpMenuEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Quickplay);
                }
            });


    position.y -= ItemHeight;
    position.y -= 6.f;
    helpText.getComponent<cro::Transform>().setPosition(position);


    //back button
    entity = createItem(glm::vec2(0.f, 8.f), "Back", gameMenuEnt);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });



    //-------quick play menu------//
    position = { 0.f, MenuStartHeight };

    static const std::array<std::string, 3u> ClubsetStrings =
    {
        std::string("Novice"), "Expert", "Pro"
    };

    //club selection
    entity = createItem(position, "Club Set: " + ClubsetStrings[m_sharedData.clubSet], qpMenuEnt);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Quickplay);
    entity.getComponent<cro::UIInput>().setSelectionIndex(10);

    if (Social::getClubLevel())
    {
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                        m_sharedData.preferredClubSet = (m_sharedData.preferredClubSet + 1) % (Social::getClubLevel() + 1);
                        m_sharedData.clubSet = m_sharedData.preferredClubSet;
                        Club::setClubLevel(m_sharedData.clubSet);

                        e.getComponent<cro::Text>().setString("Club Set: " + ClubsetStrings[m_sharedData.clubSet]);
                        centreText(e);
                    }
                });
    }
    position.y -= ItemHeight * 2.f;

    //start
    entity = createItem(position, "Start Game", qpMenuEnt);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Quickplay);
    entity.getComponent<cro::UIInput>().setSelectionIndex(11);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::MenuRequest;
                    msg->data = RequestID::QuickPlay;
                }
            });


    //back button
    entity = createItem(glm::vec2(0.f, 8.f), "Back", qpMenuEnt);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Quickplay);
    entity.getComponent<cro::UIInput>().setSelectionIndex(12);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, gameMenuEnt, qpMenuEnt](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    //switch menu layout
                    gameMenuEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    qpMenuEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::GameMode);
                }
            });



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

void FreePlayState::quitState()
{
    m_scene.setSystemActive<cro::UISystem>(false);

    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}