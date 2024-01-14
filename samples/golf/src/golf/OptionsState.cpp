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

#include "OptionsState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "MessageIDs.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>
#include <Input.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/InfoFlags.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/util/Rectangle.hpp>
#include <crogine/util/String.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <sstream>
#include <iomanip>

#include "OptionsEnum.inl"

using namespace cl;

namespace
{
    constexpr float CameraDepth = 3.f;

    constexpr float BackgroundDepth = -0.5f;
    constexpr float OptionsDepth = -0.4f;
    constexpr float TabWindowDepth = 0.1f;
    constexpr float TabBarDepth = 0.15f;

    constexpr glm::vec3 PanelPosition(4.f, 52.f, TabWindowDepth); //y is 20 if we increase panel by 66
    constexpr glm::vec3 HiddenPosition(-10000.f);

    constexpr float HighlightOffset = 0.25f;
    constexpr float TextOffset = 0.15f;

    constexpr float ToolTipDepth = CameraDepth - 0.8f;

    constexpr glm::vec2 BackgroundSize(192.f, 108.f);
    const cro::Colour BackgroundColour = cro::Colour(std::uint8_t(26), 30, 45);

    struct MenuID final
    {
        enum
        {
            Video, Controls, Dummy, Achievements, Stats
        };
    };

    const std::array<cro::String, MixerChannel::Count> MixerLabels =
    {
        "Music Volume",
        "Effects Volume",
        "Menu Volume",
        "Voice Volume",
        "Vehicle Volume",
        "Environment",
        "User Music"
    };
    //generally static vars would be a bad idea, but in this case
    //a static index value will remember the last channel between
    //showing instances of options, as well as being available to
    //the slider callbacks :3
    std::uint8_t mixerChannelIndex = MixerChannel::Music;

    static constexpr float SliderWidth = 142.f;
    static constexpr glm::vec3 ToolTipOffset(10.f, 10.f, 0.f);

    cro::Entity resolutionLabel; //nasty static hack to update label on window FS toggle

    const std::array<cro::String, 4u> AAStrings =
    {
        "None",
        "2x MSAA",
        "4x MSAA",
        "8x MSAA"
    };
    constexpr std::array<std::size_t, 9u> AAIndexMap =
    {
        0,
        0,
        1,
        0,
        2,
        0,
        0,
        0,
        3
    };
    constexpr std::array<std::uint32_t, 4u> AASamples =
    {
        0,2,4,8
    };

    //this needs to be here to be accessed by label callback
    std::array<cro::Entity, InputBinding::Count> bindingEnts = {}; //buttons to activate key rebinding

    struct SliderData final
    {
        const glm::vec2 basePosition = glm::vec2(0.f);
        const float width = SliderWidth;
        std::function<void(float)> onActivate;

        explicit SliderData(glm::vec2 p, float w = SliderWidth)
            : basePosition(p), width(w) {}
    };

    struct SliderCallback final
    {
        void operator() (cro::Entity e, float)
        {
            const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
            float vol = cro::AudioMixer::getVolume(mixerChannelIndex);
            e.getComponent<cro::Transform>().setPosition({ pos.x + (width * vol), pos.y });
        }
    };

    struct SliderDownCallback final
    {
        explicit SliderDownCallback(cro::Entity audio)
            : audioEnt(audio) { }

        void operator() (cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                float vol = cro::AudioMixer::getVolume(mixerChannelIndex);
                vol = std::max(0.f, vol - 0.1f);
                cro::AudioMixer::setVolume(vol, mixerChannelIndex);

                audioEnt.getComponent<cro::AudioEmitter>().play();
            }
        }

        cro::Entity audioEnt;
    };

    struct SliderUpCallback final
    {
        explicit SliderUpCallback(cro::Entity audio)
            : audioEnt(audio) { }

        void operator() (cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                float vol = cro::AudioMixer::getVolume(mixerChannelIndex);
                vol = std::min(1.f, vol + 0.1f);
                cro::AudioMixer::setVolume(vol, mixerChannelIndex);
                audioEnt.getComponent<cro::AudioEmitter>().play();
            }
        }
        cro::Entity audioEnt;
    };

    struct ScrollCallback final
    {
        void operator() (cro::Entity e, float dt)
        {
            float target = e.getComponent<cro::Callback>().getUserData<float>();

            auto rect = e.getComponent<cro::Sprite>().getTextureRect();
            auto move = target - rect.bottom;

            
            if (std::abs(move) > 0.1f)
            {
                rect.bottom += move * 15.f * dt;
            }
            else
            {
                rect.bottom = target;
                e.getComponent<cro::Callback>().active = false;
            }
            e.getComponent<cro::Sprite>().setTextureRect(rect);
        }
    };

    struct ToolTip final
    {
        cro::Entity target;
    };

    inline cro::String keyString(std::int32_t idx, const SharedStateData& sharedData)
    {
        return " (" + cro::Keyboard::keyString(sharedData.inputBinding.keys[idx]) + ")";
    };
}

OptionsState::OptionsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 256/*, cro::INFO_FLAG_SYSTEMS_ACTIVE | cro::INFO_FLAG_SYSTEM_TIME*/),
    m_sharedData        (sd),
    m_updatingKeybind   (false),
    m_lastMousePos      (0.f),
    m_bindingIndex      (-1),
    m_currentTabFunction(0),
    m_activeToolTip     (-1),
    m_viewScale         (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    m_videoSettings.fullScreen = ctx.mainWindow.isFullscreen();
    auto size = ctx.mainWindow.getSize();
    for (auto i = 0u; i < sd.resolutions.size(); ++i)
    {
        if (sd.resolutions[i].x == size.x && sd.resolutions[i].y == size.y)
        {
            m_videoSettings.resolutionIndex = i;
            break;
        }
    }

    buildScene();

    cacheState(StateID::Credits);
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


    if (!m_updatingKeybind
        && !m_activeSlider.isValid())
    {
        m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    }


    const auto toggleControllerIcon = [&](std::int32_t controllerID)
    {
        if (Social::isSteamdeck())
        {
            return;
        }

#ifdef CRO_DEBUG_
        static bool temp = false;
        LogI << "This is debug mode, it's supposed to happen" << std::endl;
        temp = !temp;
        if (temp)
#else
        if (IS_PS(controllerID))
#endif
        {
            m_psController.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            m_xboxController.getComponent<cro::Transform>().setScale(glm::vec2(0.f));

            //overlay is used for extra buttons in golf mode too
            m_psOverlay.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            m_xboxOverlay.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        else
        {
            m_psController.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            m_xboxController.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

            m_psOverlay.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            m_xboxOverlay.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        m_scene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces refresh
    };

    const auto closeWindow = [&]()
    {
        if (!m_updatingKeybind)
        {
            quitState();
        }
        else
        {
            //cancel the input
            updateKeybind(evt.key.keysym.sym);
        }
    };

    const auto scrollMenu = [&](std::int32_t direction)
    {
        std::int32_t callbackID = 0;
        auto menuID = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
        if (menuID == MenuID::Achievements)
        {
            callbackID = direction == 0 ? ScrollID::AchUp : ScrollID::AchDown;
        }
        else if (menuID == MenuID::Stats)
        {
            callbackID = direction == 0 ? ScrollID::StatUp : ScrollID::StatDown;
        }

        cro::ButtonEvent fakeEvent;
        fakeEvent.type = SDL_MOUSEBUTTONDOWN;
        fakeEvent.button.button = SDL_BUTTON_LEFT;
        m_scrollFunctions[callbackID](cro::Entity(), fakeEvent);
    };

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default:
            if (m_updatingKeybind)
            {
                //apply keybind
                updateKeybind(evt.key.keysym.sym);
            }
            break;
#ifdef CRO_DEBUG_
        case SDLK_KP_DIVIDE:
            toggleControllerIcon(0);
            break;
#endif
        case SDLK_RETURN:
        case SDLK_RETURN2:
        case SDLK_KP_ENTER:
        case SDLK_KP_PLUS:
        case SDLK_KP_MINUS:
        case SDLK_F1:
        case SDLK_F5:
        case SDLK_TAB:
            //handle these cases just so
            //they can't be applied to keybinds
            //TODO need to print some sort of user feedback
            //letting them know why this doesen't work
            break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            closeWindow();
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
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonB:
            closeWindow();
            return false;
        case cro::GameController::ButtonLeftShoulder:
        case cro::GameController::ButtonRightShoulder:
        {
            auto ent = m_scene.createEntity();
            ent.addComponent<cro::Callback>().active = true;
            ent.getComponent<cro::Callback>().function =
                [&](cro::Entity e, float)
            {
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                m_scene.destroyEntity(e);
            };
        }
            break;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        cro::App::getWindow().setMouseCaptured(true);
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonLeftShoulder:
            if (!m_updatingKeybind)
            {
                m_currentTabFunction = (m_currentTabFunction + (m_tabFunctions.size() - 1)) % m_tabFunctions.size();
                m_tabFunctions[m_currentTabFunction]();
            }
            break;
        case cro::GameController::ButtonRightShoulder:
            if (!m_updatingKeybind)
            {
                m_currentTabFunction = (m_currentTabFunction + 1) % m_tabFunctions.size();
                m_tabFunctions[m_currentTabFunction]();
            }
            break;
        }

        toggleControllerIcon(cro::GameController::controllerID(evt.cbutton.which));
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        if (std::abs(evt.caxis.value) > LeftThumbDeadZone)
        {
            toggleControllerIcon(cro::GameController::controllerID(evt.caxis.which));

            cro::App::getWindow().setMouseCaptured(true);

            if (evt.caxis.axis == cro::GameController::AxisRightY)
            {
                /*if (evt.caxis.value > 0)
                {
                    scrollMenu(1);
                }
                else
                {
                    scrollMenu(0);
                }*/
            }
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT
            && !m_updatingKeybind)
        {
            if (m_activeToolTip == -1)
            {
                pickSlider();
            }
            else
            {
                switch (m_activeToolTip)
                {
                default: break;
                case ToolTipID::CustomMusic:
#ifdef USE_GNS
                    if (!Social::isSteamdeck())
                    {
                        Social::showGuides();
                    }
#else
                    cro::Util::String::parseURL("https://steamcommunity.com/sharedfiles/filedetails/?id=3013809801");
#endif
                    break;
                }
            }
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            m_activeSlider = {};
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            closeWindow();
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        updateSlider();
        cro::App::getWindow().setMouseCaptured(false);
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            //up
            scrollMenu(0);
        }
        else if (evt.wheel.y < 0)
        {
            //down
            scrollMenu(1);
        }
    }

    m_scene.forwardEvent(evt);
    return false;
}

