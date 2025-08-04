/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "TournamentState.hpp"
#include "SharedStateData.hpp"
#include "SharedCourseData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "Utility.hpp"
#include "CallbackData.hpp"
#include "MenuCallbacks.hpp"
#include "TextAnimCallback.hpp"
#include "PacketIDs.hpp"
#include "Clubs.hpp"
#include "../GolfGame.hpp"
#include "../Colordome-32.hpp"
#include "../WebsocketServer.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Input.hpp>

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
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct ConfirmationData final
    {
        float progress = 0.f;
        enum
        {
            In, Out
        }dir = In;
    };

    struct MenuID final
    {
        enum
        {
            Dummy,
            Career, ConfirmQuit,
            Info, Stat
        };
    };

    enum
    {
        CareerOptions = 10,
        
        CareerGimme,
        CareerWeather,
        CareerClubs,
        CareerNight,
        CareerClubStats,
        CareerQuit,
        CareerProfile,
        CareerReset,
        CareerStart,
        CareerStats,
        CareerInfo,

        CareerTournPrev,
        CareerTournNext,
        CareerScrollPrev,
        CareerScrollNext,

        //CareerSeason = 100
    };

    const std::string ConfigFile("career.cfg");

    std::int32_t tournamentID = 0;
    const std::array<std::string, 2u> TournamentNames = { std::string("Dagle-Bunnage Cup"), "Sammonfield Championship" };

    constexpr glm::uvec2 TreeTexSize(1280, 110);
    constexpr glm::vec2 TreeTexSizeF(TreeTexSize);

    constexpr float NameSpacing = 176.f;
    constexpr float Padding = 4.f;
    constexpr glm::vec2 Tier0Position(Padding + NameSpacing, TreeTexSizeF.y - 16.f);
    constexpr float Tier0Spacing = 12.f; //vertical spacing
    constexpr float Tier0Stride = TreeTexSizeF.x - (NameSpacing * 2.f) - (Padding * 2.f); //horizontal spacing for right bracket

    constexpr glm::vec2 Tier1Position = Tier0Position + glm::vec2(NameSpacing + Padding, -Tier0Spacing * 0.5f);
    constexpr float Tier1Spacing = Tier0Spacing * 2.f;
    constexpr float Tier1Stride = Tier0Stride - (NameSpacing * 2.f) - (Padding * 2.f);

    constexpr glm::vec2 Tier2Position = Tier1Position + glm::vec2(NameSpacing + Padding, -Tier1Spacing * 0.5f);
    constexpr float Tier2Spacing = Tier1Spacing * 2.f;
    constexpr float Tier2Stride = Tier1Stride - (NameSpacing * 2.f) - (Padding * 2.f);

    constexpr cro::FloatRect TreeCroppingArea({ 0.f, 0.f, 490.f, 110.f });

    constexpr std::array ScrollPositions =
    {
        Tier0Position.x - (Padding + NameSpacing),

        (TreeTexSizeF.x / 2.f) - (TreeCroppingArea.width / 2.f),

        TreeTexSizeF.x - TreeCroppingArea.width
    };

    struct ScrollID final
    {
        enum
        {
            Left, Centre, Right,

            Reset //plays an animation
        };
    };

    struct ScrollCallbackData final
    {
        std::int32_t scrollID = 0;
        float progress = 0.f;

        static constexpr float DefaultVelocity = 20000.f;
        float velocity = DefaultVelocity;
    };
}

TournamentState::TournamentState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_sharedData    (sd),
    m_viewScale     (2.f),
    m_axisPosition  (0.f),
    m_currentMenu   (MenuID::Career)
{
    ctx.mainWindow.setMouseCaptured(false);

    loadAssets();
    addSystems();
    buildScene();
}

TournamentState::~TournamentState()
{
    //this might be quitting from a cached state and not
    //necessarily starting a career game.
    saveConfig();
}

//public
bool TournamentState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    const auto scrollTree = [&](bool left)
        {
            if (m_scene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Career)
            {
                auto idx = m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID;
                if (left)
                {
                    idx = (idx + (ScrollPositions.size() - 1)) % ScrollPositions.size();
                }
                else
                {
                    idx = (idx + 1) % ScrollPositions.size();
                }
                m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = idx;

                m_audioEnts[AudioID::Switch].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
                m_audioEnts[AudioID::Switch].getComponent<cro::AudioEmitter>().play();
            }
        };


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
        /*case SDLK_l:
            m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = ScrollID::Reset;
            break;*/
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
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
            quitState();
            return false;
            break;
        case cro::GameController::ButtonLeftShoulder:
        case cro::GameController::ButtonRightShoulder:
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            tournamentID = (tournamentID + 1) % 2;
            refreshTree();
            break;
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
        if (evt.caxis.value > cro::GameController::LeftThumbDeadZone)
        {
            cro::App::getWindow().setMouseCaptured(true);
        }

        if (evt.caxis.axis == cro::GameController::AxisRightX)
        {
            const auto DeadZone = cro::GameController::LeftThumbDeadZone * 2;

            if (evt.caxis.value > DeadZone
                && m_axisPosition < DeadZone)
            {
                scrollTree(false);
            }
            else if (evt.caxis.value < -DeadZone
                && m_axisPosition > -DeadZone)
            {
                scrollTree(true);
            }

            m_axisPosition = evt.caxis.value;
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        scrollTree(evt.wheel.y > 0);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void TournamentState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cro::Message::StateMessage)
    {
        const auto& data = msg.getData<cro::Message::StateEvent>();
        if (data.action == cro::Message::StateEvent::Popped
            && getStateCount() == 2) //hmm we really want to be able to check the top state is this one
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;

            if (data.id == StateID::Unlock
                && m_sharedData.showCredits)
            {
                m_sharedData.showCredits = false;
                requestStackPush(StateID::Credits);
            }
            else if (data.id == StateID::Profile)
            {
                refreshTree(); // may have changed profile name
            }
        }
    }
    else if (msg.id == cro::Message::WindowMessage)
    {
        const auto& data = msg.getData<cro::Message::WindowEvent>();
        if (data.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            //if we have a window over the top (eg profile editor)
            //we want to activate this on window resize so layout
            //is correctly updated.
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        }
    }

    m_scene.forwardMessage(msg);
}

