/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2026
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

//TODO a bunch of these includes aren't needed
#include "EditTournamentState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "PacketIDs.hpp"
#include "Utility.hpp"
#include "TextAnimCallback.hpp"
#include "Career.hpp"
#include "Tournament.hpp"
#include "spooky2.hpp"
#include "../GolfGame.hpp"
#include "../WebsocketServer.hpp"

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
#include <cstring>

namespace
{
    struct ButtonID final
    {
        enum
        {
            //don't change this order, callbacks depend on it!!
            T1Down = 100, T1Up,
            T2Down,       T2Up,
            T3Down,       T3Up,
            T4Down,       T4Up,
            //these are OK to change
            Name, Save, Cancel
        };
    };
}

EditTournamentState::EditTournamentState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus()),
    m_sharedData        (sd),
    m_viewScale         (2.f),
    m_showOSK           (false),
    m_showImguiInput    (false)
{
    ctx.mainWindow.setMouseCaptured(false);
    m_scene.setTitle("Tournament Edit");

    buildScene();
    loadCourseInfo();
    registerWindow(std::bind(&EditTournamentState::imguiWindow, this));

    std::fill(m_tierIndices.begin(), m_tierIndices.end(), 0);
}

//public
bool EditTournamentState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    if (m_showImguiInput
        || m_showOSK)
    {
        return false;
    }

    if (evt.type == SDL_EVENT_KEY_UP)
    {
        if (evt.key.key == SDLK_BACKSPACE
            || evt.key.key == SDLK_ESCAPE
            || evt.key.key == SDLK_P)
        {
            quitState();
            return false;
        }
    }
    else if (evt.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
    {
        if (evt.gbutton.button == cro::GameController::ButtonB)
        {
            quitState();
            return false;
        }
    }

    else if (evt.type == SDL_EVENT_MOUSE_BUTTON_UP)
    {
        if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            quitState();
            return false;
        }
    }

    else if (evt.type == SDL_EVENT_KEY_DOWN)
    {
        switch (evt.key.key)
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

    else if (evt.type == SDL_EVENT_MOUSE_MOTION)
    {
        cro::App::getWindow().setMouseCaptured(false);
    }

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void EditTournamentState::handleMessage(const cro::Message& msg)
{
    if (msg.id == cl::MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::CancelOSK)
        {
            m_sharedData.useOSKBuffer = false;
            m_showOSK = false;
        }
        else if (data.type == SystemEvent::SubmitOSK)
        {
            m_sharedData.useOSKBuffer = false;
            m_showOSK = false;

            if (!m_sharedData.OSKBuffer.empty())
            {
                m_tournamentNameEntity.getComponent<cro::Text>().setString(m_sharedData.OSKBuffer);
                m_tournamentInfo.setTitle(m_sharedData.OSKBuffer);
            }
        }
    }

    m_scene.forwardMessage(msg);
}

bool EditTournamentState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void EditTournamentState::render()
{
    m_scene.render();
}