void OptionsState::handleMessage(const cro::Message& msg)
{
    if (msg.id == MessageID::SystemMessage)
    {
        const auto& data = msg.getData<SystemEvent>();
        if (data.type == SystemEvent::PostProcessToggled)
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        }
    }
    else if (msg.id == cro::Message::SystemMessage)
    {
        const auto& data = msg.getData<cro::Message::SystemEvent>();
        if (data.type == cro::Message::SystemEvent::FullScreenToggled)
        {
            auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
            cam.active = true;
            cam.resizeCallback(cam);

            m_videoSettings.fullScreen = cro::App::getWindow().isFullscreen();

            auto currentRes = cro::App::getWindow().getSize();
            if (auto res = std::find_if(m_sharedData.resolutions.cbegin(), m_sharedData.resolutions.cend(),
                [currentRes](const glm::uvec2& r)
                {
                    return r == currentRes;
                });
                res != m_sharedData.resolutions.cend())
            {
                m_videoSettings.resolutionIndex = std::distance(m_sharedData.resolutions.cbegin(), res);

                resolutionLabel.getComponent<cro::Text>().setString(m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
                centreText(resolutionLabel);
            }
        }
    }
    m_scene.forwardMessage(msg);
}

bool OptionsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void OptionsState::render()
{
    m_scene.render();
}

//private
void OptionsState::pickSlider()
{
    auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());

    for (auto& entity : m_sliders)
    {
        auto bounds = entity.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = entity.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            m_activeSlider = entity;
            m_lastMousePos = mousePos.x;

            break;
        }
    }
}

void OptionsState::updateSlider()
{
    if (m_activeSlider.isValid())
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        float movement = (mousePos.x - m_lastMousePos) / m_viewScale.x;

        const auto& [pos, width, onActivate] = m_activeSlider.getComponent<cro::Callback>().getUserData<SliderData>();
        float maxMove = pos.x + width;
        float minMove = pos.x;

        auto currentPos = m_activeSlider.getComponent<cro::Transform>().getPosition();
        float finalPos = std::min(maxMove, std::max(minMove, currentPos.x + movement));

        m_activeSlider.getComponent<cro::Transform>().setPosition({ finalPos, currentPos.y });

        onActivate((finalPos - pos.x) / width);

        m_lastMousePos = mousePos.x;
    }
}

void OptionsState::updateKeybind(SDL_Keycode key)
{
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

    auto& keys = m_sharedData.inputBinding.keys;
    if (auto result = std::find(keys.begin(), keys.end(), key); result != keys.end())
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::PlayerConfig;
        cmd.action = [key](cro::Entity e, float)
        {
            //briefly shows error message before returning to old string
            auto oldStr = e.getComponent<cro::Text>().getString();
            e.getComponent<cro::Callback>().setUserData<std::pair<float, cro::String>>(2.f, oldStr);
            e.getComponent<cro::Callback>().active = true;

            auto msg = cro::Keyboard::keyString(key);
            msg += " is already bound";
            e.getComponent<cro::Text>().setString(msg);
            centreText(e);
        };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        return;
    }


    m_updatingKeybind = false;

    //these keys cancel the input
    if (key != SDLK_ESCAPE
        && key != SDLK_BACKSPACE)
    {
        keys[m_bindingIndex] = key;
    }

    auto index = m_bindingIndex;
    m_bindingIndex = -1;

    //actually update the info text
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::PlayerConfig;
    cmd.action = [&,index, key](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_labelStrings[index] + " (" + cro::Keyboard::keyString(key) + ")");
        centreText(e);

        //reset any existing callback so that it doesn't timeout
        //and set the wrong string
        e.getComponent<cro::Callback>().active = false;
    };
    m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void OptionsState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();

    m_scene.addSystem<cro::UISystem>(mb)->setMouseScrollNavigationEnabled(false);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

    m_scene.setSystemActive<cro::AudioPlayerSystem>(false);


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

                m_scene.setSystemActive<cro::AudioPlayerSystem>(true);
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Video);
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                requestStackPop();

                m_currentTabFunction = 0;
                m_tabFunctions[0]();

                state = RootCallbackData::FadeIn;
                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
            }
            break;
        }

    };

    m_rootNode = rootNode;


    //quad to darken the screen
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, BackgroundDepth });
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

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options.spt", m_sharedData.sharedResources->textures);

    //options background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, OptionsDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;
    auto bgSize = glm::vec2(bounds.width, bounds.height);

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::White); e.getComponent<cro::AudioEmitter>().play(); });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    //video options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(PanelPosition);
    entity.getComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("audio_video");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto videoEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Video, selectedID, unselectedID, spriteSheet);
    videoEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto videoButtonEnt = entity;

    auto hideVideo = [videoEnt]() mutable
    {
        videoEnt.getComponent<cro::Transform>().setPosition(HiddenPosition);
    };
    auto showVideo = [videoEnt]() mutable
    {
        videoEnt.getComponent<cro::Transform>().setPosition(PanelPosition);
    };

    //control options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(HiddenPosition);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("input");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto controlEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Controls, selectedID, unselectedID, spriteSheet);
    controlEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto controlButtonEnt = entity;

    //controls tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 0.f, bgSize.y - bounds.height, TabBarDepth });
    controlButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto hideControls = [controlEnt]() mutable
    {
        controlEnt.getComponent<cro::Transform>().setPosition(HiddenPosition);
    };
    auto showControls = [controlEnt]() mutable
    {
        controlEnt.getComponent<cro::Transform>().setPosition(PanelPosition);
    };

    //achievements
    bounds = spriteSheet.getSprite("input").getTextureBounds();
    const glm::uvec2 bufferSize(bounds.width, bounds.height);

    m_achievementBuffer.create(bufferSize.x, bufferSize.y, false);
    m_achievementBuffer.clear(cro::Colour::Green);
    m_achievementBuffer.display();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(HiddenPosition);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_achievementBuffer.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto achEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Achievements, selectedID, unselectedID, spriteSheet);
    achEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto achButtonEnt = entity;

    //achievement tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("ach_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 0.f, bgSize.y - bounds.height, TabBarDepth });
    achButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto hideAchievements = [achEnt]() mutable
    {
        achEnt.getComponent<cro::Transform>().setPosition(HiddenPosition);
    };
    auto showAchievements = [achEnt]() mutable
    {
        achEnt.getComponent<cro::Transform>().setPosition(PanelPosition);
    };

    //stats
    m_statsBuffer.create(bufferSize.x, bufferSize.y, false);
    m_statsBuffer.clear(cro::Colour::Red);
    m_statsBuffer.display();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(HiddenPosition);
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_statsBuffer.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto statsEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Stats, selectedID, unselectedID, spriteSheet);
    statsEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto statsButtonEnt = entity;

    //stats tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("stat_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 0.f, bgSize.y - bounds.height, TabBarDepth });
    statsButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto hideStats = [statsEnt]() mutable
    {
        statsEnt.getComponent<cro::Transform>().setPosition(HiddenPosition);
    };
    auto showStats = [statsEnt]() mutable
    {
        statsEnt.getComponent<cro::Transform>().setPosition(PanelPosition);
    };



    auto spriteSelectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto spriteUnselectedID = uiSystem.addCallback([](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    //delays reactivating the camera for one frame.
    const auto refreshView =
        [&]() 
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            m_scene.destroyEntity(e);
        };
    };

    //tab callback functions
    m_tabFunctions[0] = [&, showVideo, hideControls, hideAchievements, hideStats, refreshView]() mutable
    {
        //hide other tabs
        hideControls();
        hideAchievements();
        hideStats();

        //show video tab
        showVideo();
        uiSystem.setActiveGroup(MenuID::Video);
        uiSystem.selectByIndex(TabController);

        //refresh visible objects for one frame
        refreshView();

        m_currentTabFunction = 0;
    };

    m_tabFunctions[1] = [&, hideVideo, showControls, hideAchievements, hideStats, refreshView]() mutable
    {
        //hide other tabs
        hideVideo();
        hideAchievements();
        hideStats();

        //reset position for control button ent
        showControls();

        refreshView();

        uiSystem.setActiveGroup(MenuID::Controls);
        uiSystem.selectByIndex(TabAchievements);

        m_currentTabFunction = 1;
    };

    m_tabFunctions[2] = [&, hideVideo, hideControls, showAchievements, hideStats, refreshView]() mutable
    {
        //hide other tabs
        hideVideo();
        hideControls();
        hideStats();

        //show achievment tab
        showAchievements();

        refreshView();

        uiSystem.setActiveGroup(MenuID::Achievements);
        uiSystem.selectByIndex(TabStats);

        m_currentTabFunction = 2;
    };

    m_tabFunctions[3] = [&, hideVideo, hideControls, hideAchievements, showStats, refreshView]() mutable
    {
        //hide other tabs
        hideVideo();
        hideControls();
        hideAchievements();

        //show stats tab
        showStats();

        refreshView();

        uiSystem.setActiveGroup(MenuID::Stats);
        uiSystem.selectByIndex(TabAV);

        m_currentTabFunction = 3;
    };


    const std::array<glm::vec3, 4u> TabPositions =
    {
        glm::vec3(1.f, 237.f, TabBarDepth + HighlightOffset),
        glm::vec3(101.f, 237.f, TabBarDepth + HighlightOffset),
        glm::vec3(201.f, 237.f, TabBarDepth + HighlightOffset),
        glm::vec3(301.f, 237.f, TabBarDepth + HighlightOffset)
    };

    auto createTab = [&, spriteSelectedID, spriteUnselectedID](cro::Entity parent, std::size_t index, std::int32_t menuID, std::size_t selectionIndex)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(TabPositions[index]);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
        ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        ent.addComponent<cro::UIInput>().area = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::UIInput>().setGroup(menuID);
        ent.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = spriteSelectedID;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = spriteUnselectedID;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
            [&, index](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_tabFunctions[index]();
                }
            });

        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };
    entity = createTab(videoButtonEnt, 1, MenuID::Video, TabController);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, AVMixerRight);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowCredits);
    entity = createTab(videoButtonEnt, 2, MenuID::Video, TabAchievements);
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, AVVolumeDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowApply);
    entity = createTab(videoButtonEnt, 3, MenuID::Video, TabStats);
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, AVVolumeUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowClose);

    entity = createTab(controlButtonEnt, 0, MenuID::Controls, TabAV);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, CtrlThreshL);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowAdvanced);
    entity = createTab(controlButtonEnt, 2, MenuID::Controls, TabAchievements);
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, CtrlRB);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, CtrlDown);
    entity = createTab(controlButtonEnt, 3, MenuID::Controls, TabStats);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, CtrlLB);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, WindowClose);

    entity = createTab(achButtonEnt, 0, MenuID::Achievements, TabAV);
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowAdvanced);
    entity = createTab(achButtonEnt, 1, MenuID::Achievements, TabController);
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, WindowCredits);
    entity = createTab(achButtonEnt, 3, MenuID::Achievements, TabStats);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, ScrollUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowClose);

    entity = createTab(statsButtonEnt, 0, MenuID::Stats, TabAV);
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, WindowAdvanced);
    entity = createTab(statsButtonEnt, 1, MenuID::Stats, TabController);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, WindowCredits);
    entity = createTab(statsButtonEnt, 2, MenuID::Stats, TabAchievements);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, ScrollUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowApply);

    
    auto createTabTip = [&](std::int32_t tipID, glm::vec3 position)
    {
        auto bounds = spriteSheet.getSprite("tab_highlight").getTextureBounds();

        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(position);
        ent.addComponent<cro::Callback>().active = true;
        ent.getComponent<cro::Callback>().function =
            [&, bounds, tipID](cro::Entity e, float)
        {
            auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
            auto worldBounds = e.getComponent<cro::Transform>().getWorldTransform() * bounds;
            if (worldBounds.contains(mousePos))
            {
                mousePos.x = std::floor(mousePos.x);
                mousePos.y = std::floor(mousePos.y);
                mousePos.z = 2.f;

                //need to refresh the view if the tip was previously hidden
                if (m_tooltips[tipID].getComponent<cro::Transform>().getScale().x == 0)
                {
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
                
                m_tooltips[tipID].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
                m_tooltips[tipID].getComponent<cro::Transform>().setScale(m_viewScale);
            }
            else
            {
                m_tooltips[tipID].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
        return ent;
    };

    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Video, TabPositions[0]).getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Controls, TabPositions[1]).getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Achievements, TabPositions[2]).getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Stats, TabPositions[3]).getComponent<cro::Transform>());

    buildAVMenu(videoEnt, spriteSheet);
    buildControlMenu(controlEnt, spriteSheet);
    buildAchievementsMenu(achEnt, spriteSheet);
    buildStatsMenu(statsEnt, spriteSheet);

    //dummy input to consume events during animation
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);


    //tool tips for options
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto createToolTip = [&](const cro::String& tip)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({-100.f, -100.f, ToolTipDepth});
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<ToolTip>();
        entity.addComponent<cro::Text>(font).setString(tip);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        
        static constexpr float Padding = 10.f;
        auto bounds = cro::Text::getLocalBounds(entity);
        bounds.width += Padding;
        bounds.height += Padding;
        bounds.left -= (Padding / 2.f);
        bounds.bottom -= (Padding / 2.f);

        auto colour = cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha);
        auto bgEnt = m_scene.createEntity();
        bgEnt.addComponent<cro::Transform>().setPosition({0.f, 0.f, -TextOffset});
        bgEnt.addComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(bounds.left, bounds.bottom + bounds.height), colour),
                cro::Vertex2D(glm::vec2(bounds.left, bounds.bottom), colour),
                cro::Vertex2D(glm::vec2(bounds.left + bounds.width, bounds.bottom + bounds.height), colour),
                cro::Vertex2D(glm::vec2(bounds.left + bounds.width, bounds.bottom), colour)
            }
        );
        entity.getComponent<cro::Transform>().addChild(bgEnt.getComponent<cro::Transform>());

        return entity;
    };
    m_tooltips[ToolTipID::Volume] = createToolTip("Vol: 100");
    m_tooltips[ToolTipID::AA] = createToolTip("Automatically disabled when\nusing pixel scaling");
    m_tooltips[ToolTipID::FOV] = createToolTip("FOV: 60");
    m_tooltips[ToolTipID::Pixel] = createToolTip("Scale up pixels to match\nthe current resolution.");
    m_tooltips[ToolTipID::VertSnap] = createToolTip("Snaps vertices to the nearest\nwhole pixel for a retro \'wobble\'.\nMay cause z-fighting.");
    m_tooltips[ToolTipID::Beacon] = createToolTip("Shows a beacon to indicate flag position\nat far distances.");
    m_tooltips[ToolTipID::BeaconColour] = createToolTip("Display colour of the beacon.");
    m_tooltips[ToolTipID::Units] = createToolTip("Select to display in yards/feet or\nunselect to display in metres/cm");
    m_tooltips[ToolTipID::MouseSpeed] = createToolTip("1.00");
    m_tooltips[ToolTipID::PuttingPower] = createToolTip("Decreases the difficulty when\nputting, at the cost of XP");
    m_tooltips[ToolTipID::Video] = createToolTip("Sound & Video Settings");
    m_tooltips[ToolTipID::Controls] = createToolTip("Controls");
    m_tooltips[ToolTipID::Achievements] = createToolTip("Achievements");
    m_tooltips[ToolTipID::Stats] = createToolTip("Stats");
    m_tooltips[ToolTipID::NeedsRestart] = createToolTip("Applied On Next Game Load");
    m_tooltips[ToolTipID::CustomMusic] = createToolTip("Visit the Steam Community for\nguides on adding custom music");

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);
    };

    entity = m_scene.getActiveCamera();
    entity.getComponent<cro::Transform>().setPosition({ 0.f, 0.f, CameraDepth });
    entity.getComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());


    //we need to delay setting the active group until all the
    //new entities are processed, so we kludge this with a callback
    entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Controls);
        //m_scene.getActiveCamera().getComponent<cro::Camera>().isStatic = true;
        e.getComponent<cro::Callback>().active = false;
        m_scene.destroyEntity(e);
    };

    //then this refreshes the UI at least once
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.5f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;

        if(currTime < 0)
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().isStatic = true;
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };


    //and default to dummy input until loaded
    //m_scene.simulate(0.f);
    //m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
}