bool TournamentState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void TournamentState::render()
{
    m_scene.render();
}

//private
void TournamentState::loadAssets()
{
    //these are used to create a texture containing the current tree
    m_treeTexture.create(TreeTexSize.x, TreeTexSize.y, false);

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    m_treeText.setFont(font);
    m_treeText.setCharacterSize(UITextSize);
    m_treeText.setFillColour(TextNormalColour);
    m_treeText.setShadowColour(LeaderboardTextDark);

    m_bracketTexture.loadFromFile("assets/golf/images/bracket.png");
    m_treeQuad.setTexture(m_bracketTexture);
}

void TournamentState::addSystems()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::UISystem>(false);
    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
}

void TournamentState::buildScene()
{
    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");
    m_audioEnts[AudioID::Switch] = m_scene.createEntity();
    m_audioEnts[AudioID::Switch].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch02");

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
                e.getComponent<cro::Transform>().setScale({ m_viewScale.x, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime) });
                if (currTime == 1)
                {
                    state = RootCallbackData::FadeOut;
                    e.getComponent<cro::Callback>().active = false;

                    m_scene.setSystemActive<cro::AudioPlayerSystem>(true);

                    m_scene.setSystemActive<cro::UISystem>(true);
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                    m_scene.getSystem<cro::UISystem>()->selectAt(CareerStart);
                    WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Choosing a Tournament" }));

                    applySettingsValues(); //loadConfig() might not load anything
                    loadConfig();
                    refreshTree();

                    if (!m_sharedData.unlockedItems.empty())
                    {
                        requestStackPush(StateID::Unlock);
                    }
                    else
                    {
                        //show the tournament info pane if we haven't played a tournament yet
                        //TODO this needs a better case to make sure it only shows once
                        //probably check the stats too to see any were completed yet?
                        if (m_sharedData.tournaments[0].winner == -2
                            && m_sharedData.tournaments[1].winner == -2)
                        {
                            //enterInfoCallback();
                            //LogI << "Implement me! " << cro::FileSystem::getFileName(__FILE__) << ", " << __LINE__ << std::endl;
                        }
                    }


                    //show club warning (in case clubset was changed elsewhere)
                    refreshClubsetWarning();

                    m_axisPosition = 0.f;

                    //start title animation
                    cro::Command cmd;
                    cmd.targetFlags = CommandID::Menu::TitleText;
                    cmd.action = [](cro::Entity t, float)
                        {
                            t.getComponent<cro::Callback>().setUserData<float>(0.f);
                            t.getComponent<cro::Callback>().active = true;
                        };
                    m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

                    auto* msg = cro::App::getInstance().getMessageBus().post<SystemEvent>(cl::MessageID::SystemMessage);
                    msg->type = SystemEvent::MenuChanged;
                    msg->data = -1;
                }
                break;
            case RootCallbackData::FadeOut:
                currTime = std::max(0.f, currTime - (dt * 2.f));
                e.getComponent<cro::Transform>().setScale({ m_viewScale.x, m_viewScale.y * cro::Util::Easing::easeOutQuint(currTime) });


                //interestingly only clang tells us capturing a structured binding is C++20 (we're using 17)
                auto ct = currTime;

                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::TitleText;
                cmd.action =
                    [ct](cro::Entity t, float)
                    {
                        t.getComponent<cro::Transform>().setScale(glm::vec2(ct));
                    };
                m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


                if (currTime == 0)
                {
                    requestStackPop();

                    state = RootCallbackData::FadeIn;
                    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
                }
                break;
            }

        };

    m_rootNode = rootNode;

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/tournament_menu.spt", m_sharedData.sharedResources->textures);

    //background
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 10.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;
    createProfileLayout(bgEnt, spriteSheet);

    //title
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 1.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //dummy menu ent for transitions
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    //cursor
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 23.f, -3.f, -0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cursor");
    entity.addComponent<cro::SpriteAnimation>().play(0);

    auto selectCursor = m_scene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectCursor = m_scene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });

    auto selectHighlight = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            //for the boink anim
            if (e.hasComponent<cro::Callback>())
            {
                e.getComponent<cro::Callback>().active = true;
            }
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectHighlight = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto selectOffset = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom += bounds.height;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectOffset = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom -= bounds.height;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });



    cro::String playerName;
    if (Social::isAvailable())
    {
        playerName = Social::getPlayerName();
    }
    else
    {
        playerName = m_sharedData.localConnectionData.playerData[0].name;
    }

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto& labelFont = m_sharedData.sharedResources->fonts.get(FontID::Label);

    //scroll tournament tree left
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 133.f, 221.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("previous");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerScrollPrev);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerScrollNext, CareerTournNext);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerOptions, CareerProfile);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                
                auto idx = m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID;
                idx = (idx + (ScrollPositions.size() - 1)) % ScrollPositions.size();
                m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = idx;
            }
        }
    );
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //scroll tournament tree right
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 361.f, 221.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("next");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerScrollNext);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerOptions, CareerTournNext);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerScrollPrev, CareerReset);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                
                auto idx = m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID;
                idx = (idx + 1) % ScrollPositions.size();
                m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = idx;
            }
        }
    );
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //current tournament name
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgEnt.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 98.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(TournamentNames[tournamentID]);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //prev tournament
    m_titleString = entity;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 133.f, 88.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("previous");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerTournPrev);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerTournNext, CareerClubStats);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerTournNext, CareerScrollPrev);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                tournamentID = (tournamentID + 1) % 2;
                refreshTree();
            }
        }
    );
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //next tournament
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 361.f, 88.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("next");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerTournNext);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerTournPrev, CareerGimme);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerTournPrev, CareerScrollNext);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                tournamentID = (tournamentID + 1) % 2;
                refreshTree();
            }
        }
    );
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //displays winner if complete else current round / course
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgEnt.getComponent<cro::Sprite>().getTextureBounds().width / 2.f, 84.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_detailString = entity;


    //tournament tree - texture is updated by refreshTree()
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 8.f, 108.f, 0.1f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea(TreeCroppingArea);
    entity.addComponent<cro::Sprite>(m_treeTexture.getTexture());
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<ScrollCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& [id, progress, velocity] = e.getComponent<cro::Callback>().getUserData<ScrollCallbackData>();

            if (id == ScrollID::Reset)
            {
                progress += velocity * dt;
                velocity *= 0.95f;

                while (progress > TreeTexSizeF.x)
                {
                    progress -= TreeTexSizeF.x;
                }

                if (velocity < 1.f)
                {
                    refreshTree(); //this makes sure to set the correct index to scroll to
                    velocity = ScrollCallbackData::DefaultVelocity;
                    quitConfirmCallback();
                }
            }
            else
            {
                const auto target = ScrollPositions[id];
                const auto movement = (target - progress) * (dt * 10.f);
                progress += movement;
            }

            auto origin = e.getComponent<cro::Transform>().getOrigin();
            origin.x = std::round(progress);
            e.getComponent<cro::Transform>().setOrigin(origin);

            auto crop = TreeCroppingArea;
            crop.left = origin.x;
            e.getComponent<cro::Drawable2D>().setCroppingArea(crop);
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_treeRoot = entity;


    //gimme
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 310.f, 54.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("No Gimme");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_settingsDetails.gimme = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 310.f, 50.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("gimme_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerGimme);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerWeather, Social::getClubLevel() ? CareerClubs : CareerReset);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerClubStats, CareerTournNext);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_sharedData.gimmeRadius = (m_sharedData.gimmeRadius + 1) % 3;
                m_settingsDetails.gimme.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
            }
        }
    );
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height / 2.f) });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //weather icon
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("weather_icon");
    auto iconEnt = entity;
    bounds = entity.getComponent<cro::Sprite>().getTextureRect();

    //text for current weather type
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 406.f, 54.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setString(WeatherStrings[m_sharedData.weatherType]);
    centreText(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::uint32_t>(WeatherType::Count);
    entity.getComponent<cro::Callback>().function =
        [&, iconEnt, bounds](cro::Entity e, float) mutable
        {
            auto& lastType = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
            if (lastType != m_sharedData.weatherType)
            {
                e.getComponent<cro::Text>().setString(WeatherStrings[m_sharedData.weatherType]);
                centreText(e);

                auto x = std::round(cro::Text::getLocalBounds(e).width) + 2.f;
                iconEnt.getComponent<cro::Transform>().setPosition(glm::vec3(x, -9.f, 0.1f));

                auto rect = bounds;
                rect.left += bounds.width * m_sharedData.weatherType;
                iconEnt.getComponent<cro::Sprite>().setTextureRect(rect);
            }
            lastType = m_sharedData.weatherType;
        };

    entity.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 411.f, 50.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("weather_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerWeather);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerStats, CareerNight);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerGimme, CareerOptions);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_sharedData.weatherType = (m_sharedData.weatherType + 1) % WeatherType::Count;
            }
        }
    );
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height / 2.f) });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //checkbox to show night time status
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 382.f, 30.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("square");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_settingsDetails.night = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 380.f, 28.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 60.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerNight);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerInfo, CareerReset);
    entity.getComponent<cro::UIInput>().setPrevIndex(Social::getClubLevel() ? CareerClubs : CareerClubStats, CareerWeather);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
        m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = 
        m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_sharedData.nightTime = m_sharedData.nightTime ? 0 : 1;
                    const float scale = m_sharedData.nightTime ? 1.f : 0.f;
                    m_settingsDetails.night.getComponent<cro::Transform>().setScale(glm::vec2(scale));

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //stats button highlight
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 475.f, 50.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("info_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerStats);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerClubStats, CareerInfo);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerWeather, CareerOptions);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    enterStatCallback();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //info button highlight
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 475.f, 31.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("info_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerInfo);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerClubStats, CareerStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerNight, CareerStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    enterInfoCallback();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    
    //options button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 461.f, 241.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("options_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerOptions);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerScrollPrev, CareerStats);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerScrollNext, CareerReset);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = false; //don't update menu when there's another state active

                requestStackPush(StateID::Options);
            }
        }
    );
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //select club set
    if (Social::getClubLevel())
    {
        m_sharedData.preferredClubSet %= (Social::getClubLevel() + 1);

        //warning string 
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 364.f, 37.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(std::uint32_t(0x26A0));
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_warningString = entity;


        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 315.f, 27.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bag_select");
        entity.addComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        auto buttonEnt = entity;
        /*entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -32.f, 9.f, 0.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("Clubs:");
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());*/


        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ -2.f, -2.f, 0.f });
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bag_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CareerClubs);
        entity.getComponent<cro::UIInput>().setNextIndex(CareerNight, CareerReset);
        entity.getComponent<cro::UIInput>().setPrevIndex(CareerClubStats, CareerGimme);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 
            m_scene.getSystem<cro::UISystem>()->addCallback(
            [&, buttonEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    //we're not connected yet so we have to rely on joining the server to send tis
                    m_sharedData.preferredClubSet = (m_sharedData.preferredClubSet + 1) % (Social::getClubLevel() + 1);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    buttonEnt.getComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);

                    if (m_sharedData.tournaments[tournamentID].winner == -2
                        && m_sharedData.tournaments[tournamentID].initialClubSet > -1
                        && m_sharedData.tournaments[tournamentID].initialClubSet != m_sharedData.preferredClubSet)
                    {
                        //show warning for changing clubs mid-tournament
                        m_warningString.getComponent<cro::Transform>().setScale(glm::vec2(1.f/m_viewScale.x));
                    }
                    else
                    {
                        //hide it
                        m_warningString.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    }
                }
            });
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        buttonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_settingsDetails.clubset = buttonEnt;
    }


    //club stats button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 27.f, 23.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("clubinfo_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerClubStats);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerGimme, CareerProfile);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerInfo, CareerTournPrev);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = false; //don't update menu when there's another state active

                requestStackPush(StateID::Stats);
            }
        }
    );
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //entity with confirmation for starting round
    createConfirmMenu(rootNode);

    //and info overlay
    createInfoMenu(rootNode);

    //and stat info
    createStatMenu(rootNode);

    //banner across bottom
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("banner_small");
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, BannerPosition };
    entity.getComponent<UIElement>().depth = -0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteRect](cro::Entity e, float)
        {
            auto rect = spriteRect;
            rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
            e.getComponent<cro::Sprite>().setTextureRect(rect);
            e.getComponent<cro::Callback>().active = false;
        };
    auto bannerEnt = entity;
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //quit
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, 2.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("exit");
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerQuit);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerProfile, CareerScrollPrev);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerStart, CareerClubStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectOffset;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectOffset;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitState();
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ 16.f, 13.f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(smallFont).setString("Back");
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(labelEnt).width;

    //profile editor
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_icon");
    entity.addComponent<UIElement>().absolutePosition = { -80.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.3334f, 0.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerProfile);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerReset, CareerScrollPrev);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerQuit, CareerClubStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = false;
                    requestStackPush(StateID::Profile);
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 10.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Edit Profile");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(entity).width;


    //rank icon
    if (Social::getLevel() > 0)
    {
        auto iconEnt = m_scene.createEntity();
        iconEnt.addComponent<cro::Transform>();
        iconEnt.addComponent<cro::Drawable2D>();
        iconEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("rank_icon");
        iconEnt.addComponent<cro::SpriteAnimation>().play(std::min(5, Social::getLevel() / 10));
        auto bounds = iconEnt.getComponent<cro::Sprite>().getTextureBounds();
        iconEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        iconEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.f };
        iconEnt.getComponent<UIElement>().absolutePosition = { 0.f, 11.f };
        iconEnt.getComponent<UIElement>().depth = 0.2f;
        iconEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
        bannerEnt.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());
    }

    //reset current tournament
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("league_icon");
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.6667f, 0.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerReset);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerStart, CareerOptions);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerProfile, CareerNight);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    enterConfirmCallback(true, true);
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 10.f, 0.1f });;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Restart Tournament");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    iconEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    iconEnt.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(entity).width;

    //ticker for freeplay reminder
    //entity = m_scene.createEntity();
    //entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    //entity.addComponent<cro::Drawable2D>();
    //entity.addComponent<cro::Text>(smallFont).setString("Don't forget you can practice any course at any time in Free Play mode!");
    //entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    //entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    ////can't do this because the callback specifies the position
    ////entity.addComponent<UIElement>().absolutePosition = { 0.f, 15.f };
    ////entity.getComponent<UIElement>().relativePosition = { 0.6667f, 0.f };
    ////entity.getComponent<UIElement>().depth = 0.2f;
    //entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    //entity.addComponent<cro::Callback>().active = true;
    //entity.getComponent<cro::Callback>().setUserData<ScrollData>();
    //entity.getComponent<cro::Callback>().getUserData<ScrollData>().bounds = cro::Text::getLocalBounds(entity);
    //entity.getComponent<cro::Callback>().getUserData<ScrollData>().bounds.height += 2.f;
    //entity.getComponent<cro::Callback>().function =
    //    [&](cro::Entity e, float dt)
    //    {
    //        auto& [bounds, xPos] = e.getComponent<cro::Callback>().getUserData<ScrollData>();
    //        xPos -= (dt * 30.f);

    //        static constexpr float BGWidth = 198.f;

    //        if (xPos < (-bounds.width))
    //        {
    //            xPos = BGWidth + 20.f;
    //        }

    //        auto pos = e.getComponent<cro::Transform>().getPosition();
    //        pos.x = std::round(xPos);

    //        e.getComponent<cro::Transform>().setPosition(pos);

    //        auto cropping = bounds;
    //        cropping.left = -pos.x;
    //        cropping.left += 20.f;
    //        cropping.width = BGWidth;
    //        e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
    //    };
    //bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -16.f, 5.f };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("start");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left -= 3.f;
    bounds.width += 6.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Career);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CareerStart);
    entity.getComponent<cro::UIInput>().setNextIndex(CareerQuit, CareerOptions);
    entity.getComponent<cro::UIInput>().setPrevIndex(CareerReset, CareerInfo);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    saveConfig();

                    //show confirmation
                    enterConfirmCallback(m_sharedData.tournaments[tournamentID].winner != -2, false);
                }
            });
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto updateView = [&, rootNode, bannerEnt](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        //rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&, size, rootNode](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = (element.relativePosition * size) / m_viewScale;
            pos -= glm::vec2(rootNode.getComponent<cro::Transform>().getPosition()) / m_viewScale;
            pos += element.absolutePosition;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        bannerEnt.getComponent<cro::Callback>().active = true;

        if (m_warningString.isValid())
        {
            const auto charSize = InfoTextSize * static_cast<std::uint32_t>(m_viewScale.x);
            m_warningString.getComponent<cro::Text>().setCharacterSize(charSize);

            refreshClubsetWarning();
        }
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void TournamentState::createConfirmMenu(cro::Entity parent)
{
    auto& menuTransform = parent.getComponent<cro::Transform>();

    auto enter = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto exit = m_scene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    //quit confirmation
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_sharedData.sharedResources->textures);


    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("message_board");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
            float scale = 0.f;
            if (data.dir == ConfirmationData::In)
            {
                data.progress = std::min(1.f, data.progress + (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);

                if (data.progress == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::ConfirmQuit);

                    m_currentMenu = MenuID::ConfirmQuit;
                    m_scene.getSystem<cro::UISystem>()->selectAt(1);
                }
            }
            else
            {
                data.progress = std::max(0.f, data.progress - (dt * 4.f));
                scale = cro::Util::Easing::easeOutQuint(data.progress);
                if (data.progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_currentMenu = MenuID::Career;

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                }
            }

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;


    //quad to darken the screen
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().function =
        [&, confirmEnt](cro::Entity e, float)
        {
            auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
            scale = std::min(1.f, scale);

            if (scale > 0)
            {
                auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
                e.getComponent<cro::Transform>().setScale(size / scale);
            }

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
            }

            e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
            //m_scene.getActiveCamera().getComponent<cro::Camera>().active = confirmEnt.getComponent<cro::Callback>().active;
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto shadeEnt = entity;



    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);


    //confirmation text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Start Game?");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto titleEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 46.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("This Will Reset\nThe Current Tournament.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setVerticalSpacing(-2.f);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto resetMessageEnt = entity;


    //stash this so we can access it from the event handler (escape to ignore etc)
    quitConfirmCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 20.f, 24.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("No");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitConfirmCallback();
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //track the callback IDs separately so we can change the behaviour of the Yes button
    const auto startGameCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                m_sharedData.activeTournament = tournamentID;

                auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                msg->type = SystemEvent::MenuRequest;
                msg->data = RequestID::Tournament;
            }
        });

    const auto restartCallback = m_scene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy); //disable the input so we can't do anything while anim plays

                //m_sharedData.tournaments[tournamentID] = {}; //don't do this it erases the ID
                //m_sharedData.tournaments[tournamentID].id = tournamentID;
                resetTournament(m_sharedData.tournaments[tournamentID]);
                writeTournamentData(m_sharedData.tournaments[tournamentID]);

                //trigger animation
                m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = ScrollID::Reset;
            }
        });

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 20.f, 24.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("Yes");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = startGameCallback;
    auto buttonEnt = entity;
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 44.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Pro clubs can be tough to use!");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto messageEnt = entity;


    //displays the message
    enterConfirmCallback = 
        [&, confirmEnt, shadeEnt, titleEnt, buttonEnt, messageEnt,
        resetMessageEnt, startGameCallback, restartCallback](bool showReset, bool resetTournament) mutable
    {
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        if (resetTournament)
        {
            titleEnt.getComponent<cro::Text>().setString("Restart?");
            buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = restartCallback;
        }
        else
        {
            titleEnt.getComponent<cro::Text>().setString("Start Game?");
            buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = startGameCallback;
        }

        if (!showReset)
        {
            if (m_sharedData.preferredClubSet == 2)
            {
                messageEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
            else
            {
                messageEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
            resetMessageEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        else
        {
            resetMessageEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            messageEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };
}

void TournamentState::createInfoMenu(cro::Entity parent)
{
    auto& menuTransform = parent.getComponent<cro::Transform>();

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/avatar_browser.spt", m_sharedData.sharedResources->textures);

    //background sprite
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
            float scale = 0.f;
            if (data.dir == ConfirmationData::In)
            {
                data.progress = std::min(1.f, data.progress + (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);

                if (data.progress == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Info);

                    m_currentMenu = MenuID::Info;
                    m_scene.getSystem<cro::UISystem>()->selectAt(1);
                }
            }
            else
            {
                data.progress = std::max(0.f, data.progress - (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);
                if (data.progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_currentMenu = MenuID::Career;

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                }
            }

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;


    //close button
    auto buttonEnt = m_scene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ 468.f, 331.f, 0.1f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_button");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().setGroup(MenuID::Info);
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [](cro::Entity e) 
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
            });
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            });
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitInfoCallback();
                }
            });
    buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    confirmEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());




    //quad to darken the screen (idk we could probably recycle this from confirm menu, but meh)
    bounds = confirmEnt.getComponent<cro::Sprite>().getTextureBounds();
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().function =
        [&, confirmEnt](cro::Entity e, float)
        {
            auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
            scale = std::min(1.f, scale);

            if (scale > 0)
            {
                auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
                e.getComponent<cro::Transform>().setScale(size / scale);
            }

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
            }

            e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto shadeEnt = entity;


    quitInfoCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    enterInfoCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 298.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Welcome To The Tournaments!");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    std::string desc = R"(
There are two tournaments available; the Dagle-Bunnage Cup and the Sammonfield Championship.
Each tournament has 16 players, with initial positions drawn based on your current XP level.

Each round is 9 holes - the front 9 if you draw the left bracket, or the back 9 if you draw
the right bracket. The exception is the final round which takes place across all 18 holes.

Rounds are played one on one - if you lose your round you are eliminated and you'll have
to start again.

Unlike Free Play mode you can leave a Tournament round at any time, and resume it again
when you're ready. Career mode will even remember which hole you're on!

Both Tournaments are always available and can be played at any time. Completing a
Tournament in first place will unlock a new ball for your profile. These items remain
unlocked even if you decide to reset your Career.

You can reset your Career at any time from the Settings page of the Options menu, and
edit your opponent's names in the League Browser.

Good Luck!
)";
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 280.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString(desc);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //animated decorations
    spriteSheet.loadFromFile("assets/golf/sprites/main_menu.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 19.f, 22.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag_base");
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 1.f, 27.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    spriteSheet.loadFromFile("assets/golf/sprites/bounce.spt", m_sharedData.sharedResources->textures);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 95.f, 22.f, 0.05f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("bounce");
    entity.getComponent<cro::Sprite>().getAnimations()[0].looped = false;
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(10.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
            currTime -= dt;
            if (currTime < 0.f
                && m_scene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Info)
            {
                currTime += 22.f + cro::Util::Random::value(-4, 3);
                e.getComponent<cro::SpriteAnimation>().play(0);
            }
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void TournamentState::createStatMenu(cro::Entity parent)
{
    auto& menuTransform = parent.getComponent<cro::Transform>();

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/tourn_stats.spt", m_sharedData.sharedResources->textures);

    //background sprite
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = 1.8f;
    entity.addComponent<cro::Callback>().setUserData<ConfirmationData>();
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& data = e.getComponent<cro::Callback>().getUserData<ConfirmationData>();
            float scale = 0.f;
            if (data.dir == ConfirmationData::In)
            {
                data.progress = std::min(1.f, data.progress + (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);

                if (data.progress == 1)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Stat);

                    m_currentMenu = MenuID::Stat;
                    m_scene.getSystem<cro::UISystem>()->selectAt(1);
                }
            }
            else
            {
                data.progress = std::max(0.f, data.progress - (dt * 2.f));
                scale = cro::Util::Easing::easeOutBack(data.progress);
                if (data.progress == 0)
                {
                    e.getComponent<cro::Callback>().active = false;
                    m_currentMenu = MenuID::Career;

                    m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Career);
                }
            }

            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;


    //close button
    auto buttonEnt = m_scene.createEntity();
    buttonEnt.addComponent<cro::Transform>().setPosition({ 161.f, 69.f, 0.1f });
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_button");
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::UIInput>().setGroup(MenuID::Stat);
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.getComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
            });
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [](cro::Entity e)
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            });
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_scene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitStatCallback();
                }
            });
    buttonEnt.addComponent<cro::Callback>().function = MenuTextCallback();
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    confirmEnt.getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());




    //quad to darken the screen (idk we could probably recycle this from confirm menu, but meh)
    bounds = confirmEnt.getComponent<cro::Sprite>().getTextureBounds();
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().function =
        [&, confirmEnt](cro::Entity e, float)
        {
            auto scale = confirmEnt.getComponent<cro::Transform>().getScale().x;
            scale = std::min(1.f, scale);

            if (scale > 0)
            {
                auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
                e.getComponent<cro::Transform>().setScale(size / scale);
            }

            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour.setAlpha(BackgroundAlpha * confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().progress);
            }

            e.getComponent<cro::Callback>().active = confirmEnt.getComponent<cro::Callback>().active;
        };
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto shadeEnt = entity;

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 10.f, 50.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setVerticalSpacing(-1.f);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto textEnt = entity;

    quitStatCallback = [&, confirmEnt, shadeEnt]() mutable
        {
            confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
            confirmEnt.getComponent<cro::Callback>().active = true;
            shadeEnt.getComponent<cro::Callback>().active = true;
            m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        };

    enterStatCallback = [&, confirmEnt, shadeEnt, textEnt]() mutable
        {
            m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
            confirmEnt.getComponent<cro::Callback>().active = true;
            shadeEnt.getComponent<cro::Callback>().active = true;

            const auto entered = static_cast<std::int32_t>(Achievements::getStat(StatStrings[StatID::UnrealPlayed + tournamentID])->value);
            const auto won = static_cast<std::int32_t>(Achievements::getStat(StatStrings[StatID::UnrealWon + tournamentID])->value);
            const auto tier = static_cast<std::int32_t>(Achievements::getStat(StatStrings[StatID::UnrealBest + tournamentID])->value);

            std::string s = "Number of times completed: " + std::to_string(entered);
            s += "\nNumber of times won: " + std::to_string(won);
            if (tier >  2) //somewhere along the line tier was getting set to 5, so hack around it
            {
                s += "\nBest rank: Winner!";
            }
            else
            {
                s += "\nBest tier rating: " + std::to_string(tier + 1);
            }

            textEnt.getComponent<cro::Text>().setString(s);

            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
        };
}