//private
void EditTournamentState::imguiWindow()
{
    if (m_showImguiInput)
    {
        const float viewScale = getViewScale();

        const auto size = glm::vec2(cro::App::getWindow().getSize());
        const glm::vec2 WindowSize = glm::vec2(200.f, 80.f) * viewScale;
        const auto WindowPos = (size - WindowSize) / 2.f;

        ImGui::SetNextWindowSize({ WindowSize.x, WindowSize.y });
        ImGui::SetNextWindowPos({ WindowPos.x, WindowPos.y });

        ImGui::GetFont()->Scale *= viewScale;
        ImGui::PushFont(ImGui::GetFont());

        ImGui::Begin("Tournament Name", &m_showImguiInput, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::SetNextItemWidth(-1.f);
        if (ImGui::InputText("##input", &m_imguiBuffer))
        {
            static constexpr std::size_t MaxChars = ConstVal::MaxStringChars;
            if (m_imguiBuffer.length() > MaxChars)
            {
                m_imguiBuffer = m_imguiBuffer.substr(0, MaxChars);
            }
        }
        if (ImGui::Button("OK", {(WindowSize.x / 2.f) - 12.f, 0.f}))
        {
            if (!m_imguiBuffer.empty())
            {
                cro::String s = cro::String::fromUtf8(m_imguiBuffer.begin(), m_imguiBuffer.end());
                m_tournamentNameEntity.getComponent<cro::Text>().setString(s);
                m_tournamentInfo.setTitle(s);
            }
            m_imguiBuffer.clear();
            m_showImguiInput = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", {-1.f, 0.f}))
        {
            m_imguiBuffer.clear();
            m_showImguiInput = false;
        }
        ImGui::End();

        ImGui::GetFont()->Scale = 1.f;
        ImGui::PopFont();
    }
}

void EditTournamentState::buildScene()
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
    spriteSheet.loadFromFile("assets/golf/sprites/tournament_editor.spt", m_textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    const auto selected = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::AudioEmitter>().play();

            const auto idx = (e.getComponent<cro::UIInput>().getSelectionIndex() - ButtonID::T1Down) / 2;
            if (idx < m_tierIndices.size())
            {
                updatePreview(m_tierIndices[idx]);
            }
        });
    const auto unselected = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    const auto nextCourse = uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                const auto idx = (e.getComponent<cro::UIInput>().getSelectionIndex() - ButtonID::T1Down) / 2;
                m_tierIndices[idx] = (m_tierIndices[idx] + 1) % m_courseInfo.size();
                updatePreview(m_tierIndices[idx]);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    const auto prevCourse = uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                const auto idx = (e.getComponent<cro::UIInput>().getSelectionIndex() - ButtonID::T1Down) / 2;
                m_tierIndices[idx] = (m_tierIndices[idx] + (m_courseInfo.size() - 1)) % m_courseInfo.size();
                updatePreview(m_tierIndices[idx]);

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    const auto createButton =
        [&](glm::vec2 pos, const std::string& sprName)
        {
            auto e = m_scene.createEntity();
            e.addComponent<cro::Transform>().setPosition(glm::vec3(pos, 0.1f));
            e.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            e.addComponent<cro::Drawable2D>();
            e.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprName);
            auto bounds = e.getComponent<cro::Sprite>().getTextureBounds();
            e.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            e.addComponent<cro::Callback>().function = MenuTextCallback();

            e.addComponent<cro::UIInput>().area = bounds;
            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;

            return e;
        };


    entity = createButton({ 110.f, 115.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T1Down);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T1Up, ButtonID::T2Down);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T1Up, ButtonID::Save);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = prevCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 226.f, 115.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T1Up);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T1Down, ButtonID::T2Up);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T1Down, ButtonID::Cancel);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = nextCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 110.f, 99.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T2Down);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T2Up, ButtonID::T3Down);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T2Up, ButtonID::T1Down);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = prevCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 226.f, 99.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T2Up);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T2Down, ButtonID::T3Up);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T2Down, ButtonID::T1Up);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = nextCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 110.f, 83.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T3Down);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T3Up, ButtonID::T4Down);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T3Up, ButtonID::T2Down);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = prevCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 226.f, 83.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T3Up);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T3Down, ButtonID::T4Up);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T3Down, ButtonID::T2Up);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = nextCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 110.f, 67.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T4Down);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T4Up, ButtonID::Name);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T4Up, ButtonID::T3Down);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = prevCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 226.f, 67.f }, "small_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::T4Up);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::T4Down, ButtonID::Name);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T4Down, ButtonID::T3Up);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = nextCourse;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 168.f, 38.f, 0.3f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Buns");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_tournamentNameEntity = entity;
    
    entity = createButton({ 168.f, 34.f }, "large_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Name);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Cancel, ButtonID::Save);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::T4Down, ButtonID::T4Up);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //TODO check this is what we do elsewhere to be consistent
                    if (/*cro::GameController::getControllerCount() != 0*/evt.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
                    {
#ifdef USE_GNS
                        if (Social::isSteamdeck(true))
                        {
                            //OSK
                            const auto cb =
                                [&](bool submitted, const char* buffer)
                                {
                                    if (submitted)
                                    {
                                        const auto str = cro::String::fromUtf8(buffer, buffer + std::strlen(buffer));
                                        if (!str.empty())
                                        {
                                            m_tournamentNameEntity.getComponent<cro::Text>().setString(str);
                                            m_tournamentInfo.setTitle(str);
                                        }
                                    }
                                };

                            //this only shows the overlay as Steam takes care of dismissing it
                            const auto utf = m_tournamentInfo.getTitle().toUtf8Char();
                            Social::showTextInput(cb, "Tournament Name", ConstVal::MaxStringChars * 2, utf.data());
                        }
                        else
#endif
                        {
                            m_showOSK = true;
                            m_sharedData.useOSKBuffer = true;
                            m_sharedData.OSKBuffer = m_tournamentInfo.getTitle();
                            requestStackPush(StateID::Keyboard);
                        }
                    }
                    else
                    {
                        //show ImGuiWindow
                        cro::App::getWindow().setMouseCaptured(false);
                        m_imguiBuffer = m_tournamentNameEntity.getComponent<cro::Text>().getString().toUtf8Char();
                        m_showImguiInput = true;
                    }
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    entity = createButton({ 140.f, 12.f }, "medium_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Save);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Cancel, ButtonID::T1Down);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Cancel, ButtonID::Name);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    //actually save
                    if (m_sharedData.tournamentPath.empty())
                    {
                        //this must be new so create a new directory
                        //the seed creates unique values when using the same name
                        const auto str = m_tournamentInfo.getTitle().toUtf8();
                        const auto h = SpookyHash::Hash32(str.data(), str.size(), std::time(nullptr));
                        m_sharedData.tournamentPath = Content::getUserContentPath(Content::UserContent::Tournament) + std::to_string(h) + "/";
                        if (!cro::FileSystem::directoryExists(m_sharedData.tournamentPath))
                        {
                            cro::FileSystem::createDirectory(m_sharedData.tournamentPath);
                        }

                        const auto filePath = m_sharedData.tournamentPath + TournamentDataFile;

                        Tournament newTournament;
                        newTournament.id = TournamentIndex::Custom;
                        resetTournament(newTournament);
                        writeTournamentData(newTournament, filePath.c_str());

                    }

                    for (auto i = 0u; i < 4u; ++i)
                    {
                        m_tournamentInfo.setCourse(i, m_courseInfo[m_tierIndices[i]].dir);
                    }
                    m_tournamentInfo.save(m_sharedData.tournamentPath);

                    Achievements::awardAchievement(AchievementStrings[AchievementID::PartyPlanner]);
                    quitState();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createButton({ 196.f, 12.f }, "medium_highlight");
    entity.getComponent<cro::UIInput>().setSelectionIndex(ButtonID::Cancel);
    entity.getComponent<cro::UIInput>().setNextIndex(ButtonID::Save, ButtonID::T1Up);
    entity.getComponent<cro::UIInput>().setPrevIndex(ButtonID::Save, ButtonID::Name);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //thumbnail
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 99.f, 149.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_preview.thumbnail = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 168.f, 142.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_preview.title = entity;


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

    entity = m_scene.getActiveCamera();
    entity.getComponent<cro::Camera>().resizeCallback = updateView;
    updateView(entity.getComponent<cro::Camera>());
}

void EditTournamentState::loadCourseInfo()
{
    const auto installPaths = Content::getInstallPaths();
    for (const auto& path : installPaths)
    {
        const auto coursePath = path + "courses/";
        if (cro::FileSystem::directoryExists(coursePath))
        {
            auto courseDirs = cro::FileSystem::listDirectories(coursePath);
            //this might necessarily be in alphabetical order
            std::sort(courseDirs.begin(), courseDirs.end());
            courseDirs.erase(std::remove_if(courseDirs.begin(), courseDirs.end(), 
                [](const std::string& s)
                {
                    return s.find("course_") == std::string::npos;
                }), courseDirs.end());

            for (const auto& dir : courseDirs)
            {
                const auto dataPath = coursePath + dir + "/course.data";
                if (cro::FileSystem::fileExists(dataPath))
                {
                    cro::ConfigFile cfg;
                    if (cfg.loadFromFile(dataPath))
                    {
                        if (const auto* t = cfg.findProperty("title"); t != nullptr)
                        {
                            auto& inf = m_courseInfo.emplace_back();
                            inf.dir = dir;
                            inf.displayName = t->getValue<cro::String>();
                            inf.texture = &m_textures.get(coursePath + dir + "/preview.png");
                        }
                    }
                }
            }
        }
    }

    if (!m_courseInfo.empty())
    {
        updatePreview(0);
    }
}

void EditTournamentState::updatePreview(std::size_t index)
{
    const auto scale = CourseThumbnailSize / glm::vec2(m_courseInfo[index].texture->getSize());
    m_preview.thumbnail.getComponent<cro::Transform>().setScale(scale);
    m_preview.thumbnail.getComponent<cro::Sprite>().setTexture(*m_courseInfo[index].texture);

    m_preview.title.getComponent<cro::Text>().setString(m_courseInfo[index].displayName);
}

void EditTournamentState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

    WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Choosing a Tournament" }));
}

void EditTournamentState::onCachedPush()
{
    m_rootNode.getComponent<cro::Callback>().active = true;

    //check if there's an existing tournament in the shared
    //data and load it ready for editing
    if (!m_sharedData.tournamentPath.empty())
    {
        m_tournamentInfo.load(m_sharedData.tournamentPath, m_sharedData.courseData);
        
        //map the courses to their indices
        for (auto i = 0; i < 4; ++i)
        {
            const auto result = std::find_if(m_courseInfo.cbegin(), m_courseInfo.cend(),
                [&](const CourseInfo& c) {return c.dir == m_tournamentInfo.getCourse(i); });

            if (result != m_courseInfo.cend())
            {
                m_tierIndices[i] = std::distance(m_courseInfo.cbegin(), result);
            }
        }
    }
    else
    {
        //make sure to clear out existing data
        m_tournamentInfo = {};
        std::fill(m_tierIndices.begin(), m_tierIndices.end(), 0);
    }

    m_tournamentNameEntity.getComponent<cro::Text>().setString(m_tournamentInfo.getTitle());
    updatePreview(m_tierIndices[0]);
    m_scene.getSystem<cro::UISystem>()->selectByIndex(ButtonID::T1Down);

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

    WebSock::broadcastPacket(Social::setStatus(Social::InfoID::Menu, { "Customising a Tournament" }));
}