void OptionsState::buildAVMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto bgBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    const auto& titleFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 174.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(titleFont).setString("Audio & Video");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());

    auto createLabel = [&](glm::vec2 pos, const std::string& str)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, TextOffset));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setString(str);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

    //audio label
    auto audioLabel = createLabel(glm::vec2((bgBounds.width / 2.f) - 101.f, 139.f), "Music Volume");
    centreText(audioLabel);
    audioLabel.addComponent<cro::Callback>().active = true;
    audioLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::CustomMusic);
    };

    //antialiasing label
    auto aliasLabel = createLabel(glm::vec2(12.f, 114.f), "Antialiasing");
    aliasLabel.addComponent<cro::Callback>().active = true;
    aliasLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::AA);
    };

    auto aaLabel = createLabel(glm::vec2(136.f, 114.f), AAStrings[AAIndexMap[m_sharedData.multisamples]]);
    centreText(aaLabel);

    //FOV label
    auto fovLabel = createLabel(glm::vec2(12.f, 98.f), "FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));

    //resolution label
    auto resLabel = createLabel(glm::vec2(12.f, 82.f), "Resolution");
    //centreText(resLabel);

    //resolution value text
    resLabel = createLabel(glm::vec2(136.f, 82.f), m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
    centreText(resLabel);
    resolutionLabel = resLabel; //globa static used by callback to update display when window is toggled FS

    //pixel scale label
    auto pixelLabel = createLabel(glm::vec2(12.f, 66.f), "Pixel Scaling");
    pixelLabel.addComponent<cro::Callback>().active = true;
    pixelLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::Pixel);
    };

    //vertex snap label
    auto vertLabel = createLabel(glm::vec2(12.f, 50.f), "Vertex Snap      (requires restart)");
    vertLabel.addComponent<cro::Callback>().active = true;
    vertLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::VertSnap);
    };

    //full screen label
    createLabel(glm::vec2(12.f, 34.f), "Full Screen");

    //beacon label
    auto beaconLabel = createLabel(glm::vec2(12.f, 18.f), "Flag Beacon");
    beaconLabel.addComponent<cro::Callback>().active = true;
    beaconLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::Beacon);
    };

    //ball trail label
    createLabel({ 204.f, 114.f }, "Enable       Ball Trail");
    
    //putting assist
    auto puttingEnt = createLabel({ 204.f, 98.f }, "Enable       Putting Assist");
    puttingEnt.addComponent<cro::Callback>().active = true;
    puttingEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::PuttingPower);
    };


    //post process label
    createLabel({ 204.f, 82.f }, "Post FX");

    //measurements
    auto measureLabel = createLabel({ 204.f, 66.f }, "Units         Imperial Measurements");
    measureLabel.addComponent<cro::Callback>().active = true;
    measureLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::Units);
    };

    //grid transparency
    createLabel({ 204.f, 50.f }, "Grid Amount");

    //tree quality
    auto treeLabel = createLabel({ 204.f, 34.f }, "Tree Quality");
    treeLabel.addComponent<cro::Callback>().active = true;
    treeLabel.getComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::NeedsRestart);
    };


    //shadow quality
    createLabel({ 204.f, 18.f }, "Shadow Quality");
 



    auto createSlider = [&](glm::vec2 position)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slider");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), /*std::floor*/(bounds.height / 2.f), -TextOffset });


        auto userData = SliderData(position);
        userData.onActivate = [](float distance)
        {
            float vol = distance;
            cro::AudioMixer::setVolume(vol, mixerChannelIndex);
        };
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<SliderData>(userData);
        entity.getComponent<cro::Callback>().function = SliderCallback();

        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        m_sliders.push_back(entity);
        return entity;
    };

    //volume slider
    auto volSlider = createSlider(glm::vec2(192.f, 136.f));
    auto tipEnt = m_scene.createEntity();
    tipEnt.addComponent<cro::Transform>();
    tipEnt.addComponent<cro::Callback>().active = true;
    tipEnt.getComponent<cro::Callback>().function =
        [&, volSlider](cro::Entity e, float)
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        auto bounds = volSlider.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = volSlider.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            mousePos.x = std::floor(mousePos.x);
            mousePos.y = std::floor(mousePos.y);
            mousePos.z = ToolTipDepth / 2.f;
            
            if (m_tooltips[ToolTipID::Volume].getComponent<cro::Transform>().getScale().x == 0)
            {
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }

            m_tooltips[ToolTipID::Volume].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
            m_tooltips[ToolTipID::Volume].getComponent<cro::Transform>().setScale(m_viewScale);

            float vol = cro::AudioMixer::getVolume(mixerChannelIndex);
            m_tooltips[ToolTipID::Volume].getComponent<cro::Text>().setString("Vol: " + std::to_string(static_cast<std::int32_t>(vol * 100.f)));
        }
        else
        {
            m_tooltips[ToolTipID::Volume].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };
    volSlider.getComponent<cro::Transform>().addChild(tipEnt.getComponent<cro::Transform>());



    //fov slider
    auto fovPos = glm::vec2(99.f, 95.f);
    auto fovSlider = createSlider(fovPos);
    auto userData = SliderData(fovPos, 76.f);
    userData.onActivate = 
        [&, fovLabel](float distance) mutable
    {
        float fov = MinFOV + ((MaxFOV - MinFOV) * distance);

        m_sharedData.fov = fov;

        //raise a window resize message to trigger callbacks
        auto size = cro::App::getWindow().getSize();
        auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
        msg->data0 = size.x;
        msg->data1 = size.y;
        msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;

        fovLabel.getComponent<cro::Text>().setString("FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));
    };

    fovSlider.getComponent<cro::Callback>().setUserData<SliderData>(userData);
    fovSlider.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
        float amount = (m_sharedData.fov - MinFOV) / (MaxFOV - MinFOV);

        e.getComponent<cro::Transform>().setPosition({ pos.x + (width * amount), pos.y });
    };

    tipEnt = m_scene.createEntity();
    tipEnt.addComponent<cro::Transform>();
    tipEnt.addComponent<cro::Callback>().active = true;
    tipEnt.getComponent<cro::Callback>().function =
        [&, fovSlider](cro::Entity, float)
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        auto bounds = fovSlider.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = fovSlider.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            mousePos.x = std::floor(mousePos.x);
            mousePos.y = std::floor(mousePos.y);
            mousePos.z = ToolTipDepth;

            if (m_tooltips[ToolTipID::FOV].getComponent<cro::Transform>().getScale().x == 0)
            {
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }

            m_tooltips[ToolTipID::FOV].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
            m_tooltips[ToolTipID::FOV].getComponent<cro::Transform>().setScale(m_viewScale);

            float fov = m_sharedData.fov;
            m_tooltips[ToolTipID::FOV].getComponent<cro::Text>().setString("FOV: " + std::to_string(static_cast<std::int32_t>(fov)));
        }
        else
        {
            m_tooltips[ToolTipID::FOV].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };
    fovSlider.getComponent<cro::Transform>().addChild(tipEnt.getComponent<cro::Transform>());

    //grid transparency
    auto transPos = glm::vec2(280.f, 47.f);
    auto transSlider = createSlider(transPos);
    auto ud = SliderData(transPos, 91.f);
    ud.onActivate =
        [&](float distance)
    {
        m_sharedData.gridTransparency = distance;
    };
    transSlider.getComponent<cro::Callback>().setUserData<SliderData>(ud);
    transSlider.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
        e.getComponent<cro::Transform>().setPosition({ pos.x + (width * m_sharedData.gridTransparency), pos.y });
    };

    auto helpEnt = m_scene.createEntity();
    helpEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, -4.f, 0.1f });
    helpEnt.addComponent<cro::Drawable2D>();
    helpEnt.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    helpEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    helpEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(helpEnt.getComponent<cro::Transform>());


    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([&,helpEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            helpEnt.getComponent<cro::Text>().setString(e.getLabel());
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });
    auto unselectedID = uiSystem.addCallback([helpEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); 

            helpEnt.getComponent<cro::Text>().setString(" ");
        });

    auto createHighlight = [&](glm::vec2 pos)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(pos);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
        ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        ent.addComponent<cro::UIInput>().setGroup(MenuID::Video);
        auto bounds = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::UIInput>().area = bounds;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        ent.addComponent<cro::Callback>().function = HighlightAnimationCallback();
        ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -HighlightOffset });
        ent.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };

    //channel select down
    auto entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 156.f, 130.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVMixerLeft);
    entity.getComponent<cro::UIInput>().setNextIndex(AVMixerRight, AVAAL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVolumeUp, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&,audioLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                mixerChannelIndex = (mixerChannelIndex + (MixerChannel::Count - 1)) % MixerChannel::Count;
                audioLabel.getComponent<cro::Text>().setString(MixerLabels[mixerChannelIndex]);
                centreText(audioLabel);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //channel select up
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 55.f, 130.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVMixerRight);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVolumeDown, AVAAR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVMixerLeft, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&,audioLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                mixerChannelIndex = (mixerChannelIndex + 1) % MixerChannel::Count;
                audioLabel.getComponent<cro::Text>().setString(MixerLabels[mixerChannelIndex]);
                centreText(audioLabel);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });


    //audio down
    entity = createHighlight(glm::vec2(174.f, 130.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVolumeDown);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVolumeUp, AVAAR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVMixerRight, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderDownCallback(m_audioEnts[AudioID::Accept]));

    //audio up
    entity = createHighlight(glm::vec2(341.f, 130.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVolumeUp);
    entity.getComponent<cro::UIInput>().setNextIndex(AVMixerLeft, AVTrailL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVolumeDown, TabStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderUpCallback(m_audioEnts[AudioID::Back]));

    //aa down
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 105.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVAAL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVAAR, AVFOVL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTrailR, AVMixerLeft);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, aaLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    auto currIndex = AAIndexMap[m_sharedData.multisamples];
                    currIndex = (currIndex + (AASamples.size() - 1)) % AASamples.size();
                    //m_sharedData.multisamples = AASamples[currIndex];
                    
                    aaLabel.getComponent<cro::Text>().setString(AAStrings[currIndex]);
                    centreText(aaLabel);

                    toggleAntialiasing(m_sharedData, AASamples[currIndex] != 0, AASamples[currIndex]);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //aa up
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 105.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVAAR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTrail, AVFOVR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVAAL, AVMixerRight);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, aaLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    auto currIndex = AAIndexMap[m_sharedData.multisamples];
                    currIndex = (currIndex + 1) % AASamples.size();
                   // m_sharedData.multisamples = AASamples[currIndex];

                    aaLabel.getComponent<cro::Text>().setString(AAStrings[currIndex]);
                    centreText(aaLabel);

                    toggleAntialiasing(m_sharedData, AASamples[currIndex] != 0, AASamples[currIndex]);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });


    //FOV down
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 89.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVFOVL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFOVR, AVResolutionL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVPuttAss, AVAAL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, fovLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto fov = m_sharedData.fov;
                fov = std::max(MinFOV, fov - 5.f);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                if (fov < m_sharedData.fov)
                {
                    m_sharedData.fov = fov;

                    //raise a window resize message to trigger callbacks
                    auto size = cro::App::getWindow().getSize();
                    auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
                    msg->data0 = size.x;
                    msg->data1 = size.y;
                    msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;

                    fovLabel.getComponent<cro::Text>().setString("FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));
                }
            }
        });

    //FOV up
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 89.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVFOVR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVPuttAss, AVResolutionR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVFOVL, AVAAR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, fovLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto fov = m_sharedData.fov;
                fov = std::min(MaxFOV, fov + 5.f);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                if (fov > m_sharedData.fov)
                {
                    m_sharedData.fov = fov;

                    //raise a window resize message to trigger callbacks
                    auto size = cro::App::getWindow().getSize();
                    auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
                    msg->data0 = size.x;
                    msg->data1 = size.y;
                    msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;

                    fovLabel.getComponent<cro::Text>().setString("FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));
                }
            }
        });


    //res down
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 73.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVResolutionL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVResolutionR, AVPixelScale);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVPostR, AVFOVL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, resLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_videoSettings.resolutionIndex = (m_videoSettings.resolutionIndex + (m_sharedData.resolutions.size() - 1)) % m_sharedData.resolutions.size();
                resLabel.getComponent<cro::Text>().setString(m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
                centreText(resLabel);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //res up
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 73.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVResolutionR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVPost, AVBeaconR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVResolutionL, AVFOVR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, resLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_videoSettings.resolutionIndex = (m_videoSettings.resolutionIndex + 1) % m_sharedData.resolutions.size();
                    resLabel.getComponent<cro::Text>().setString(m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
                    centreText(resLabel);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });


    //pixel scale check box
    entity = createHighlight(glm::vec2(81.f, 57.f));
    entity.setLabel("Scale up pixels to match the screen resolution.\nShortcut: +/- on numpad");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPixelScale);
    entity.getComponent<cro::UIInput>().setNextIndex(AVUnits, AVVertSnap);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVUnits, AVResolutionL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    togglePixelScale(m_sharedData, !m_sharedData.pixelScale);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //pixel scale checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 59.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.pixelScale ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //vertex snap checkbox
    entity = createHighlight(glm::vec2(81.f, 41.f));
    entity.setLabel("Usually used in conjunction with Pixel Scaling. Default is OFF.\nMay cause z-fighting.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVertSnap);
    entity.getComponent<cro::UIInput>().setNextIndex(AVGridL, AVFullScreen);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVGridR, AVPixelScale);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.vertexSnap = !m_sharedData.vertexSnap;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    //vertex snap centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 43.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.vertexSnap ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //full screen check box
    entity = createHighlight(glm::vec2(81.f, 25.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVFullScreen);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTreeL, AVBeacon);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTreeR, AVVertSnap);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_videoSettings.fullScreen = !m_videoSettings.fullScreen;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //full screen checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 27.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_videoSettings.fullScreen ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //beacon check box
    entity = createHighlight(glm::vec2(81.f, 9.f));
    entity.setLabel("Displays a beacon on the course at the pin position.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVBeacon);
    entity.getComponent<cro::UIInput>().setNextIndex(AVBeaconL, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVShadowR, AVFullScreen);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.showBeacon = !m_sharedData.showBeacon;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });


    //beacon checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 11.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.showBeacon ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //beacon colour preview
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(99.f, 11.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().setVertexData(
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), cro::Colour::Magenta),
        cro::Vertex2D(glm::vec2(0.f), cro::Colour::Magenta),
        cro::Vertex2D(glm::vec2(7.f), cro::Colour::Magenta),
        cro::Vertex2D(glm::vec2(7.f, 0.f), cro::Colour::Magenta)
    });
    auto& shader = m_sharedData.sharedResources->shaders.get(ShaderID::Beacon);
    entity.getComponent<cro::Drawable2D>().setShader(&shader);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = [&](cro::Entity e, float) {updateToolTip(e, ToolTipID::BeaconColour); };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto shaderID = shader.getGLHandle();
    auto uniformID = shader.getUniformID("u_colourRotation");
    glUseProgram(shaderID);
    glUniform1f(uniformID, m_sharedData.beaconColour);

    entity = createHighlight(glm::vec2(113.f, 9.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVBeaconL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVBeaconR, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVBeacon, AVFullScreen);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, shaderID, uniformID](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.beaconColour = std::max(m_sharedData.beaconColour - 0.1f, 0.f);
                    glUseProgram(shaderID);
                    glUniform1f(uniformID, m_sharedData.beaconColour);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity = createHighlight(glm::vec2(182.f, 9.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVBeaconR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVShadowL, TabController);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVBeaconL, AVResolutionR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, shaderID, uniformID](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.beaconColour = std::min(m_sharedData.beaconColour + 0.1f, 1.f);
                    glUseProgram(shaderID);
                    glUniform1f(uniformID, m_sharedData.beaconColour);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //colour slider
    auto colourPos = glm::vec2(131.f, 15.f);
    auto colourSlider = createSlider(colourPos);
    auto sliderData = SliderData(colourPos, 44.f);
    sliderData.onActivate =
        [&, shaderID, uniformID](float distance)
    {
        m_sharedData.beaconColour = distance;
        glUseProgram(shaderID);
        glUniform1f(uniformID, m_sharedData.beaconColour);
    };

    colourSlider.getComponent<cro::Callback>().setUserData<SliderData>(sliderData);
    colourSlider.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
        e.getComponent<cro::Transform>().setPosition({ pos.x + (width * m_sharedData.beaconColour), pos.y });
    };




    //ball trail checkbox
    entity = createHighlight(glm::vec2(246.f, 105.f));
    entity.setLabel("Draws a trail behind the ball when it is in flight.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTrail);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTrailL, AVPuttAss);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVAAR, AVVolumeDown);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.showBallTrail = !m_sharedData.showBallTrail;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //ball trail checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 107.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.showBallTrail ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //trail colour text
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 350.f, 114.f, HighlightOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.trailBeaconColour ? "Beacon" : "White");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto trailLabel = entity;

    //prev/next trail colour
    entity = createHighlight(glm::vec2(313.f, 105.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTrailL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTrailR, AVPostL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTrail, AVVolumeUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, trailLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.trailBeaconColour = !m_sharedData.trailBeaconColour;
                    trailLabel.getComponent<cro::Text>().setString(m_sharedData.trailBeaconColour ? "Beacon" : "White");
                    centreText(trailLabel);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    //just toggles a bool so share the callback.
    auto cbID = entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown];
    entity = createHighlight(glm::vec2(378.f, 105.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTrailR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVAAL, AVPostR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTrailL, AVVolumeUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = cbID;


    //putting assist toggle
    entity = createHighlight(glm::vec2(246.f, 89.f));
    entity.setLabel("Displays a power estimation when putting at the cost of awarded XP.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPuttAss);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFOVL, AVPost);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVFOVR, AVTrail);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.showPuttingPower = !m_sharedData.showPuttingPower;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //putting assist checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 91.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.showPuttingPower ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //post process checkbox
    entity = createHighlight(glm::vec2(246.f, 73.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPost);
    entity.getComponent<cro::UIInput>().setNextIndex(AVPostL, AVUnits);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVResolutionR, AVPuttAss);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessToggled;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //post process checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 75.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.usePostProcess ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto shaderLabel = createLabel(glm::vec2(325.f, 82.f), ShaderNames[m_sharedData.postProcessIndex]);
    centreText(shaderLabel);

    //prev/next post process
    entity = createHighlight(glm::vec2(263.f, 73.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPostL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVPostR, AVGridL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVPost, AVTrailL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, shaderLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.postProcessIndex = static_cast<std::int32_t>((m_sharedData.postProcessIndex + (ShaderNames.size() - 1)) % ShaderNames.size());
                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessIndexChanged;

                    shaderLabel.getComponent<cro::Text>().setString(ShaderNames[m_sharedData.postProcessIndex]);
                    centreText(shaderLabel);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity = createHighlight(glm::vec2(378.f, 73.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPostR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVResolutionL, AVGridR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVPostL, AVTrailR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, shaderLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.postProcessIndex = (m_sharedData.postProcessIndex + 1) % ShaderNames.size();
                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessIndexChanged;

                    shaderLabel.getComponent<cro::Text>().setString(ShaderNames[m_sharedData.postProcessIndex]);
                    centreText(shaderLabel);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //imperial measurements
    entity = createHighlight(glm::vec2(246.f, 57.f));
    entity.setLabel("Enables imperial measurements in yards, feet and inches.\nDefault is metric (metres and centimetres)");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVUnits);
    entity.getComponent<cro::UIInput>().setNextIndex(AVPixelScale, AVGridL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVPixelScale, AVPost);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.imperialMeasurements = !m_sharedData.imperialMeasurements;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //imperial checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 59.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.imperialMeasurements ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //prev/next grid transparency
    entity = createHighlight(glm::vec2(263.f, 41.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVGridL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVGridR, AVTreeL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVertSnap, AVUnits);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.gridTransparency = std::max(0.f, m_sharedData.gridTransparency - 0.1f);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity = createHighlight(glm::vec2(378.f, 41.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVGridR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVertSnap, AVTreeR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVGridL, AVPostR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.gridTransparency = std::min(1.f, m_sharedData.gridTransparency + 0.1f);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });


    static const std::array<std::string, 3u> TreeLabels =
    {
        "Classic", "Low", "High"
    };
    auto treeQualityText = createLabel({ 325.f, 34.f }, TreeLabels[m_sharedData.treeQuality]);
    centreText(treeQualityText);

    //prev / next tree quality
    entity = createHighlight(glm::vec2(286.f, 25.f));
    entity.setLabel("Switch between billboard and 3D trees.\nClassic trees are applied when the game is loaded.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTreeL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTreeR, AVShadowL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVFullScreen, AVGridL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, treeQualityText](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.treeQuality = (m_sharedData.treeQuality + 2) % 3;
                    treeQualityText.getComponent<cro::Text>().setString(TreeLabels[m_sharedData.treeQuality]);
                    centreText(treeQualityText);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::TreeQualityChanged;
                }
            });

    entity = createHighlight(glm::vec2(355.f, 25.f));
    entity.setLabel("Switch between billboard and 3D trees.\nClassic trees are applied when the game is loaded.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTreeR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFullScreen, AVShadowR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTreeL, AVGridR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, treeQualityText](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.treeQuality = (m_sharedData.treeQuality + 1) % 3;
                    treeQualityText.getComponent<cro::Text>().setString(TreeLabels[m_sharedData.treeQuality]);
                    centreText(treeQualityText);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::TreeQualityChanged;
                }
            });


    auto shadowQualityText = createLabel({ 325.f, 18.f }, m_sharedData.hqShadows ? "High" : "Low");
    centreText(shadowQualityText);

    //prev / next shadow quality
    auto shadowChanged = uiSystem.addCallback([&, shadowQualityText](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.hqShadows = !m_sharedData.hqShadows;
                    shadowQualityText.getComponent<cro::Text>().setString(m_sharedData.hqShadows ? "High" : "Low");
                    centreText(shadowQualityText);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::ShadowQualityChanged;
                }
            });

    entity = createHighlight(glm::vec2(286.f, 9.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVShadowL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVShadowR, WindowApply);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVBeaconR, AVTreeL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = shadowChanged;

    entity = createHighlight(glm::vec2(355.f, 9.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVShadowR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVBeacon, WindowClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVShadowL, AVTreeR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = shadowChanged;
}

void OptionsState::buildControlMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();

    auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& uiFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, 174.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(uiFont).setString(m_sharedData.baseState == StateID::Clubhouse ? "Controls (Billiards)" : "Controls");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());


    auto infoEnt = m_scene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, -4.f, TextOffset });
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(infoFont).setAlignment(cro::Text::Alignment::Centre);
    if (!Social::isSteamdeck())
    {
        infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
    }
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    infoEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    infoEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerConfig; //not the best description, just recycling existing members here...
    infoEnt.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [currTime, str] = e.getComponent<cro::Callback>().getUserData<std::pair<float, cro::String>>();
        currTime -= dt;
        if (currTime < 0)
        {
            e.getComponent<cro::Text>().setString(str);
            e.getComponent<cro::Callback>().active = false;
        }
    };

    parent.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());


    auto buttonChangeEnt = m_scene.createEntity();
    buttonChangeEnt.addComponent<cro::Transform>().setPosition(glm::vec3((parentBounds.width / 4.f) * 3.f, 130.f, TextOffset));
    buttonChangeEnt.addComponent<cro::Drawable2D>();
    buttonChangeEnt.addComponent<cro::Text>(infoFont).setString("Press Enter to Change");
    buttonChangeEnt.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
    buttonChangeEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(buttonChangeEnt);
    parent.getComponent<cro::Transform>().addChild(buttonChangeEnt.getComponent<cro::Transform>());

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto unselectID = uiSystem.addCallback(
        [infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            if (Social::isSteamdeck())
            {
                infoEnt.getComponent<cro::Text>().setString(" ");
            }
            else
            {
                infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
            }

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    auto createHighlight = [&, infoEnt](glm::vec2 position, std::int32_t keyIndex)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, HighlightOffset));
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
            [&, infoEnt, keyIndex](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (evt.type == SDL_KEYUP
                    && activated(evt))
                {
                    m_updatingKeybind = true;

                    //we have to copy this because GCC thinks getComponent<>()
                    //still returns an immutable reference
                    auto b = infoEnt;
                    b.getComponent<cro::Text>().setString("Press a Key");

                    m_bindingIndex = keyIndex;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        
        if (keyIndex > -1)
        {
            bindingEnts[keyIndex] = entity;
        }

        return entity;
    };

    if (m_sharedData.baseState == StateID::Clubhouse)
    {
        m_labelStrings =
        {
            "Take Shot",
            "Power Up",
            "Power Down",
            "Rotate Camera (Hold)",
            "Switch View",
            "Unused",
            "Side Spin",
            "Side Spin",
            "Top Spin",
            "Back Spin"
        };

        if (!Social::isSteamdeck())
        {
            //add overlay from sprite sheet
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 236.f, 32.f, HighlightOffset });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("extra_buttons");
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_xboxOverlay = entity;

            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 236.f, 32.f, HighlightOffset });
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("extra_buttons_ps");
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_psOverlay = entity;
        }
    }
    else
    {
        m_labelStrings =
        {
            "Take Shot",
            "Next Club",
            "Previous Club",
            "Ball Spin",
            "Emote Wheel",
            "Cancel Shot",
            "Aim Left",
            "Aim Right",
            "Raise Putt Camera",
            "Lower Putt Camera"
        };

        if (!Social::isSteamdeck())
        {
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 288.f, 67.f, HighlightOffset + 0.15f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("score_arrow");
            auto rect = entity.getComponent<cro::Sprite>().getTextureRect();
            rect.height -= 5.f;
            entity.getComponent<cro::Sprite>().setTextureRect(rect);
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_xboxOverlay = entity;

            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 288.f, 62.f, HighlightOffset + 0.05f });
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("score_arrow");
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_psOverlay = entity;
        }
    }

    //display a PS controller if we found one
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({236.f, 14.f, HighlightOffset / 2.f});
    if (Social::isSteamdeck())
    {
        entity.getComponent<cro::Transform>().move({ 0.f, 22.f, 0.f });
    }
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = Social::isSteamdeck() ? spriteSheet.getSprite("controller_deck") : spriteSheet.getSprite("controller_ps");
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_psController = entity;

    //or xbox
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 236.f, 14.f, HighlightOffset / 2.f });
    if (Social::isSteamdeck())
    {
        entity.getComponent<cro::Transform>().move({ 0.f, 22.f, 0.f });
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = Social::isSteamdeck() ? spriteSheet.getSprite("controller_deck") : spriteSheet.getSprite("controller_xbox");
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_xboxController = entity;


    struct Highlight final
    {
        enum
        {
            PrevClub,
            NextClub,
            CancelShot,
            AimLeft,
            AimRight,
            Swing,
            RaiseCam,
            LowerCam,
            SwitchView,
            RotateCam,
            EmoteWheel,
            SpinMenu,
            ShowScores,

            Count
        };
    };

    constexpr std::array<glm::vec2, Highlight::Count> ButtonPositions =
    {
        glm::vec2(258.f, 96.f),
        glm::vec2(324.f, 96.f),
        glm::vec2(347.f, 67.f),
        glm::vec2(230.f, 59.f),
        glm::vec2(230.f, 44.f),
        glm::vec2(352.f, 48.f),
        glm::vec2(230.f, 74.f),
        glm::vec2(230.f, 29.f),
        glm::vec2(338.f, 84.f),
        glm::vec2(302.f, 90.f),
        glm::vec2(338.f, 84.f),
        glm::vec2(302.f, 90.f),
        glm::vec2(285.f, 92.f)
    };

    constexpr std::array<glm::vec2, Highlight::Count> DeckPositions =
    {
        glm::vec2(248.f, 106.f), //
        glm::vec2(334.f, 106.f), //
        glm::vec2(342.f, 95.f), //
        glm::vec2(241.f, 95.f), //
        glm::vec2(251.f, 95.f), //
        glm::vec2(337.f, 89.f), //
        glm::vec2(246.f, 100.f), //
        glm::vec2(246.f, 89.f), //
        glm::vec2(337.f, 101.f), //
        glm::vec2(332.f, 95.f), //
        glm::vec2(337.f, 101.f), //
        glm::vec2(332.f, 95.f), //
        glm::vec2(257.f, 103.f)  //
    };

    const glm::vec2* Positions = Social::isSteamdeck() ? DeckPositions.data() : ButtonPositions.data();


    //prev club
    entity = createHighlight(Positions[Highlight::PrevClub], InputBinding::PrevClub);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlRB);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlTab, CtrlUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshR, TabAchievements);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&,infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::PrevClub] + keyString(InputBinding::PrevClub, m_sharedData) + " (Mouse Wheel Down)");

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });


    //next club
    entity = createHighlight(Positions[Highlight::NextClub], InputBinding::NextClub);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLB);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlThreshL, CtrlY);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlX, TabStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&,infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::NextClub] + keyString(InputBinding::NextClub, m_sharedData) + " (Mouse Wheel Up)");

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    //cancel shot
    entity = createHighlight(Positions[Highlight::CancelShot], InputBinding::CancelShot);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlB);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlInvY, CtrlA);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlRight, CtrlY);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&, infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::CancelShot] + keyString(InputBinding::CancelShot, m_sharedData));

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    //aim left
    entity = createHighlight(Positions[Highlight::AimLeft], InputBinding::Left);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLeft);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlB, CtrlLeft);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlVib, CtrlUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&,infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::Left] + keyString(InputBinding::Left, m_sharedData));

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    //aim right
    entity = createHighlight(Positions[Highlight::AimRight], InputBinding::Right);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlRight);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlA, CtrlDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlReset, CtrlRight);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&,infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::Right] + keyString(InputBinding::Right, m_sharedData));

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    //swing
    entity = createHighlight(Positions[Highlight::Swing], InputBinding::Action);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlA);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlReset, WindowClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlDown, CtrlB);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&,infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::Action] + keyString(InputBinding::Action, m_sharedData));

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });


    //top spin / raise putt cam
    entity = createHighlight(Positions[Highlight::RaiseCam], InputBinding::Up);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlUp);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlY, CtrlLeft);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLookR, CtrlRB);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&, infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::Up] + keyString(InputBinding::Up, m_sharedData));

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    //back spin / lower putt cam
    entity = createHighlight(Positions[Highlight::LowerCam], InputBinding::Down);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlDown);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlA, TabAchievements);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlReset, CtrlRight);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [&, infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::Down] + keyString(InputBinding::Down, m_sharedData));

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });


    if (m_sharedData.baseState == StateID::Clubhouse)
    {
        //switch view
        entity = createHighlight(Positions[Highlight::SwitchView], InputBinding::SwitchView);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlY);
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlInvX, CtrlB);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlUp, CtrlLB);
        entity.getComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight_white");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 0; //don't rebind this
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [&,infoEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::SwitchView] + keyString(InputBinding::SwitchView, m_sharedData));
            });

        //rotate cam
        entity = createHighlight(Positions[Highlight::RotateCam], InputBinding::CamModifier);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlX);
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlLB, WindowApply);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlTab, TabAchievements);
        entity.getComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight_white");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 0; //don't rebind this
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [&,infoEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::CamModifier] + keyString(InputBinding::CamModifier, m_sharedData));
            });
    }
    else
    {
        //emote wheel
        entity = createHighlight(Positions[Highlight::EmoteWheel], InputBinding::EmoteMenu);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlY);
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlInvX, CtrlB);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlUp, CtrlLB);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [&,infoEnt, buttonChangeEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::EmoteMenu] + keyString(InputBinding::EmoteMenu, m_sharedData));

                buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            });

        //spin menu
        entity = createHighlight(Positions[Highlight::SpinMenu], InputBinding::SpinMenu);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlX);
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlLB, WindowApply);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlTab, TabAchievements);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [&, infoEnt, buttonChangeEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(m_labelStrings[InputBinding::SpinMenu] + keyString(InputBinding::SpinMenu, m_sharedData));

                buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            });


        //show scores
        entity = createHighlight(Positions[Highlight::ShowScores], -1);
        entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlTab);
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlX, WindowApply);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlRB, TabAchievements);
        entity.getComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight_white");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 0; //don't rebind this
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [&, infoEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString("Show Scores (Tab)");
            });
    }


    //label entities
    struct LabelCallback final
    {
        std::int32_t labelIndex = 0;
        const InputBinding& binding;
        LabelCallback(std::int32_t idx, const InputBinding& b) : labelIndex(idx), binding(b) {}

        std::int32_t previousKey = 0;

        void operator() (cro::Entity e, float)
        {
            auto key = binding.keys[labelIndex];

            if (key != previousKey)
            {
                auto str = cro::Keyboard::keyString(key);
                str.replace(" ", "\n");

                e.getComponent<cro::Text>().setString(str);
                centreText(e);

                //update the button area to include the text
                auto pos = e.getComponent<cro::Transform>().getPosition();
                auto area = cro::Text::getLocalBounds(e);
                area.left += pos.x;
                area.bottom += pos.y;

                pos = bindingEnts[labelIndex].getComponent<cro::Transform>().getPosition();
                area.left -= pos.x;
                area.left -= (area.width / 2.f); //compensation for horizontal centering
                area.bottom -= pos.y;

                area.left -= 2.f;
                area.width += 4.f;
                area.bottom -= 2.f;
                area.height += 4.f;

                bindingEnts[labelIndex].getComponent<cro::UIInput>().area = area;
                bindingEnts[labelIndex].getComponent<cro::Transform>().setRotation(0.f);//hack to trigger transform callback
            }
            previousKey = key;
        }
    };

    auto createLabel = [&](glm::vec2 position, std::int32_t bindingIndex)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(position, TextOffset));
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        e.getComponent<cro::Text>().setVerticalSpacing(-4.f);
        e.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        e.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        //e.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function = LabelCallback(bindingIndex, m_sharedData.inputBinding);
        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    };

    if (!Social::isSteamdeck())
    {
        //prev club
        createLabel(glm::vec2(252.f, 110.f), InputBinding::PrevClub);

        //next club
        createLabel(glm::vec2(340.f, 110.f), InputBinding::NextClub);

        //cancel shot
        createLabel(glm::vec2(371.f, 76.f), InputBinding::CancelShot);

        //stroke
        createLabel(glm::vec2(366.f, 45.f), InputBinding::Action);

        //left
        createLabel(glm::vec2(220.f, 66.f), InputBinding::Left);

        //right
        createLabel(glm::vec2(220.f, 51.f), InputBinding::Right);

        //up
        createLabel(glm::vec2(220.f, 81.f), InputBinding::Up);

        //down
        createLabel(glm::vec2(220.f, 36.f), InputBinding::Down);


        if (m_sharedData.baseState == StateID::Clubhouse)
        {
            //rotate cam
            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 298.f, 110.f, TextOffset });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setString("RMB");
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

            //switch view
            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 351.f, 96.f, TextOffset });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setString("Num 1");
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
        else
        {
            //emote
            createLabel(glm::vec2(361.f, 100.f), InputBinding::EmoteMenu);

            //spin menu
            createLabel(glm::vec2(314.f, 115.f), InputBinding::SpinMenu);

            //scores
            entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition({ 274.f, 111.f, TextOffset });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
            entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
            entity.getComponent<cro::Text>().setString("Tab");
            entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
            entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
            parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        }
    }

    //mouse input controls
    auto createText = [&](glm::vec2 position, const std::string& str)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(position, TextOffset));
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        e.getComponent<cro::Text>().setString(str);
        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());

        return e;
    };
    std::stringstream ss;
    ss.precision(2);
    ss << "Swingput Threshold " << m_sharedData.swingputThreshold;
    auto swingputText = createText(glm::vec2(20.f, 124.f), ss.str());


    std::stringstream st;
    st.precision(2);
    st << "Look Speed (Billiards) " << m_sharedData.mouseSpeed;
    auto mouseText = createText(glm::vec2(20.f, 92.f), st.str());
    createText(glm::vec2(32.f, 63.f), "Invert X");
    createText(glm::vec2(32.f, 47.f), "Invert Y");
    createText(glm::vec2(118.f, 63.f), "Use Vibration");
    createText(glm::vec2(118.f, 47.f), "Hold For Power");

    //TODO don't duplicate these as they already exist in the AV menu
    auto selectedID = uiSystem.addCallback([infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            if (e.getLabel().empty())
            {
                if (Social::isSteamdeck())
                {
                    infoEnt.getComponent<cro::Text>().setString(" ");
                }
                else
                {
                    infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
                }
            }
            else
            {
                infoEnt.getComponent<cro::Text>().setString(e.getLabel());
            }
        });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    auto createSlider = [&, mouseText](glm::vec2 position) mutable
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slider");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), /*std::floor*/(bounds.height / 2.f), -TextOffset });

        //TODO this should be done outside the factory func
        auto userData = SliderData(position);
        userData.onActivate = [&, mouseText](float distance) mutable
        {
            //distance = 0-1
            m_sharedData.mouseSpeed = ConstVal::MinMouseSpeed + ((ConstVal::MaxMouseSpeed - ConstVal::MinMouseSpeed) * distance);

            std::stringstream st;
            st.precision(2);
            st << "Look Speed (Billiards) " << m_sharedData.mouseSpeed;

            //some versions of gcc complain about this so hack around
            //it by making a copy
            auto mt = mouseText;
            mt.getComponent<cro::Text>().setString(st.str());
        };
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<SliderData>(userData);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
            float amount = (m_sharedData.mouseSpeed - ConstVal::MinMouseSpeed) / (ConstVal::MaxMouseSpeed - ConstVal::MinMouseSpeed);

            e.getComponent<cro::Transform>().setPosition({ pos.x + (width * amount), pos.y });
        };

        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        m_sliders.push_back(entity);
        return entity;
    };

    auto swingputSlider = createSlider({ 35.f, 109.f });
    swingputSlider.getComponent<cro::Callback>().getUserData<SliderData>().onActivate =
        [&, swingputText](float distance) mutable
    {
        m_sharedData.swingputThreshold = ConstVal::MinSwingputThresh + ((ConstVal::MaxSwingputThresh - ConstVal::MinSwingputThresh) * distance);

        std::stringstream ss;
        ss.precision(2);
        ss << "Swingput Threshold " << m_sharedData.swingputThreshold;
        swingputText.getComponent<cro::Text>().setString(ss.str());
    };
    swingputSlider.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
        float amount = (m_sharedData.swingputThreshold - ConstVal::MinSwingputThresh) / (ConstVal::MaxSwingputThresh - ConstVal::MinSwingputThresh);

        e.getComponent<cro::Transform>().setPosition({ pos.x + (width * amount), pos.y });
    };

    //mouse speed slider
    createSlider(glm::vec2(35.f, 77.f));


    auto createSquareHighlight = [&](glm::vec2 pos)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(pos);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
        ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        ent.addComponent<cro::UIInput>().setGroup(MenuID::Controls);
        auto bounds = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::UIInput>().area = bounds;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        ent.addComponent<cro::Callback>().function = HighlightAnimationCallback();
        ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -HighlightOffset });
        ent.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };

    //swingput down
    entity = createSquareHighlight(glm::vec2(17.f, 103.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlThreshL);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlThreshR, CtrlLookL);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLB, TabAV);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, swingputText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                std::stringstream ss;
                ss.precision(2);
                ss << "Swingput Threshold " << m_sharedData.swingputThreshold;
                swingputText.getComponent<cro::Text>().setString(ss.str());

                m_sharedData.swingputThreshold = std::max(ConstVal::MinSwingputThresh, m_sharedData.swingputThreshold - 0.6f);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //swingput up
    entity = createSquareHighlight(glm::vec2(184.f, 103.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlThreshR);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlRB, CtrlLookR);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshL, TabAchievements);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, swingputText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                std::stringstream ss;
                ss.precision(2);
                ss << "Swingput Threshold " << m_sharedData.swingputThreshold;
                swingputText.getComponent<cro::Text>().setString(ss.str());

                m_sharedData.swingputThreshold = std::min(ConstVal::MaxSwingputThresh, m_sharedData.swingputThreshold + 0.6f);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });


    //mouse speed down
    entity = createSquareHighlight(glm::vec2(17.f, 71.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLookL);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlLookR, CtrlInvX);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLB, CtrlThreshL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, mouseText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                std::stringstream st;
                st.precision(2);
                st << "Look Speed (Billiards) " << m_sharedData.mouseSpeed;
                mouseText.getComponent<cro::Text>().setString(st.str());

                m_sharedData.mouseSpeed = std::max(ConstVal::MinMouseSpeed, m_sharedData.mouseSpeed - 0.1f);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //mouse speed up
    entity = createSquareHighlight(glm::vec2(184.f, 71.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLookR);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlUp, CtrlVib);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLookL, CtrlThreshR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, mouseText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                std::stringstream st;
                st.precision(2);
                st << "Look Speed (Billiards) " << m_sharedData.mouseSpeed;
                mouseText.getComponent<cro::Text>().setString(st.str());

                m_sharedData.mouseSpeed = std::min(ConstVal::MaxMouseSpeed, m_sharedData.mouseSpeed + 0.1f);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    //invert X
    entity = createSquareHighlight(glm::vec2(17.f, 54.f));
    entity.setLabel("Invert the controller X axis when playing Billiards");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlInvX);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlVib, CtrlInvY);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlY, CtrlLookL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.invertX = !m_sharedData.invertX;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    //centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(19.f, 56.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.invertX ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //rumble enable
    entity = createSquareHighlight(glm::vec2(103.f, 54.f));
    entity.setLabel("Enable or disable controller vibration");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlVib);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlLeft, CtrlAltPower);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlInvX, CtrlLookR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.enableRumble = m_sharedData.enableRumble ? 0 : 1;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(105.f, 56.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = static_cast<float>(m_sharedData.enableRumble);
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //alt power input
    entity = createSquareHighlight(glm::vec2(103.f, 38.f));
    entity.setLabel("When enabled press and hold Action to select stroke power\nelse use the default 2-tap method when disabled");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlAltPower);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlLeft, CtrlReset);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlInvY, CtrlVib);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.pressHold = !m_sharedData.pressHold;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });
    //y'know if we defined these first we could capture them and update them in the button callback...
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(105.f, 40.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            float scale = m_sharedData.pressHold ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //invert Y
    entity = createSquareHighlight(glm::vec2(17.f, 38.f));
    entity.setLabel("Invert the controller Y axis when playing Billiards");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlInvY);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlRight, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlB, CtrlInvX);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.invertY = !m_sharedData.invertY;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(19.f, 40.f, HighlightOffset));
    entity.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
        cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
    };
    entity.getComponent<cro::Drawable2D>().updateLocalBounds();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        float scale = m_sharedData.invertY ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //reset to defaults
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 11.f, HighlightOffset));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("small_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlReset);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlDown, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlA, CtrlVib);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            if (!Social::isSteamdeck())
            {
                infoEnt.getComponent<cro::Text>().setString("Reset All Keybinds To Their Default Values");
            }

            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            if (!Social::isSteamdeck())
            {
                infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
            }

            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) 
        {
            if (activated(evt))
            {
                InputBinding defaultBinding;
                m_sharedData.inputBinding.keys = defaultBinding.keys;

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -HighlightOffset });
    entity.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void OptionsState::buildAchievementsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    //render details to buffer
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, 174.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(largeFont).setString("Achievements");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());


    cro::SimpleText title(largeFont);
    title.setFillColour(TextNormalColour);
    title.setCharacterSize(UITextSize);

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    cro::SimpleText desc(smallFont);
    desc.setFillColour(TextNormalColour);
    desc.setCharacterSize(InfoTextSize);

    cro::SimpleText timestamp(smallFont);
    timestamp.setFillColour(TextNormalColour);
    timestamp.setCharacterSize(InfoTextSize);

    cro::SimpleQuad icon;
    icon.setScale({ 0.5f, 0.5f });


    static constexpr float VerticalSpacing = 42.f;

    auto bufferSize = m_achievementBuffer.getSize();
    cro::FloatRect cropping(0.f, 0.f, static_cast<float>(bufferSize.x), static_cast<float>(bufferSize.y));

    bufferSize.y = static_cast<std::uint32_t>(VerticalSpacing) * AchievementID::Count;
    bufferSize.y += 4;
    m_achievementBuffer.create(bufferSize.x, bufferSize.y, false);

    auto textureRect = cropping;
    textureRect.bottom = bufferSize.y - cropping.height;
    parent.getComponent<cro::Sprite>().setTextureRect(textureRect);

    glm::vec2 position(12.f, bufferSize.y - (4 + VerticalSpacing));
    const glm::vec2 TitleOffset(40.f, 24.f);
    const glm::vec2 DescOffset(40.f, 13.f);
    const glm::vec2 TSOffset(40.f, 2.f);

    static constexpr float Border = 0.f;
    const std::array Colours = { cro::Colour(std::uint8_t(107), 111, 114, 120), cro::Colour(std::uint8_t(58), 57, 65, 120) };
    cro::SimpleVertexArray background;
    std::vector<cro::Vertex2D> verts;
    for (auto i = 0; i < AchievementID::Count + 1; ++i)
    {
        verts.emplace_back(glm::vec2(Border, VerticalSpacing * i), Colours[i % 2]);
        verts.emplace_back(glm::vec2(bufferSize.x - Border, VerticalSpacing * i), Colours[i % 2]);
        verts.emplace_back(glm::vec2(Border, VerticalSpacing * (i + 1)), Colours[i % 2]);
        verts.emplace_back(glm::vec2(bufferSize.x - Border, VerticalSpacing * (i + 1)), Colours[i % 2]);
    }

    background.setVertexData(verts);
    background.setPosition({ 0.f, -6.f });

    m_achievementBuffer.clear(cro::Colour::Transparent);
    background.draw();
    for (auto i = 1; i < AchievementID::Count; ++i)
    {
        auto iconData = Achievements::getIcon(AchievementStrings[i]);
        if (iconData.texture)
        {
            icon.setTexture(*iconData.texture);
            
            glm::vec2 texSize(iconData.texture->getSize());
            iconData.textureRect.left *= texSize.x;
            iconData.textureRect.width *= texSize.x;
            iconData.textureRect.bottom *= texSize.y;
            iconData.textureRect.height *= texSize.y;

            icon.setTextureRect(iconData.textureRect);
            icon.setPosition(position);
            icon.draw();
        }

        auto* data = Achievements::getAchievement(AchievementStrings[i]);
        if (data->achieved || !AchievementDesc[i].second)
        {
            title.setString(AchievementLabels[i]);
            desc.setString(AchievementDesc[i].first);
        }
        else
        {
            title.setString("HIDDEN");
            desc.setString("????????");
        }
        title.setPosition(position + TitleOffset);
        title.draw();

        desc.setPosition(position + DescOffset);
        desc.draw();

        auto ts = data->timestamp;
        if (ts != 0)
        {
            timestamp.setPosition(position + TSOffset);
            timestamp.setString("Achieved " + cro::SysTime::dateString(ts));
            timestamp.draw();
        }

        position.y -= VerticalSpacing;
    }
    m_achievementBuffer.display();


    //set up scroll callback
    parent.addComponent<cro::Callback>().setUserData<float>(textureRect.bottom);
    parent.getComponent<cro::Callback>().function = ScrollCallback();


    //scroll functions (kept here to hook mouse scroll callback)
    m_scrollFunctions[ScrollID::AchUp] = [&, parent, bufferSize](cro::Entity, const cro::ButtonEvent evt) mutable
    {
        if (activated(evt))
        {
            auto rect = parent.getComponent<cro::Sprite>().getTextureRect();
            auto& target = parent.getComponent<cro::Callback>().getUserData<float>();
            if (target < (bufferSize.y - rect.height))
            {
                target += VerticalSpacing;
                parent.getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    };

    m_scrollFunctions[ScrollID::AchDown] = [&, parent](cro::Entity, const cro::ButtonEvent evt) mutable
    {
        if (activated(evt))
        {
            auto& target = parent.getComponent<cro::Callback>().getUserData<float>();

            if (target > VerticalSpacing)
            {
                target -= VerticalSpacing;
                parent.getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    };


    //scroll button sprites
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cropping.width - 18.f, cropping.height - 18.f, TextOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_up");
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cropping.width - 18.f, 8.f, TextOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_down");
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto createHighlight = [&](glm::vec2 pos)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(pos);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
        ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        ent.addComponent<cro::UIInput>().setGroup(MenuID::Achievements);
        auto bounds = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::UIInput>().area = bounds;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        ent.addComponent<cro::Callback>().function = HighlightAnimationCallback();
        ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -HighlightOffset });
        ent.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };
    
    auto upHighlight = createHighlight({ cropping.width - 19.f, cropping.height - 19.f });
    upHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollUp);
    upHighlight.getComponent<cro::UIInput>().setNextIndex(TabAV, ScrollDown);
    upHighlight.getComponent<cro::UIInput>().setPrevIndex(TabStats, TabStats);
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::AchUp]);

    auto downHighlight = createHighlight({ cropping.width - 19.f, 7.f });
    downHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollDown);
    downHighlight.getComponent<cro::UIInput>().setNextIndex(WindowAdvanced, WindowClose);
    downHighlight.getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, ScrollUp);
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::AchDown]);
}