void TournamentState::createProfileLayout(cro::Entity bgEnt, const cro::SpriteSheet& spriteSheet)
{
    //XP info
    const float xPos = bgEnt.getComponent<cro::Sprite>().getTextureRect().width / 2.f;
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ xPos, 10.f, 0.1f });

    const auto progress = Social::getLevelProgress();
    constexpr float BarWidth = 80.f;

    entity.addComponent<cro::Drawable2D>().setVertexData(createXPBar(BarWidth, progress.progress));
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ (-BarWidth / 2.f) + 2.f, 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Level " + std::to_string(Social::getLevel()));
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(-xPos * 0.6f), 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString(std::to_string(progress.currentXP) + "/" + std::to_string(progress.levelXP) + " XP");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    centreText(labelEnt);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    labelEnt = m_scene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(xPos * 0.6f), 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Total: " + std::to_string(Social::getXP()) + " XP");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    centreText(labelEnt);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    //club status - render to a texture to save rendering
    //a whole bunch of different text entities
    if (m_clubTexture.create(210, 36, false))
    {
        auto clubEnt = m_scene.createEntity();
        clubEnt.addComponent<cro::Transform>().setPosition({ 30.f, 26.f, 0.1f });
        clubEnt.addComponent<cro::Drawable2D>();
        clubEnt.addComponent<cro::Sprite>(m_clubTexture.getTexture());
        bgEnt.getComponent<cro::Transform>().addChild(clubEnt.getComponent<cro::Transform>());

        cro::SimpleText text(font);
        text.setFillColour(TextNormalColour);
        text.setShadowColour(LeaderboardTextDark);
        text.setShadowOffset({ 1.f, -1.f });
        text.setCharacterSize(InfoTextSize);

        glm::vec2 position(2.f, 28.f);

        m_clubTexture.clear(cro::Colour::Transparent);

        static constexpr std::int32_t Col = 4;
        static constexpr std::int32_t Row = 3;

        const auto flags = Social::getUnlockStatus(Social::UnlockType::Club);

        for (auto i = 0; i < Row; ++i)
        {
            for (auto j = 0; j < Col; ++j)
            {
                auto idx = i + (j * Row);
                text.setString(Clubs[idx].getLabel() + " ");
                text.setPosition(position);

                if (flags & (1 << idx))
                {
                    if (ClubID::DefaultSet & (1 << idx))
                    {
                        text.setFillColour(TextNormalColour);
                    }
                    else
                    {
                        text.setFillColour(TextGoldColour);
                    }
                }
                else
                {
                    text.setFillColour(TextHighlightColour);
                }

                text.draw();
                position.x += 49.f;
            }
            position.x = 2.f;
            position.y -= 12.f;
        }

        m_clubTexture.display();


        //TODO make this a button which deactivates the scene camera
        //and pushes the stats state on top so we can view the club data
    }
}

