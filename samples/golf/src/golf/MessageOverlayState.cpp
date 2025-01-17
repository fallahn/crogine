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

#include "MessageOverlayState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "PacketIDs.hpp"
#include "Utility.hpp"
#include "TextAnimCallback.hpp"
#include "Career.hpp"
#include "Tournament.hpp"
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

#include <filesystem>

namespace
{

}

MessageOverlayState::MessageOverlayState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool MessageOverlayState::handleEvent(const cro::Event& evt)
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

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void MessageOverlayState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool MessageOverlayState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void MessageOverlayState::render()
{
    m_scene.render();
}

//private
void MessageOverlayState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);// ->setActiveControllerID(m_sharedData.inputBinding.controllerID);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

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
    //message text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(position);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.errorMessage);
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(entity);
    menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    position.y -= ItemHeight;


    //if this is entitled 'welcome' we'll assume it's a tutorial tip
    if (m_sharedData.errorMessage == "Welcome")
    {
        entity.getComponent<cro::Transform>().move({ 0.f, 4.f });

        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position + glm::vec2(-82.f, 2.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("Would you like to do the tutorial?\nThe tutorial can be launched\nfrom the 19th hole at any time.");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //buttons
        entity = createItem(glm::vec2(-28.f, -16.f), "Yes", menuEntity);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
                {
                    if (activated(evt))
                    {
                        //launch tutorial
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                        m_sharedData.hosting = true;
                        m_sharedData.gameMode = GameMode::Tutorial;
                        m_sharedData.localConnectionData.playerCount = 1;
                        m_sharedData.localConnectionData.playerData[0].isCPU = false;
                        m_sharedData.clubSet = 0;
                        m_sharedData.preferredClubSet = 0;
                        m_sharedData.leagueRoundID = LeagueRoundID::Club;

                        //start a local server and connect
                        if (quickConnect(m_sharedData))
                        {
                            m_sharedData.mapDirectory = "tutorial";

                            //set the course to tutorial
                            auto data = serialiseString(m_sharedData.mapDirectory);
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

                            //now we wait for the server to send us the map name so we know the tutorial
                            //course has been set. Then the network event handler launches the game.

                            //reset the tutorial flag in shared data
                            m_sharedData.showTutorialTip = false;
                        }
                        else
                        {
                            //quick connect sets the error for us
                            requestStackPush(StateID::Error); //error makes sure to reset any connection
                        }
                    }
                });
        entity = createItem(glm::vec2(28.f, -16.f), "No", menuEntity);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        quitState();
                    }
                });

        //checkbox
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(-70.f, -32.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("Show This Next Time");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        //checkbox centre
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(-82.f, -39.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox");
        bounds = entity.getComponent<cro::Sprite>().getTextureRect();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, bounds](cro::Entity e, float)
        {
            auto b = bounds;
            if (m_sharedData.showTutorialTip)
            {
                b.bottom -= bounds.height;
            }
            e.getComponent<cro::Sprite>().setTextureRect(b);
        };
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //checkbox highlight
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(-83.f, -40.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
            uiSystem.addCallback(
                [](cro::Entity e)
                {
                    e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
            uiSystem.addCallback(
                [](cro::Entity e)
                {
                    e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback(
                [&](cro::Entity e, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        m_sharedData.showTutorialTip = !m_sharedData.showTutorialTip;
                    }
                });

        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    else if (m_sharedData.errorMessage == "Welcome to the Roster")
    {
        entity.getComponent<cro::Transform>().move({ 0.f, 4.f });

        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position + glm::vec2(0.f, 2.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("To add an opponent click\nAdd Player or Create Profile");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        entity = createItem(glm::vec2(0.f, -16.f), "Got It!", menuEntity);
        //entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        quitState();
                    }
                });

        //checkbox
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(-70.f, -32.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("Show This Next Time");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        //checkbox centre
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(-82.f, -39.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox");
        bounds = entity.getComponent<cro::Sprite>().getTextureRect();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, bounds](cro::Entity e, float)
            {
                auto b = bounds;
                if (m_sharedData.showRosterTip)
                {
                    b.bottom -= bounds.height;
                }
                e.getComponent<cro::Sprite>().setTextureRect(b);
            };
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //checkbox highlight
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec2(-83.f, -40.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("checkbox_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
            uiSystem.addCallback(
                [](cro::Entity e)
                {
                    e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
            uiSystem.addCallback(
                [](cro::Entity e)
                {
                    e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback(
                [&](cro::Entity e, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        m_sharedData.showRosterTip = !m_sharedData.showRosterTip;
                    }
                });

        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    else if (m_sharedData.errorMessage == "reset_profile")
    {
        entity.getComponent<cro::Text>().setString("Are You REALLY Sure?");
        centreText(entity);

        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position + glm::vec2(0.f, -4.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("This will reset all of your\nprogress including all of\nyour XP and quit the game.");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



        //buttons
        entity = createItem(glm::vec2(28.f, -26.f), "No", menuEntity);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        quitState();
                    }
                });

        entity = createItem(glm::vec2(-28.f, -26.f), "Yes", menuEntity);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        //this is a kludge which tells the
                        //menu state to remove any existing connection/server instance
                        //if for some reason we're resetting mid-game
                        //m_sharedData.gameMode = GameMode::Tutorial;

                        Social::resetProfile();
                        Career::instance(m_sharedData).reset();

                        League l(LeagueRoundID::Club, m_sharedData);
                        l.reset();
                        
                        Tournament t;
                        t.id = 0;
                        resetTournament(t);
                        writeTournamentData(t);
                        t.id = 1;
                        resetTournament(t);
                        writeTournamentData(t);

                        readTournamentData(m_sharedData.tournaments[0]);
                        readTournamentData(m_sharedData.tournaments[1]);

                        cro::App::quit();

                        /*requestStackClear();
                        requestStackPush(StateID::SplashScreen);*/
                    }
                });

    }
    else if (m_sharedData.errorMessage == "reset_career")
    {
        entity.getComponent<cro::Text>().setString("Are You REALLY Sure?");
        centreText(entity);

        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position + glm::vec2(0.f, -4.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("This will reset all of your\ncareer progress, preserving\nany unlocked items.");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



        //buttons
        entity = createItem(glm::vec2(28.f, -26.f), "No", menuEntity);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        quitState();
                    }
                });

        entity = createItem(glm::vec2(-28.f, -26.f), "Yes", menuEntity);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        //this is a kludge which tells the
                        //menu state to remove any existing connection/server instance
                        //if for some reason we're resetting mid-game
                        m_sharedData.gameMode = GameMode::Tutorial;
                        m_sharedData.leagueTable = 0; //must reset this else league browser tries to open non-existent table
                        m_sharedData.leagueRoundID = 0;

                        Career::instance(m_sharedData).reset();

                        Tournament t;
                        t.id = 0;
                        resetTournament(t);
                        writeTournamentData(t);
                        t.id = 1;
                        resetTournament(t);
                        writeTournamentData(t);

                        /*Social::setUnlockStatus(Social::UnlockType::CareerAvatar, 0);
                        Social::setUnlockStatus(Social::UnlockType::CareerBalls, 0);
                        Social::setUnlockStatus(Social::UnlockType::CareerHair, 0);
                        Social::setUnlockStatus(Social::UnlockType::CareerPosition, 0);*/

                        requestStackClear();
                        requestStackPush(StateID::SplashScreen);
                    }
                });

    }
    else if (m_sharedData.errorMessage == "delete_profile")
    {
        entity.getComponent<cro::Text>().setString("Are You REALLY Sure?");
        centreText(entity);

        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({50.f, 10.f, 0.1f});
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("This will remove this\nprofile permanently.");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);
        centreText(entity);
        menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



        //buttons
        entity = createItem(glm::vec2(28.f, -26.f), "No", menuEntity);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        m_sharedData.errorMessage = "";
                        quitState();
                    }
                });

        entity = createItem(glm::vec2(-28.f, -26.f), "Yes", menuEntity);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
                {
                    if (activated(evt))
                    {
                        m_sharedData.errorMessage = "delete_profile"; //used when returning to menu to decide on appropriate action
                        quitState();
                    }
                });
    }
    else //a generic message
    {
        //back button
        entity = createItem(glm::vec2(0.f, -28.f), "OK", menuEntity);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
                {
                    if (activated(evt))
                    {
                        quitState();
                    }
                });
    }


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

void MessageOverlayState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}