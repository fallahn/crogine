/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

#include "PracticeState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "PacketIDs.hpp"
#include "Utility.hpp"
#include "TextAnimCallback.hpp"
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
}

PracticeState::PracticeState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool PracticeState::handleEvent(const cro::Event& evt)
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

void PracticeState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool PracticeState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void PracticeState::render()
{
    m_scene.render();
}

//private
void PracticeState::buildScene()
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
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/facilities_menu.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    rootNode.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto helpText = m_scene.createEntity();
    helpText.addComponent<cro::Transform>().setOrigin({0.f, 0.f, -0.2f});
    helpText.addComponent<cro::Drawable2D>();
    helpText.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info));
    helpText.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    helpText.getComponent<cro::Text>().setFillColour(TextNormalColour);
    menuEntity.getComponent<cro::Transform>().addChild(helpText.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });
    auto unselectedLockedID = uiSystem.addCallback(
        [helpText](cro::Entity) mutable
        {
            helpText.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
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

        e.addComponent<cro::Callback>().function = MenuTextCallback();

        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        return e;
    };


    glm::vec2 position(0.f, 28.f);
    static constexpr float ItemHeight = 10.f;
    //tutorial button
    entity = createItem(position, "Tutorial", menuEntity);
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) 
            {
                //TODO check stuff like current connection status (must be disconnected)
                //and whether or not the tutorial course data exists.
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_sharedData.hosting = true;
                    m_sharedData.gameMode = GameMode::Tutorial;
                    m_sharedData.localConnectionData.playerCount = 1;
                    m_sharedData.localConnectionData.playerData[0].isCPU = false;

                    //start a local server and connect
                    if (!m_sharedData.clientConnection.connected)
                    {
                        m_sharedData.serverInstance.launch(1, Server::GameMode::Golf, m_sharedData.fastCPU);

                        //small delay for server to get ready
                        cro::Clock clock;
                        while (clock.elapsed().asMilliseconds() < 500) {}

#ifdef USE_GNS
                        m_sharedData.clientConnection.connected = m_sharedData.serverInstance.addLocalConnection(m_sharedData.clientConnection.netClient);
#else
                        m_sharedData.clientConnection.connected = m_sharedData.clientConnection.netClient.connect("255.255.255.255", ConstVal::GamePort);
#endif

                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.stop();
                            m_sharedData.errorMessage = "Failed to connect to local server.\nPlease make sure port "
                                + std::to_string(ConstVal::GamePort)
                                + " is allowed through\nany firewalls or NAT";
                            requestStackPush(StateID::Error); //error makes sure to reset any connection
                        }
                        else
                        {
                            m_sharedData.serverInstance.setHostID(m_sharedData.clientConnection.netClient.getPeer().getID());
                            m_sharedData.mapDirectory = "tutorial";

                            //set the course to tutorial
                            auto data = serialiseString(m_sharedData.mapDirectory);
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

                            //now we wait for the server to send us the map name so we know the tutorial
                            //course has been set. Then the network event handler launches the game.
                        }
                    }
                }            
            });
    position.y -= ItemHeight;

    //driving range
    entity = createItem(position, "Driving Range", menuEntity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    requestStackClear();
                    requestStackPush(StateID::DrivingRange);
                }
            });
    position.y -= ItemHeight;


    entity = createItem(position, "Clubhouse", menuEntity);
    if (Achievements::getAchievement(AchievementStrings[AchievementID::JoinTheClub])->achieved)
    {
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        //also used as table index so reset this in case
                        //the current value is greater than the number of tables...
                        m_sharedData.courseIndex = 0;

                        requestStackClear();
                        requestStackPush(StateID::Clubhouse);

                        Achievements::awardAchievement(AchievementStrings[AchievementID::Socialiser]);
                    }
                });
    }
    else
    {
        //add padlock icon and help text (override selected/unselected event)
        entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedLockedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
            uiSystem.addCallback([helpText](cro::Entity e) mutable
            {
                helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                helpText.getComponent<cro::Text>().setString("Get Join The Club to Unlock");
                centreText(helpText);

                e.getComponent<cro::AudioEmitter>().play();
                e.getComponent<cro::Callback>().setUserData<float>(0.f);
                e.getComponent<cro::Callback>().active = true;
            });

        auto padlock = m_scene.createEntity();
        padlock.addComponent<cro::Transform>().setPosition(glm::vec3(PadlockOffset, position.y - ItemHeight + 2.f, 0.1f));
        padlock.addComponent<cro::Drawable2D>();
        padlock.addComponent<cro::Sprite>() = spriteSheet.getSprite("padlock");
        menuEntity.getComponent<cro::Transform>().addChild(padlock.getComponent<cro::Transform>());
    }
    position.y -= ItemHeight;

    //course editor
    entity = createItem(position, "Course Editor", menuEntity);
    if (Achievements::getAchievement(AchievementStrings[AchievementID::GrandTour])->achieved)
    {
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        requestStackClear();
                        requestStackPush(StateID::Playlist);
                    }
                });
    }
    else
    {
        //add padlock icon and tool tip
        entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedLockedID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
            uiSystem.addCallback([helpText](cro::Entity e) mutable
            {
                    helpText.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    helpText.getComponent<cro::Text>().setString("Get Grand Tour to Unlock");
                    centreText(helpText);

                    e.getComponent<cro::AudioEmitter>().play();
                    e.getComponent<cro::Callback>().setUserData<float>(0.f);
                    e.getComponent<cro::Callback>().active = true;
            });


        auto padlock = m_scene.createEntity();
        padlock.addComponent<cro::Transform>().setPosition(glm::vec3(PadlockOffset, position.y - ItemHeight + 2.f, 0.1f));
        padlock.addComponent<cro::Drawable2D>();
        padlock.addComponent<cro::Sprite>() = spriteSheet.getSprite("padlock");
        menuEntity.getComponent<cro::Transform>().addChild(padlock.getComponent<cro::Transform>());
    }
    position.y -= ItemHeight;

    position.y -= 3.f;
    helpText.getComponent<cro::Transform>().setPosition(position);


    //back button
    entity = createItem(glm::vec2(0.f, -28.f), "Back", menuEntity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    quitState();
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

void PracticeState::quitState()
{
    m_scene.setSystemActive<cro::UISystem>(false);

    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}