void TournamentState::applySettingsValues()
{
    if (m_settingsDetails.clubset.isValid())
    {
        m_settingsDetails.clubset.getComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
    }
    m_settingsDetails.gimme.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
    const float scale = m_sharedData.nightTime ? 1.f : 0.f;
    m_settingsDetails.night.getComponent<cro::Transform>().setScale(glm::vec2(scale));
}

void TournamentState::refreshTree()
{
    m_treeText.setShadowOffset({ 0.f, 0.f });

    static constexpr auto CPUPlayerColour = cro::Colour(0xdbaf77ff);
    cro::String playerName;
    if (Social::isAvailable())
    {
        playerName = Social::getPlayerName();
    }
    else
    {
        playerName = m_sharedData.localConnectionData.playerData[0].name;
    }

    const auto setNameData = [&](std::int32_t id, bool activeRound)
        {
            static constexpr float activeAlpha = 0.85f;

            if (id > -1)
            {
                auto c = CPUPlayerColour;
                if (!activeRound)
                {
                    c.setAlpha(activeAlpha);
                }

                m_treeText.setString(m_sharedData.leagueNames[id]);
                m_treeText.setFillColour(c);
            }
            else
            {
                if (id == -1)
                {
                    auto c = TextNormalColour;
                    if (!activeRound)
                    {
                        c.setAlpha(activeAlpha);
                    }

                    m_treeText.setString(playerName);
                    m_treeText.setFillColour(c);
                    return true; //just to track if we're setting the player's name
                }
                else
                {
                    m_treeText.setString("N/A");
                    m_treeText.setFillColour(LeaderboardTextDark);
                }
            }
            return false;
        };

    const auto& t = m_sharedData.tournaments[tournamentID];
    m_treeText.setAlignment(cro::SimpleText::Alignment::Right);
    m_treeText.setPosition(Tier0Position);

    m_treeTexture.clear(cro::Colour::Transparent);
    m_treeQuad.draw();
    
    auto bracket = ScrollID::Left;

    //tier 0
    for (auto i = 0u; i < t.tier0.size() / 2; ++i)
    {
        setNameData(t.tier0[i], t.round == 0);
        m_treeText.draw();
        m_treeText.move({ 0.f, -Tier0Spacing });
    }
    m_treeText.setAlignment(cro::SimpleText::Alignment::Left);
    m_treeText.setPosition(Tier0Position);
    m_treeText.move({ Tier0Stride, 0.f });
    for (auto i = t.tier0.size() / 2; i < t.tier0.size(); ++i)
    {
        if (setNameData(t.tier0[i], t.round == 0))
        {
            bracket = ScrollID::Right;
        }
        m_treeText.draw();
        m_treeText.move({ 0.f, -Tier0Spacing });
    }


    //tier 1
    m_treeText.setAlignment(cro::SimpleText::Alignment::Right);
    m_treeText.setPosition(Tier1Position);
    for (auto i = 0u; i < t.tier1.size() / 2; ++i)
    {
        setNameData(t.tier1[i], t.round == 1);
        m_treeText.draw();
        m_treeText.move({ 0.f, -Tier1Spacing });
    }
    m_treeText.setAlignment(cro::SimpleText::Alignment::Left);
    m_treeText.setPosition(Tier1Position);
    m_treeText.move({ Tier1Stride, 0.f });
    for (auto i = t.tier1.size() / 2; i < t.tier1.size(); ++i)
    {
        setNameData(t.tier1[i], t.round == 1);
        m_treeText.draw();
        m_treeText.move({ 0.f, -Tier1Spacing });
    }


    //tier 2
    m_treeText.setAlignment(cro::SimpleText::Alignment::Right);
    m_treeText.setPosition(Tier2Position);
    for (auto i = 0u; i < t.tier2.size() / 2; ++i)
    {
        setNameData(t.tier2[i], t.round == 2);
        m_treeText.draw();
        m_treeText.move({ 0.f, -Tier2Spacing });
    }
    m_treeText.setAlignment(cro::SimpleText::Alignment::Left);
    m_treeText.setPosition(Tier2Position);
    m_treeText.move({ Tier2Stride, 0.f });
    for (auto i = t.tier2.size() / 2; i < t.tier2.size(); ++i)
    {
        setNameData(t.tier2[i], t.round == 2);
        m_treeText.draw();
        m_treeText.move({ 0.f, -Tier2Spacing });
    }



    //finals (tier 3)
    m_treeText.setAlignment(cro::SimpleText::Alignment::Centre);
    m_treeText.setPosition({ TreeTexSizeF.x / 2.f, Tier0Position.y });
    
    setNameData(t.tier3[0], t.round == 3);
    m_treeText.draw();
    m_treeText.move({ 0.f, -Tier0Spacing * 7.f});

    setNameData(t.tier3[1], t.round == 3);
    m_treeText.draw();

    //winner
    setNameData(t.winner, true);
    m_treeText.setPosition({ TreeTexSizeF.x / 2.f, (TreeTexSizeF.y / 2.f) - 32.f });
    if (t.winner != -2)
    {
        m_treeText.setShadowOffset({ 1.f, -1.f });
    }
    m_treeText.draw();
    

    m_treeTexture.display();




    //m_treeTexture.getTexture().saveToFile("tree.png");


    //update the info string
    cro::String str;
    if (t.winner == -2)
    {
        cro::String courseName;
        const auto& p = TournamentCourses[tournamentID][t.round];
        const auto& courseData = m_sharedData.courseData->courseData;
        if (const auto res = std::find_if(courseData.begin(), courseData.end(), [&](const SharedCourseData::CourseData& cd) 
            {return cd.directory == p;}); res != courseData.end())
        {
            courseName = res->title;
        }
        else
        {
            courseName = "Buns.";
        }

        //display the current round/course
        str = "Round " + std::to_string(t.round + 1);
        
        const auto hole = getTournamentHoleIndex(t);
        if (hole)
        {
            str += " (Hole " + std::to_string(hole + 1) + ")";
        }
        
        str += " - " + courseName;

        if (t.round < 3)
        {
            m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = t.round < 2 ? bracket : ScrollID::Centre;

            if (bracket == ScrollID::Left)
            {
                str += " Front 9";
            }
            else
            {
                str += " Back 9";
            }
        }
        else
        {
            m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = ScrollID::Centre;
            str += " 18 Holes";
        }
    }
    else
    {
        //set the winner name
        if (t.winner == -1)
        {
            str = "Winner: " + playerName;
        }
        else
        {
            str = "Winner: " + m_sharedData.leagueNames[t.winner];
        }
        m_treeRoot.getComponent<cro::Callback>().getUserData<ScrollCallbackData>().scrollID = ScrollID::Centre;
    }
    

    refreshClubsetWarning();

    m_detailString.getComponent<cro::Text>().setString(str);
    m_titleString.getComponent<cro::Text>().setString(TournamentNames[tournamentID]);
    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
}