void OptionsState::buildStatsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, 174.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(largeFont).setString("Stats");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());


    cro::SimpleText title(largeFont);
    title.setFillColour(TextNormalColour);
    title.setCharacterSize(UITextSize);

    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    cro::SimpleText desc(smallFont);
    desc.setFillColour(TextNormalColour);
    desc.setCharacterSize(InfoTextSize);


    static constexpr float VerticalSpacing = 32.f;

    auto bufferSize = m_statsBuffer.getSize();
    cro::FloatRect cropping(0.f, 0.f, static_cast<float>(bufferSize.x), static_cast<float>(bufferSize.y));

    bufferSize.y = static_cast<std::uint32_t>(VerticalSpacing) * (StatID::Count+ 1);
    bufferSize.y += 4;
    m_statsBuffer.create(bufferSize.x, bufferSize.y, false);

    auto textureRect = cropping;
    textureRect.bottom = bufferSize.y - cropping.height;
    parent.getComponent<cro::Sprite>().setTextureRect(textureRect);

    glm::vec2 position(30.f, bufferSize.y - (4 + VerticalSpacing));
    const glm::vec2 TitleOffset(-2.f, 24.f);
    const glm::vec2 DescOffset(-2.f, 13.f);


    static constexpr float Border = 0.f;
    const std::array Colours = { cro::Colour(std::uint8_t(107), 111, 114, 120), cro::Colour(std::uint8_t(58), 57, 65, 120) };
    cro::SimpleVertexArray background;
    std::vector<cro::Vertex2D> verts;
    for (auto i = 0; i < AchievementID::Count; ++i)
    {
        verts.emplace_back(glm::vec2(Border, VerticalSpacing * i), Colours[i % 2]);
        verts.emplace_back(glm::vec2(bufferSize.x - Border, VerticalSpacing * i), Colours[i % 2]);
        verts.emplace_back(glm::vec2(Border, VerticalSpacing * (i + 1)), Colours[i % 2]);
        verts.emplace_back(glm::vec2(bufferSize.x - Border, VerticalSpacing * (i + 1)), Colours[i % 2]);
    }

    background.setVertexData(verts);
    background.setPosition({ 0.f, 4.f });

    m_statsBuffer.clear(cro::Colour::Transparent);
    background.draw();
    for (auto i = 0u; i < StatID::Count; ++i)
    {
        //title
        title.setPosition(position + TitleOffset);
        title.setString(StatLabels[i]);
        title.draw();

        //value
        std::string value;
        switch (StatTypes[i])
        {
        default:
        case StatType::Float:
        {
            std::stringstream ss;
            ss.precision(2);
            ss << std::fixed << Achievements::getStat(StatStrings[i])->value;
            value = ss.str();
        }
            break;
        case StatType::Integer:
            value = std::to_string(static_cast<std::int32_t>(Achievements::getStat(StatStrings[i])->value));
            break;
        case StatType::Percent:
        {
            float v = Achievements::getStat(StatStrings[i])->value * 100.f;
            std::stringstream ss;
            ss.precision(2);
            ss << std::fixed << v << "%";
            value = ss.str();
        }
            break;
        case StatType::Time:
        {
            std::int32_t v = static_cast<std::int32_t>(Achievements::getStat(StatStrings[i])->value);
            auto seconds = v % 60;
            auto minutes = v / 60;
            auto hours = minutes / 60;
            minutes %= 60;

            std::stringstream ss;
            ss << hours << "h " << minutes << "m " << seconds << "s";
            value = ss.str();
        }
            break;
        }

        desc.setPosition(position + DescOffset);
        desc.setString(value);
        desc.draw();

        position.y -= VerticalSpacing;
    }
    m_statsBuffer.display();


    //set up scroll callback
    parent.addComponent<cro::Callback>().setUserData<float>(textureRect.bottom);
    parent.getComponent<cro::Callback>().function = ScrollCallback();

    //scroll functions (kept here to hook mouse scroll callback)
    m_scrollFunctions[ScrollID::StatUp] = [&, parent, bufferSize](cro::Entity, const cro::ButtonEvent evt) mutable
    {
        if (activated(evt))
        {
            auto rect = parent.getComponent<cro::Sprite>().getTextureRect();
            auto& target = parent.getComponent<cro::Callback>().getUserData<float>();
            if (target < (bufferSize.y - rect.height))
            {
                target += VerticalSpacing;
                parent.getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    };

    m_scrollFunctions[ScrollID::StatDown] = [&, parent](cro::Entity, const cro::ButtonEvent evt) mutable
    {
        if (activated(evt))
        {
            auto& target = parent.getComponent<cro::Callback>().getUserData<float>();

            if (target > VerticalSpacing)
            {
                target -= VerticalSpacing;
                parent.getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    };


    //scroll button sprites
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cropping.width - 18.f, cropping.height - 18.f, TextOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_up");
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cropping.width - 18.f, 8.f, TextOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_down");
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto createHighlight = [&](glm::vec2 pos)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(pos);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
        ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        ent.addComponent<cro::UIInput>().setGroup(MenuID::Stats);
        auto bounds = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::UIInput>().area = bounds;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

        ent.addComponent<cro::Callback>().function = HighlightAnimationCallback();
        ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -HighlightOffset });
        ent.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };

    auto upHighlight = createHighlight({ cropping.width - 19.f, cropping.height - 19.f });
    upHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollUp);
    upHighlight.getComponent<cro::UIInput>().setNextIndex(TabAV, ScrollDown);
    upHighlight.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, TabAchievements);
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::StatUp]);

    auto downHighlight = createHighlight({ cropping.width - 19.f, 7.f });
    downHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollDown);
    downHighlight.getComponent<cro::UIInput>().setNextIndex(WindowAdvanced, WindowClose);
    downHighlight.getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, ScrollUp);
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::StatDown]);
}