void TournamentState::refreshClubsetWarning()
{
    if (m_warningString.isValid())
    {
        const float scale =
            (m_sharedData.tournaments[tournamentID].winner == -2
                && m_sharedData.tournaments[tournamentID].initialClubSet > -1
                && m_sharedData.tournaments[tournamentID].initialClubSet != m_sharedData.preferredClubSet) ? (1/m_viewScale.x) : 0.f;

        m_warningString.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    }
}

void TournamentState::quitState()
{
    saveConfig();

    if (m_currentMenu == MenuID::ConfirmQuit)
    {
        quitConfirmCallback();
    }
    else if (m_currentMenu == MenuID::Info)
    {
        quitInfoCallback();
    }
    else if (m_currentMenu == MenuID::Stat)
    {
        quitStatCallback();
    }
    else if (m_currentMenu == MenuID::Career)
    {
        WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Main Menu" }));

        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_scene.setSystemActive<cro::UISystem>(false);

        m_rootNode.getComponent<cro::Callback>().active = true;
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

        auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
        msg->type = SystemEvent::MenuRequest;
        msg->data = StateID::Career;
    }
}

void TournamentState::loadConfig()
{
    const auto path = Content::getUserContentPath(Content::UserContent::Career) + ConfigFile;
    if (cro::FileSystem::fileExists(path))
    {
        cro::ConfigFile cfg;
        cfg.loadFromFile(path, false);

        for (const auto& prop : cfg.getProperties())
        {
            const auto& name = prop.getName();
            if (name == "gimme")
            {
                m_sharedData.gimmeRadius = static_cast<std::uint8_t>(std::clamp(prop.getValue<std::int32_t>(), 0, 2));
            }
            else if (name == "night")
            {
                m_sharedData.nightTime = static_cast<std::uint8_t>(std::clamp(prop.getValue<std::int32_t>(), 0, 1));
            }
            else if (name == "weather")
            {
                m_sharedData.weatherType = std::clamp(prop.getValue<std::int32_t>(), 0, static_cast<std::int32_t>(WeatherType::Count));
            }
        }

        applySettingsValues();
    }
}

void TournamentState::saveConfig() const
{
    cro::ConfigFile cfg;
    cfg.addProperty("gimme").setValue(m_sharedData.gimmeRadius);
    cfg.addProperty("night").setValue(m_sharedData.nightTime);
    cfg.addProperty("weather").setValue(m_sharedData.weatherType);
    cfg.save(Content::getUserContentPath(Content::UserContent::Career) + ConfigFile);
}

void TournamentState::onCachedPush()
{
    if (m_sharedData.showClubUpdate)
    {
        requestStackPush(StateID::ClubInfo);
    }
}