void OptionsState::createButtons(cro::Entity parent, std::int32_t menuID, std::uint32_t selectedID, std::uint32_t unselectedID, const cro::SpriteSheet& spriteSheet)
{
    //controls which input should be selected previously/next
    std::size_t upLeftA = 0;
    std::size_t upLeftB = 0;
    std::size_t upRightA = 0;
    std::size_t upRightB = 0;
    std::size_t downLeftA = 0;
    std::size_t downLeftB = 0;
    std::size_t downRightA = 0;
    std::size_t downRightB = 0;

    switch (menuID)
    {
    default:
        break;
    case MenuID::Video:
        downLeftA = TabController;
        downLeftB = TabController;
        downRightA = TabAchievements;
        downRightB = TabStats;
        upLeftA = AVBeacon;
        upLeftB = AVBeaconL;
        upRightA = AVShadowL;
        upRightB = AVShadowR;
        break;
    case MenuID::Controls:
        downLeftA = TabAV;
        downLeftB = TabAV;
        downRightA = TabAchievements;
        downRightB = TabStats;
        upLeftA = CtrlInvY;
        upLeftB = CtrlReset;
        upRightA = CtrlA;
        upRightB = CtrlA;
        break;
    case MenuID::Achievements:
        downLeftA = TabAV;
        downLeftB = TabController;
        downRightA = TabStats;
        downRightB = TabStats;
        upLeftA = TabAV;
        upLeftB = TabController;
        upRightA = ScrollDown;
        upRightB = ScrollDown;
        break;
    case MenuID::Stats:
        downLeftA = TabAV;
        downLeftB = TabController;
        downRightA = TabAchievements;
        downRightB = ScrollUp;
        upLeftA = TabAV;
        upLeftB = TabController;
        upRightA = ScrollDown;
        upRightB = ScrollDown;
        break;
    }
    
    
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto textHideCallback = [&, menuID](cro::Entity e, float)
    {
        auto current = static_cast<std::int32_t>(m_scene.getSystem<cro::UISystem>()->getActiveGroup());
        float scale = (current == menuID || current == MenuID::Dummy) ? 1 : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };

    //advanced
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(2.f, 1.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("large_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(WindowClose, upLeftA);
    entity.getComponent<cro::UIInput>().setNextIndex(WindowCredits, downLeftA);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                cro::Console::show();
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //hack to stop multiple instances covering each other when not active
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //credits
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(78.f, 1.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("large_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, upLeftB);
    entity.getComponent<cro::UIInput>().setNextIndex(WindowApply, downLeftB);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                requestStackPush(StateID::Credits);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //hack to stop multiple instances covering each other when not active
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //apply
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(297.f, 1.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("small_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowApply);
    entity.getComponent<cro::UIInput>().setPrevIndex(WindowCredits, upRightA);
    entity.getComponent<cro::UIInput>().setNextIndex(WindowClose, downRightA);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                if (m_videoSettings.fullScreen != cro::App::getWindow().isFullscreen())
                {
                    cro::App::getWindow().setFullScreen(m_videoSettings.fullScreen);
                }
                else
                {
                    cro::App::getWindow().setSize(m_sharedData.resolutions[m_videoSettings.resolutionIndex]);

                    auto& cam = m_scene.getActiveCamera().getComponent<cro::Camera>();
                    cam.active = true;
                    cam.resizeCallback(cam);
                }
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //close
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(348.f, 1.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("small_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(WindowApply, upRightB);
    entity.getComponent<cro::UIInput>().setNextIndex(WindowAdvanced, downRightB);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                quitState();
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void OptionsState::updateToolTip(cro::Entity e, std::int32_t tipID)
{
    //update the tool tip
    auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
    auto bounds = e.getComponent<cro::Drawable2D>().getLocalBounds();
    bounds = e.getComponent<cro::Transform>().getWorldTransform() * bounds;

    if (bounds.contains(mousePos))
    {
        mousePos.x = std::floor(mousePos.x);
        mousePos.y = std::floor(mousePos.y);
        mousePos.z = 2.f;

        if (!m_tooltips[tipID].getComponent<ToolTip>().target.isValid()
            || m_tooltips[tipID].getComponent<ToolTip>().target == e)
        {
            if (m_tooltips[tipID].getComponent<cro::Transform>().getScale().x == 0)
            {
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                m_tooltips[tipID].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
            }

            m_tooltips[tipID].getComponent<cro::Transform>().setScale(m_viewScale);

            m_tooltips[tipID].getComponent<ToolTip>().target = e;
        }

        m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        m_activeToolTip = tipID;
    }
    else
    {
        if (m_tooltips[tipID].getComponent<ToolTip>().target == e)
        {
            m_tooltips[tipID].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            m_tooltips[tipID].getComponent<ToolTip>().target = {};

            m_activeToolTip = -1;
        }
    }
}

void OptionsState::updateActiveCallbacks()
{
    auto group = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
    const auto& entities = m_scene.getSystem<cro::CallbackSystem>()->getEntities();
    for (auto entity : entities)
    {
        if (entity.hasComponent<cro::UIInput>())
        {
            entity.getComponent<cro::Callback>().active = entity.getComponent<cro::UIInput>().getGroup() == group;
        }
    }
}

void OptionsState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}