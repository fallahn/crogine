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

#include "../Colordome-32.hpp"
#include "../WebsocketServer.hpp"

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

#include <crogine/audio/AudioDevice.hpp>

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
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/SpriteAnimator.hpp>
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
    struct LastInput final
    {
        enum
        {
            XBox, PS,Keyboard
        };
    };
    std::int32_t lastInput = LastInput::Keyboard;

    constexpr float CameraDepth = 3.f;

    constexpr float BackgroundDepth = -0.5f;
    constexpr float OptionsDepth = -0.4f;
    constexpr float TabWindowDepth = 0.1f;
    constexpr float TabBarDepth = 0.15f;

    constexpr glm::vec3 PanelPosition(55.f, 128.f, TabWindowDepth); //y is 20 if we increase panel by 66
    constexpr glm::vec3 HiddenPosition(-10000.f);

    constexpr float HighlightOffset = 0.25f;
    constexpr float TextOffset = 0.15f;

    constexpr float ToolTipDepth = CameraDepth - 0.8f;

    constexpr glm::vec2 ScrollBarSize(9.f, 185.f);
    constexpr cro::Colour ScrollBarColour(0.f, 0.f, 0.f, 0.15f);

    //constexpr glm::vec2 BackgroundSize(192.f, 108.f);
    //constexpr cro::Colour BackgroundColour = cro::Colour(std::uint8_t(26), 30, 45);

    struct MenuID final
    {
        enum
        {
            Video, Controls, Dummy, Achievements, Stats, Settings
        };
    };

    const std::array<cro::String, MixerChannel::Count> MixerLabels =
    {
        "Menu Music",
        "Effects",
        "Menu Sounds",
        "Voice",
        "Vehicles",
        "Environment",
        "Game Music",
        "Text To Speech"
    };
    //generally static vars would be a bad idea, but in this case
    //a static index value will remember the last channel between
    //showing instances of options, as well as being available to
    //the slider callbacks :3
    std::uint8_t mixerChannelIndex = MixerChannel::Music;

    std::size_t audioDeviceIndex = 0;

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

    const std::array<std::string, 3u> ShadowText =
    {
        "Low", "High", "Very High"
    };

    //this needs to be here to be accessed by label callback
    std::array<cro::Entity, InputBinding::Count> bindingEnts = {}; //buttons to activate key rebinding

    struct LayoutID final
    {
        enum
        {
            Keyboard, XBox, PS, Deck
        };
    };

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

    constexpr cro::FloatRect LabelCrop(0.f, -8.f, 128.f, 9.f);
    struct TextScrollData final
    {
        std::int32_t direction = 0;
        float currTime = 0.f;
        float maxWidth = 100.f;
    };

    struct TabID final
    {
        enum
        {
            AV, Controls, Settings, Achievements, Stats,
            Count
        };
    };

    struct ButtonID final
    {
        enum
        {
            Credits, Advanced, Apply, Quit,
            Count
        };
    };

    //used as a component by parent nodes to hold
    //top and bottom row button entities
    struct PageButtons final
    {
        std::array<cro::Entity, TabID::Count> tabs = {};
        std::array<cro::Entity, ButtonID::Count> buttons = {};
    };

    inline cro::String keyString(std::int32_t idx, const SharedStateData& sharedData)
    {
        return " (" + cro::Keyboard::keyString(sharedData.inputBinding.keys[idx]) + ")";
    };
}

OptionsState::OptionsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State            (ss, ctx),
    m_scene                 (ctx.appInstance.getMessageBus(), 480/*, cro::INFO_FLAG_SYSTEMS_ACTIVE | cro::INFO_FLAG_SYSTEM_TIME*/),
    m_sharedData            (sd),
    m_updatingKeybind       (false),
    m_lastMousePos          (0.f),
    m_bindingIndex          (-1),
    m_currentTabFunction    (0),
    m_flagIndex             (0),
    m_activeToolTip         (-1),
    m_viewScale             (2.f),
    m_refreshControllers    (false)
{
    if (Social::isSteamdeck())
    {
        lastInput = LastInput::XBox;
    }
    
    std::fill(m_controllerScrollAxes.begin(), m_controllerScrollAxes.end(), 0);

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
    //cacheState(StateID::MessageOverlay); //don't cache this else the correct menu isn't created
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
        if (m_layoutEnt.getComponent<cro::SpriteAnimation>().id != LayoutID::Keyboard)
        {
            std::size_t i = Social::isSteamdeck(false) ? LayoutID::Deck
                : cro::GameController::getControllerCount() == 0 ? LayoutID::XBox :
                hasPSLayout(controllerID) ? LayoutID::PS : LayoutID::XBox;
            m_layoutEnt.getComponent<cro::SpriteAnimation>().play(i);
        }

        lastInput = hasPSLayout(controllerID) ? LastInput::PS : LastInput::XBox;
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
            updateKeybind(/*evt.key.keysym.sym*/SDLK_ESCAPE);
        }
    };

    const auto scrollMenu = [&](std::int32_t direction)
    {
        std::int32_t callbackID = 0;
        const auto menuID = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
        if (menuID == MenuID::Achievements)
        {
            callbackID = direction == 0 ? ScrollID::AchUp : ScrollID::AchDown;
        }
        else if (menuID == MenuID::Stats)
        {
            callbackID = direction == 0 ? ScrollID::StatUp : ScrollID::StatDown;
        }
        else
        {
            return;
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
        lastInput = LastInput::Keyboard;

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

        if (!m_updatingKeybind)
        {
            if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
            {
                m_currentTabFunction = (m_currentTabFunction + (m_tabFunctions.size() - 1)) % m_tabFunctions.size();
                m_tabFunctions[m_currentTabFunction]();
            }
            else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
            {
                m_currentTabFunction = (m_currentTabFunction + 1) % m_tabFunctions.size();
                m_tabFunctions[m_currentTabFunction]();
            }
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces refresh
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
        if (std::abs(evt.caxis.value) > cro::GameController::LeftThumbDeadZone)
        {
            toggleControllerIcon(cro::GameController::controllerID(evt.caxis.which));

            cro::App::getWindow().setMouseCaptured(true);
        }

        if (evt.caxis.axis == cro::GameController::AxisRightY)
        {
            const auto menuID = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
            const auto controllerID = cro::GameController::controllerID(evt.caxis.which);
            const auto amt = evt.caxis.value;

            if (amt < -cro::GameController::LeftThumbDeadZone
                && m_controllerScrollAxes[controllerID] >= -cro::GameController::LeftThumbDeadZone)
            {
                cro::ButtonEvent fakeEvent;
                fakeEvent.type = SDL_CONTROLLERBUTTONDOWN;
                fakeEvent.cbutton.button = cro::GameController::ButtonA;

                if (menuID == MenuID::Achievements)
                {
                    m_scrollFunctions[ScrollID::AchUp](cro::Entity(), fakeEvent);
                }
                else if (menuID == MenuID::Stats)
                {
                    m_scrollFunctions[ScrollID::StatUp](cro::Entity(), fakeEvent);
                }
            }
            else if (amt > cro::GameController::LeftThumbDeadZone
                && m_controllerScrollAxes[controllerID] <= cro::GameController::LeftThumbDeadZone)
            {
                cro::ButtonEvent fakeEvent;
                fakeEvent.type = SDL_CONTROLLERBUTTONDOWN;
                fakeEvent.cbutton.button = cro::GameController::ButtonA;
                
                if (menuID == MenuID::Achievements)
                {
                    m_scrollFunctions[ScrollID::AchDown](cro::Entity(), fakeEvent);
                }
                else if (menuID == MenuID::Stats)
                {
                    m_scrollFunctions[ScrollID::StatDown](cro::Entity(), fakeEvent);
                }
            }
            else if ((amt > -cro::GameController::LeftThumbDeadZone
                && m_controllerScrollAxes[controllerID] <= -cro::GameController::LeftThumbDeadZone)
                ||
                (amt < cro::GameController::LeftThumbDeadZone
                    && m_controllerScrollAxes[controllerID] >= cro::GameController::LeftThumbDeadZone))
            {
                resetScroll();
            }

            m_controllerScrollAxes[controllerID] = amt;
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        lastInput = LastInput::Keyboard;

        if (evt.button.button == SDL_BUTTON_LEFT
            && !m_updatingKeybind)
        {
            if (m_activeToolTip == -1)
            {
                if (!pickSlider())
                {
                    pickScrollBar();
                }
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
            m_activeScrollBar = {};
        }
        else if (evt.button.button == SDL_BUTTON_RIGHT)
        {
            closeWindow();
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        lastInput = LastInput::Keyboard;

        updateSlider();
        updateScrollBar();
        cro::App::getWindow().setMouseCaptured(false);
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        lastInput = LastInput::Keyboard;
        m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;

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

    else if (evt.type == SDL_CONTROLLERDEVICEADDED
        || evt.type == SDL_CONTROLLERDEVICEREMOVED)
        {
            m_refreshControllers = true;

            resetScroll();
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
        else if (data.type == cro::Message::SystemEvent::AudioDeviceChanged)
        {
            assertDeviceIndex();
            refreshDeviceLabel();
        }
    }
    m_scene.forwardMessage(msg);
}

bool OptionsState::simulate(float dt)
{
    for (auto i = 0u; i < m_scrollPresses.size(); ++i)
    {
        auto& press = m_scrollPresses[i];
        if (press.pressed)
        {
            press.pressedTimer += dt;
            if (press.pressedTimer > ScrollPress::PressTime)
            {
                press.active = true;
            }
        }

        if (press.active)
        {
            press.scrollTimer += dt;
            if (press.scrollTimer > ScrollPress::ScrollTime)
            {
                press.scrollTimer -= ScrollPress::ScrollTime;
                
                //this is important we use the correct type of
                //event here because only controller presses (not
                //mouse presses) should latch the active state
                cro::ButtonEvent fakeEvent;
                /*fakeEvent.type = SDL_CONTROLLERBUTTONDOWN;
                fakeEvent.cbutton.button = cro::GameController::ButtonA;*/
                fakeEvent.type = SDL_MOUSEBUTTONDOWN;
                fakeEvent.button.button = SDL_BUTTON_LEFT;
                m_scrollFunctions[i](cro::Entity(), fakeEvent);
            }
        }
    }


    m_scene.simulate(dt);

    if (m_refreshControllers)
    {
        //*sigh* the names aren't updated until AFTER the event
        //so we have to delay a frame.
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
            {
                refreshControllerList();

                e.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(e);
            };

        m_refreshControllers = false;
    }

    return true;
}

void OptionsState::render()
{
    m_scene.render();
}

//private
bool OptionsState::pickSlider()
{
    auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());

    for (auto& entity : m_sliders)
    {
        auto bounds = entity.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = entity.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            m_activeSlider = entity;
            m_lastMousePos = mousePos;

            return true;
        }
    }
    return false;
}

void OptionsState::updateSlider()
{
    if (m_activeSlider.isValid())
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        float movement = (mousePos.x - m_lastMousePos.x) / m_viewScale.x;

        const auto& [pos, width, onActivate] = m_activeSlider.getComponent<cro::Callback>().getUserData<SliderData>();
        float maxMove = pos.x + width;
        float minMove = pos.x;

        auto currentPos = m_activeSlider.getComponent<cro::Transform>().getPosition();
        float finalPos = std::min(maxMove, std::max(minMove, currentPos.x + movement));

        m_activeSlider.getComponent<cro::Transform>().setPosition({ finalPos, currentPos.y });

        onActivate((finalPos - pos.x) / width);

        m_lastMousePos = mousePos;
    }
}

void OptionsState::pickScrollBar()
{
    auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());

    for (auto& entity : m_scrollBars)
    {
        auto bounds = entity.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = entity.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            m_activeScrollBar = entity;
            m_lastMousePos = mousePos;

            break;
        }
    }
}

void OptionsState::updateScrollBar()
{
    if (m_activeScrollBar.isValid())
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        const float movement = (mousePos.y - m_lastMousePos.y) / m_viewScale.y;

        const auto& [pos, length, onActivate] = m_activeScrollBar.getComponent<cro::Callback>().getUserData<SliderData>();
        float maxMove = pos.y + length;
        float minMove = pos.y;

        auto currentPos = m_activeScrollBar.getComponent<cro::Transform>().getPosition();
        float finalPos = std::min(maxMove, std::max(minMove, currentPos.y + movement));

        m_activeScrollBar.getComponent<cro::Transform>().setPosition({ currentPos.x, finalPos });

        onActivate((finalPos - pos.y) / length);

        m_lastMousePos = mousePos;
    }
}

void OptionsState::updateKeybind(SDL_Keycode key)
{
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

    //prevent binding top row and function keys
    const std::array LockedKeys =
    {
        SDLK_1,
        SDLK_2,
        SDLK_3,
        SDLK_4,
        SDLK_5,
        SDLK_6,
        SDLK_7,
        SDLK_8,
        SDLK_9,
        SDLK_0,

        SDLK_F1,
        SDLK_F2,
        SDLK_F3,
        SDLK_F4,
        SDLK_F5,
        SDLK_F6,
        SDLK_F7,
        SDLK_F8,
        SDLK_F9,
        SDLK_F10,
        SDLK_F11,
        SDLK_F12,
    };
    if (auto result = std::find(std::begin(LockedKeys), std::end(LockedKeys), key); result != std::end(LockedKeys))
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::InfoString;
        cmd.action = [](cro::Entity e, float)
            {
                //briefly shows error message before returning to old string
                auto oldStr = e.getComponent<cro::Text>().getString();
                e.getComponent<cro::Callback>().setUserData<std::pair<float, cro::String>>(2.f, oldStr);
                e.getComponent<cro::Callback>().active = true;

                auto msg = "This key cannot be used.";
                e.getComponent<cro::Text>().setString(msg);
            };
        m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        return;
    }


    auto& keys = m_sharedData.inputBinding.keys;
    if (auto result = std::find(keys.begin(), keys.end(), key); result != keys.end())
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::InfoString;
        cmd.action = [key](cro::Entity e, float)
        {
            //briefly shows error message before returning to old string
            auto oldStr = e.getComponent<cro::Text>().getString();
            e.getComponent<cro::Callback>().setUserData<std::pair<float, cro::String>>(2.f, oldStr);
            e.getComponent<cro::Callback>().active = true;

            auto msg = cro::Keyboard::keyString(key);
            msg += " is already bound";
            e.getComponent<cro::Text>().setString(msg);
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

    //update the info text
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::InfoString;
    cmd.action = [key](cro::Entity e, float)
    {
        if (key != SDLK_ESCAPE
            && key != SDLK_BACKSPACE)
        {
            e.getComponent<cro::Text>().setString("Set to (" + cro::Keyboard::keyString(key) + ")");
        }
        else
        {
            e.getComponent<cro::Text>().setString(" ");
        }

        //reset any existing callback so that it doesn't timeout
        //and set the wrong string
        e.getComponent<cro::Callback>().active = false;
    };
    m_scene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    

    //auto index = m_bindingIndex;
    m_bindingIndex = -1;
}

void OptionsState::buildScene()
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

                assertDeviceIndex();
                refreshDeviceLabel();
                refreshControllerList();
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(m_viewScale * cro::Util::Easing::easeOutQuint(currTime));
            if (currTime == 0)
            {
                applyWebsock();
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
    spriteSheet.loadFromFile("assets/golf/sprites/options_v2.spt", m_sharedData.sharedResources->textures);

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
    const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //icons for paging tabs
    static constexpr float IconOffset = 30.f;
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconOffset, 338.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString("<" + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]));
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (lastInput == LastInput::Keyboard)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::Text>().setString("<" + cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::PrevClub]));
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgSize.x - IconOffset, 338.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) + ">");
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (lastInput == LastInput::Keyboard)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[InputBinding::NextClub]) + ">");
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ IconOffset, 338.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lb");
    entity.addComponent<cro::SpriteAnimation>().play(1);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            if (lastInput != LastInput::Keyboard)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::SpriteAnimation>().play(lastInput);
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bgSize.x - IconOffset, 338.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("rb");
    entity.addComponent<cro::SpriteAnimation>().play(1);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
        {
            if (lastInput != LastInput::Keyboard)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                e.getComponent<cro::SpriteAnimation>().play(lastInput);
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::White); e.getComponent<cro::AudioEmitter>().play(); });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });

    m_menuSounds.loadFromFile("assets/golf/sound/menu.xas", m_sharedData.sharedResources->audio);
    m_audioEnts[AudioID::Accept] = m_scene.createEntity();
    m_audioEnts[AudioID::Accept].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("accept");
    m_audioEnts[AudioID::Back] = m_scene.createEntity();
    m_audioEnts[AudioID::Back].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("back");

    //stash this so we use the sprite from the correct texture...
    auto tabSprite = spriteSheet.getSprite("tab_bar");
    auto settingsBar = spriteSheet.getSprite("set_bar");
    auto achBar = spriteSheet.getSprite("ach_bar");
    auto statBar = spriteSheet.getSprite("stat_bar");
    auto tabHighlight = spriteSheet.getSprite("tab_highlight");
    auto settingsBground = spriteSheet.getSprite("settings");
    spriteSheet.loadFromFile("assets/golf/sprites/options.spt", m_sharedData.sharedResources->textures);


    //video options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(PanelPosition);
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 86.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("audio_video");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto videoEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    entity.addComponent<PageButtons>();
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
    entity.addComponent<PageButtons>();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    createButtons(entity, MenuID::Controls, selectedID, unselectedID, spriteSheet);
    controlEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto controlButtonEnt = entity;

    //controls tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = tabSprite;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 51.f, bgSize.y - bounds.height, TabBarDepth });
    controlButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto hideControls = [controlEnt]() mutable
    {
        controlEnt.getComponent<cro::Transform>().setPosition(HiddenPosition);
    };
    auto showControls = [controlEnt]() mutable
    {
        controlEnt.getComponent<cro::Transform>().setPosition(PanelPosition);
    };


    //settings page
    static constexpr float HorizontalOffset = 42.f;
    static constexpr float VerticalOffset = 70.f; //hack for bigger background - we ought to be adding this to PanelPosition.y

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(HiddenPosition);
    entity.getComponent<cro::Transform>().setOrigin({ 0.f, 86.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = settingsBground;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto settingsEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition + glm::vec3(0.f, 86.f, 0.f));  //ffs, do this properly
    entity.addComponent<PageButtons>();
    createButtons(entity, MenuID::Settings, selectedID, unselectedID, spriteSheet);
    settingsEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto settingsButtonEnt = entity;

    //settings tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = settingsBar;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 51.f, (bgSize.y - bounds.height), TabBarDepth });
    settingsButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto hideSettings = [settingsEnt]() mutable
        {
            settingsEnt.getComponent<cro::Transform>().setPosition(HiddenPosition);
        };
    auto showSettings = [settingsEnt]() mutable
        {
            settingsEnt.getComponent<cro::Transform>().setPosition(PanelPosition);
        };



    //achievements
    bounds = spriteSheet.getSprite("input").getTextureBounds();
    const glm::uvec2 bufferSize(bounds.width + (HorizontalOffset * 2.f), bounds.height + VerticalOffset);

    m_achievementBuffer.create(bufferSize.x, bufferSize.y, false);
    m_achievementBuffer.clear(cro::Colour::Green);
    m_achievementBuffer.display();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(HiddenPosition);
    entity.getComponent<cro::Transform>().setOrigin({ HorizontalOffset - 2.f, VerticalOffset, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_achievementBuffer.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto achEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<PageButtons>();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    createButtons(entity, MenuID::Achievements, selectedID, unselectedID, spriteSheet);
    achEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto achButtonEnt = entity;

    //achievement tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = achBar;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 49.f + HorizontalOffset, (bgSize.y - bounds.height) + VerticalOffset, TabBarDepth });
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
    entity.getComponent<cro::Transform>().setOrigin({ HorizontalOffset - 2.f, VerticalOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_statsBuffer.getTexture());
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto statsEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<PageButtons>();
    entity.addComponent<cro::Transform>().setPosition(-PanelPosition);
    createButtons(entity, MenuID::Stats, selectedID, unselectedID, spriteSheet);
    statsEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto statsButtonEnt = entity;

    //stats tab
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = statBar;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 49.f + HorizontalOffset, (bgSize.y - bounds.height) + VerticalOffset, TabBarDepth });
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
        ent.getComponent<cro::Callback>().setUserData<float>(0.1f);
        ent.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            
            //sometimes 1 frame isn't enough...
            auto& t = e.getComponent<cro::Callback>().getUserData<float>();
            t -= dt;
            if (t < 0)
            {
                m_scene.destroyEntity(e);
            }
        };
    };

    //tab callback functions
    m_tabFunctions[0] = [&, showVideo, hideControls, hideSettings, hideAchievements, hideStats, refreshView]() mutable
    {
        //hide other tabs
        hideControls();
        hideSettings();
        hideAchievements();
        hideStats();

        //show video tab
        showVideo();
        uiSystem.setActiveGroup(MenuID::Video);
        uiSystem.selectByIndex(TabController);

        //refresh visible objects for one frame
        refreshView();

        //stop any controller input from scrolling the current page
        std::fill(m_controllerScrollAxes.begin(), m_controllerScrollAxes.end(), 0);
        resetScroll();
        m_currentTabFunction = 0;
    };

    m_tabFunctions[1] = [&, hideVideo, showControls, hideSettings, hideAchievements, hideStats, refreshView]() mutable
    {
        //hide other tabs
        hideVideo();
        hideSettings();
        hideAchievements();
        hideStats();

        //reset position for control button ent
        showControls();

        refreshView();

        uiSystem.setActiveGroup(MenuID::Controls);
        uiSystem.selectByIndex(TabSettings);

        std::fill(m_controllerScrollAxes.begin(), m_controllerScrollAxes.end(), 0);
        resetScroll();
        m_currentTabFunction = 1;
    };

    m_tabFunctions[2] = [&, hideVideo, hideControls, showSettings, hideAchievements, hideStats, refreshView]() mutable
        {
            //hide other tabs
            hideVideo();
            hideControls();
            hideAchievements();
            hideStats();

            //show settings tab
            showSettings();
            uiSystem.setActiveGroup(MenuID::Settings);
            uiSystem.selectByIndex(TabAchievements);

            //refresh visible objects for one frame
            refreshView();

            //stop any controller input from scrolling the current page
            std::fill(m_controllerScrollAxes.begin(), m_controllerScrollAxes.end(), 0);
            resetScroll();
            m_currentTabFunction = 2;
        };

    m_tabFunctions[3] = [&, hideVideo, hideControls, hideSettings, showAchievements, hideStats, refreshView]() mutable
    {
        //hide other tabs
        hideVideo();
        hideControls();
        hideSettings();
        hideStats();

        //show achievment tab
        showAchievements();

        refreshView();

        uiSystem.setActiveGroup(MenuID::Achievements);
        uiSystem.selectByIndex(TabStats);

        std::fill(m_controllerScrollAxes.begin(), m_controllerScrollAxes.end(), 0);
        resetScroll();
        m_currentTabFunction = 3;
    };

    m_tabFunctions[4] = [&, hideVideo, hideControls, hideSettings, hideAchievements, showStats, refreshView]() mutable
    {
        //hide other tabs
        hideVideo();
        hideControls();
        hideSettings();
        hideAchievements();

        //show stats tab
        showStats();

        refreshView();

        uiSystem.setActiveGroup(MenuID::Stats);
        uiSystem.selectByIndex(TabAV);

        std::fill(m_controllerScrollAxes.begin(), m_controllerScrollAxes.end(), 0);
        resetScroll();
        m_currentTabFunction = 4;
    };


    const std::array<glm::vec3, TabID::Count> TabPositions =
    {
        glm::vec3(67.f, 325.f, TabBarDepth + HighlightOffset),
        glm::vec3(144.f, 325.f, TabBarDepth + HighlightOffset),
        glm::vec3(221.f, 325.f, TabBarDepth + HighlightOffset),
        glm::vec3(298.f, 325.f, TabBarDepth + HighlightOffset),
        glm::vec3(375.f, 325.f, TabBarDepth + HighlightOffset)
    };

    auto createTab = [&, spriteSelectedID, spriteUnselectedID](cro::Entity parent, std::size_t index, std::int32_t menuID, std::size_t selectionIndex)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(TabPositions[index]);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = tabHighlight;
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

        parent.getComponent<PageButtons>().tabs[index] = ent;
        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };
    

    entity = createTab(videoButtonEnt, 1, MenuID::Video, TabController);
    entity.getComponent<cro::Transform>().move(videoEnt.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::UIInput>().setNextIndex(TabSettings, AVMixerRight);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowCredits);
    entity = createTab(videoButtonEnt, 2, MenuID::Video, TabSettings);
    entity.getComponent<cro::Transform>().move(videoEnt.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, AVVolumeDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowApply);    
    entity = createTab(videoButtonEnt, 3, MenuID::Video, TabAchievements);
    entity.getComponent<cro::Transform>().move(videoEnt.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, AVVolumeDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabSettings, WindowApply);
    entity = createTab(videoButtonEnt, 4, MenuID::Video, TabStats);
    entity.getComponent<cro::Transform>().move(videoEnt.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, AVVolumeUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowClose);



    entity = createTab(controlButtonEnt, 0, MenuID::Controls, TabAV);
    entity.getComponent<cro::UIInput>().setNextIndex(TabSettings, CtrlLayout);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowAdvanced);
    entity = createTab(controlButtonEnt, 2, MenuID::Controls, TabSettings);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, CtrlLB);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, WindowApply);
    entity = createTab(controlButtonEnt, 3, MenuID::Controls, TabAchievements);
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, CtrlLB);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabSettings, WindowApply);
    entity = createTab(controlButtonEnt, 4, MenuID::Controls, TabStats);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, CtrlLB);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, WindowClose);



    entity = createTab(settingsButtonEnt, 0, MenuID::Settings, TabAV);
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, SettWebsockEnable);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowCredits);
    entity = createTab(settingsButtonEnt, 1, MenuID::Settings, TabController);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, SettWebPortDown);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, WindowAdvanced);
    entity = createTab(settingsButtonEnt, 3, MenuID::Settings, TabAchievements);
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, SettFlagUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowApply);
    entity = createTab(settingsButtonEnt, 4, MenuID::Settings, TabStats);
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, SettFlagLabelUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, WindowClose);



    entity = createTab(achButtonEnt, 0, MenuID::Achievements, TabAV);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabStats, WindowAdvanced);
    entity = createTab(achButtonEnt, 1, MenuID::Achievements, TabController);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabSettings, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, WindowAdvanced);
    entity = createTab(achButtonEnt, 2, MenuID::Achievements, TabSettings);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabStats, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowAdvanced);
    entity = createTab(achButtonEnt, 4, MenuID::Achievements, TabStats);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, ScrollUp);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabSettings, WindowClose);



    entity = createTab(statsButtonEnt, 0, MenuID::Stats, TabAV);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabController, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, WindowCredits);
    entity = createTab(statsButtonEnt, 1, MenuID::Stats, TabController);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabSettings, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, WindowAdvanced);
    entity = createTab(statsButtonEnt, 2, MenuID::Stats, TabSettings);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabAchievements, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabController, WindowAdvanced);
    entity = createTab(statsButtonEnt, 3, MenuID::Stats, TabAchievements);
    entity.getComponent<cro::Transform>().move(glm::vec3(HorizontalOffset - 2.f, VerticalOffset, 0.f));
    entity.getComponent<cro::UIInput>().setNextIndex(TabAV, WindowApply);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabSettings, WindowApply);

    
    auto createTabTip = [&](std::int32_t tipID, glm::vec3 position)
    {
        auto bounds = tabHighlight.getTextureBounds();

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
    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Settings, TabPositions[2]).getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Achievements, TabPositions[3]).getComponent<cro::Transform>());
    bgEnt.getComponent<cro::Transform>().addChild(createTabTip(ToolTipID::Stats, TabPositions[4]).getComponent<cro::Transform>());

    buildAVMenu(videoEnt, spriteSheet);
    buildControlMenu(controlEnt, controlButtonEnt, spriteSheet);
    buildSettingsMenu(settingsEnt, spriteSheet);
    buildAchievementsMenu(achEnt, spriteSheet);
    buildStatsMenu(statsEnt, spriteSheet);

    //dummy input to consume events during animation
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);


    //tool tips for options
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
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
    m_tooltips[ToolTipID::Pixel] = createToolTip("Increases the pixel size\nfor an extra pixelated look");
    m_tooltips[ToolTipID::VertSnap] = createToolTip("Snaps vertices to the nearest\nwhole pixel for a retro \'wobble\'.\nMay cause z-fighting.");
    m_tooltips[ToolTipID::Beacon] = createToolTip("Shows a beacon to indicate flag position\nat far distances.");
    m_tooltips[ToolTipID::BeaconColour] = createToolTip("Display colour of the beacon.");
    m_tooltips[ToolTipID::Units] = createToolTip("Select to display in yards/feet or\nunselect to display in metres/cm");
    m_tooltips[ToolTipID::MouseSpeed] = createToolTip("1.00");
    m_tooltips[ToolTipID::PuttingPower] = createToolTip("Decreases the difficulty when\nputting, at the cost of XP");
    m_tooltips[ToolTipID::Video] = createToolTip("Sound & Video Settings");
    m_tooltips[ToolTipID::Controls] = createToolTip("Controls");
    m_tooltips[ToolTipID::Settings] = createToolTip("Game Settings");
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
    titleEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 260.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(titleFont).setString("Audio & Video");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());

    const auto createLabel = [&](glm::vec2 pos, const std::string& str)
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
    auto audioLabel = createLabel(glm::vec2((bgBounds.width / 2.f) - 133.f, 242.f), MixerLabels[mixerChannelIndex]);
    centreText(audioLabel);
    audioLabel.addComponent<cro::Callback>().active = true;
    audioLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::CustomMusic);
        };


    m_deviceLabel = createLabel(glm::vec2(244.f, 242.f), " ");
    m_deviceLabel.getComponent<cro::Drawable2D>().setCroppingArea(LabelCrop);
    m_deviceLabel.addComponent<cro::Callback>().active = true;
    m_deviceLabel.getComponent<cro::Callback>().setUserData<TextScrollData>();
    m_deviceLabel.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
        {
            static constexpr float ScrollPadding = -14.f;
            static constexpr float ScrollSpeed = 0.05f;

            auto& [dir, currTime, maxScroll] = e.getComponent<cro::Callback>().getUserData<TextScrollData>();
            currTime += dt;
            if (currTime > ScrollSpeed)
            {
                currTime -= ScrollSpeed;
                auto o = e.getComponent<cro::Transform>().getOrigin();
                if (dir == 0)
                {
                    o.x += 1.f;
                    if (o.x >= (maxScroll - (LabelCrop.width + ScrollPadding)))
                    {
                        dir = 1;
                    }
                }
                else
                {
                    o.x -= 1.f;
                    if (o.x <= ScrollPadding)
                    {
                        dir = 0;
                    }
                }
                auto bounds = LabelCrop;
                bounds.left = o.x;
                e.getComponent<cro::Transform>().setOrigin(o);
                e.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
            }
        };
    refreshDeviceLabel();

    //antialiasing label
    auto aliasLabel = createLabel(glm::vec2(12.f, 217.f), "Antialiasing");
    aliasLabel.addComponent<cro::Callback>().active = true;
    aliasLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::AA);
        };

    auto aaLabel = createLabel(glm::vec2(136.f, 217.f), AAStrings[AAIndexMap[m_sharedData.multisamples]]);
    centreText(aaLabel);

    //FOV label
    auto fovLabel = createLabel(glm::vec2(12.f, 201.f), "FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));

    //resolution label
    auto resLabel = createLabel(glm::vec2(12.f, 185.f), "Resolution");
    //centreText(resLabel);

    //resolution value text
    resLabel = createLabel(glm::vec2(136.f, 185.f), m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
    centreText(resLabel);
    resolutionLabel = resLabel; //global static used by callback to update display when window is toggled FS

    //pixel scale label
    auto pixelLabel = createLabel(glm::vec2(12.f, 169.f), "Pixel Scaling      (Default: OFF)");
    pixelLabel.addComponent<cro::Callback>().active = true;
    pixelLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::Pixel);
        };

    //vertex snap label
    auto vertLabel = createLabel(glm::vec2(12.f, 153.f), "Vertex Snap      (Requires Restart)");
    vertLabel.addComponent<cro::Callback>().active = true;
    vertLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::VertSnap);
        };

    //full screen label
    createLabel(glm::vec2(12.f, 137.f), "Full Screen");

    //fs exclusive
    createLabel(glm::vec2(12.f, 121.f), "Full Screen       - Exclusive Mode");

    //vsync label
    createLabel(glm::vec2(12.f, 105.f), "Enable VSync");

    //beacon label
    auto beaconLabel = createLabel(glm::vec2(12.f, 89.f), "Flag Beacon");
    beaconLabel.addComponent<cro::Callback>().active = true;
    beaconLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::Beacon);
        };


    //lens flare label
    createLabel(glm::vec2(12.f, 73.f), "Lens Flare");


    //remote content label
    createLabel(glm::vec2(12.f, 57.f), "Low Quality        Light Mapping");


    //ball trail label
    createLabel({ 204.f, 217.f }, "Enable       Ball Trail");

    //putting assist
    auto puttingEnt = createLabel({ 204.f, 201.f }, "Enable       Putting Assist");
    puttingEnt.addComponent<cro::Callback>().active = true;
    puttingEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::PuttingPower);
        };


    //post process label
    //createLabel({ 204.f, 185.f }, "Post FX");

    //measurements
    auto measureLabel = createLabel({ 204.f, 185.f }, "Units         Imperial Measurements");
    measureLabel.addComponent<cro::Callback>().active = true;
    measureLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::Units);
        };

    //grid transparency
    createLabel({ 204.f, 169.f }, "Grid Amount");

    //tree quality
    auto treeLabel = createLabel({ 204.f, 153.f }, "Tree Quality");
    treeLabel.addComponent<cro::Callback>().active = true;
    treeLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            updateToolTip(e, ToolTipID::NeedsRestart);
        };


    //shadow quality
    createLabel({ 204.f, 137.f }, "Shadow Quality");

    //crowd density
    createLabel({ 204, 121.f }, "Crowd Density");


    createLabel({ 204.f, 105.f }, "Use Larger Power Bar");
    createLabel({ 204.f, 89.f }, "Use High Contrast Power Bar");
    createLabel({ 204.f, 73.f }, "Use Decimated Power Bar");
    createLabel({ 204.f, 57.f }, "Use Decimalised Distances");
    createLabel({ 204.f, 41.f }, "Fixed Range Putter");


    auto createSlider = [&](glm::vec2 position, float width = 142.f)
        {
            auto entity = m_scene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(position);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slider");
            auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
            entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), /*std::floor*/(bounds.height / 2.f), -TextOffset });


            auto userData = SliderData(position, width);
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
    auto volSlider = createSlider(glm::vec2(143.f, 239.f), 65.f);

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
    constexpr auto fovPos = glm::vec2(99.f, 198.f);
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
    auto transPos = glm::vec2(281.f, 166.f);
    auto transSlider = createSlider(transPos);
    auto ud = SliderData(transPos, 90.f);
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
    helpEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, -9.f, 0.1f });
    helpEnt.addComponent<cro::Drawable2D>();
    helpEnt.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    helpEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    helpEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(helpEnt.getComponent<cro::Transform>());


    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto selectedID = uiSystem.addCallback([&, helpEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            const auto& str = e.getLabel();

            auto pos = helpEnt.getComponent<cro::Transform>().getPosition();
            if (str.find('\n') != std::string::npos)
            {
                pos.y = 9.f;
            }
            else
            {
                pos.y = 4.f;
            }
            helpEnt.getComponent<cro::Transform>().setPosition(pos);
            helpEnt.getComponent<cro::Text>().setString(str);

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
    auto entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 188.f, 233.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVMixerLeft);
    entity.getComponent<cro::UIInput>().setNextIndex(AVMixerRight, AVAAL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVDeviceUp, WindowCredits);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, audioLabel](cro::Entity e, cro::ButtonEvent evt) mutable
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 87.f, 233.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVMixerRight);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVolumeDown, AVAAL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVMixerLeft, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, audioLabel](cro::Entity e, cro::ButtonEvent evt) mutable
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
    entity = createHighlight(glm::vec2(125.f, 233.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVolumeDown);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVolumeUp, AVAAL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVMixerRight, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderDownCallback(m_audioEnts[AudioID::Accept]));

    //audio up
    entity = createHighlight(glm::vec2(215.f, 233.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVolumeUp);
    entity.getComponent<cro::UIInput>().setNextIndex(AVDeviceDown, AVAAR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVolumeDown, TabAchievements);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderUpCallback(m_audioEnts[AudioID::Back]));


    //audio device down
    entity = createHighlight(glm::vec2(231.f, 233.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVDeviceDown);
    entity.getComponent<cro::UIInput>().setNextIndex(AVDeviceUp, AVTrail);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVolumeUp, TabAchievements);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) 
        {
            if (activated(evt))
            {
                const auto size = cro::AudioDevice::getDeviceList().size();
                if (size != 0)
                {
                    audioDeviceIndex = (audioDeviceIndex + (size - 1)) % size;
                    applyAudioDevice();
                }
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });


    //audio device up
    entity = createHighlight(glm::vec2(373.f, 233.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVDeviceUp);
    entity.getComponent<cro::UIInput>().setNextIndex(AVMixerLeft, AVTrailR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVDeviceDown, TabStats);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                const auto size = cro::AudioDevice::getDeviceList().size();
                if (size != 0)
                {
                    audioDeviceIndex = (audioDeviceIndex + 1) % size;
                    applyAudioDevice();
                }
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });


    //aa down
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 208.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 208.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVAAR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTrail, AVFOVR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVAAL, AVVolumeUp);
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 192.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 192.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 176.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVResolutionL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVResolutionR, AVPixelScale);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVUnits, AVFOVL);
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 176.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVResolutionR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVUnits, AVBeaconR);
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
    entity = createHighlight(glm::vec2(81.f, 160.f));
    entity.setLabel("Makes everything extra pixelated. For the most dedicated retro enthusiast.\nShortcut: +/- on numpad. (Default: OFF)");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPixelScale);
    entity.getComponent<cro::UIInput>().setNextIndex(AVGridL, AVVertSnap);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVGridR, AVResolutionL);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 162.f, HighlightOffset));
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
    entity = createHighlight(glm::vec2(81.f, 144.f));
    entity.setLabel("Usually used in conjunction with Pixel Scaling.\nMay cause z-fighting. (Default: OFF)");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVertSnap);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTreeL, AVFullScreen);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTreeR, AVPixelScale);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 146.f, HighlightOffset));
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
    entity = createHighlight(glm::vec2(81.f, 128.f));
    entity.setLabel("Toggles Full Screen based on the the Full Screen mode.\nPress Apply for changes to take effect. (Shortcut: Alt+Enter)");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVFullScreen);
    entity.getComponent<cro::UIInput>().setNextIndex(AVShadowL, AVFullScreenMode);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVShadowR, AVVertSnap);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 130.f, HighlightOffset));
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

    //fs mode check box
    entity = createHighlight(glm::vec2(81.f, 112.f));
    entity.setLabel("When enabled the game takes exclusive full screen at the current resolution.\nWhen disabled full screen runs in a borderless window at desktop resolution.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVFullScreenMode);
    entity.getComponent<cro::UIInput>().setNextIndex(AVCrowdL, AVVSync);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVCrowdR, AVFullScreen);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    //m_videoSettings.fullScreen = !m_videoSettings.fullScreen;
                    cro::App::getWindow().setExclusiveFullscreen(!cro::App::getWindow().getExclusiveFullscreen());
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //full screen mode centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 114.f, HighlightOffset));
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
            float scale = cro::App::getWindow().getExclusiveFullscreen() ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());




    //vsync checkbox
    entity = createHighlight(glm::vec2(81.f, 96.f));
    if (Social::isSteamdeck(false))
    {
        entity.setLabel("Requires Disable Frame Limit set on Steam Deck.\nHigher frame rates may reduce battery life.");
    }
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVVSync);
    entity.getComponent<cro::UIInput>().setNextIndex(AVLargePower, AVBeacon);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVLargePower, AVFullScreenMode);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    bool vsync = cro::App::getWindow().getVsyncEnabled();
                    cro::App::getWindow().setVsyncEnabled(!vsync);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //vsync checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 98.f, HighlightOffset));
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
            float scale = cro::App::getWindow().getVsyncEnabled() ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //beacon check box
    entity = createHighlight(glm::vec2(81.f, 80.f));
    entity.setLabel("Displays a beacon on the course at the pin position.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVBeacon);
    entity.getComponent<cro::UIInput>().setNextIndex(AVBeaconL, AVLensFlare);   
    entity.getComponent<cro::UIInput>().setPrevIndex(AVContrastPower, AVVSync);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 82.f, HighlightOffset));
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(99.f, 82.f, HighlightOffset));
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

    entity = createHighlight(glm::vec2(113.f, 80.f));
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

    entity = createHighlight(glm::vec2(182.f, 80.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVBeaconR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVContrastPower, TabController);
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
    auto colourPos = glm::vec2(131.f, 86.f);
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


    //lens flare highlight
    entity = createHighlight(glm::vec2(81.f, 64.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVLensFlare);
    entity.getComponent<cro::UIInput>().setNextIndex(AVDecPower, AVLightMap);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVDecPower, AVBeacon);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.useLensFlare = !m_sharedData.useLensFlare;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //lens flare centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 66.f, HighlightOffset));
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
            float scale = m_sharedData.useLensFlare ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //lightmap highlight
    entity = createHighlight(glm::vec2(81.f, 48.f));
    entity.setLabel("Reduce lightmap VRAM usage at night, at the cost of quality");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVLightMap);
    entity.getComponent<cro::UIInput>().setNextIndex(AVDecDist, AVTextToSpeech);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVDecDist, AVLensFlare);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.lightmapQuality = m_sharedData.lightmapQuality == 0 ? 1 : 0;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //lightmap centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 50.f, HighlightOffset));
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
            float scale = m_sharedData.lightmapQuality ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //ball trail checkbox
    entity = createHighlight(glm::vec2(246.f, 208.f));
    entity.setLabel("Draws a trail behind the ball when it is in flight.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTrail);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTrailL, AVPuttAss);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVAAR, AVDeviceDown);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 210.f, HighlightOffset));
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
    entity.addComponent<cro::Transform>().setPosition({ 350.f, 218.f, HighlightOffset });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(m_sharedData.trailBeaconColour ? "Beacon" : "White");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto trailLabel = entity;

    //prev/next trail colour
    entity = createHighlight(glm::vec2(313.f, 208.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTrailL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTrailR, AVGridL);
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
    entity = createHighlight(glm::vec2(378.f, 208.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTrailR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVAAL, AVGridR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTrailL, AVDeviceUp);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = cbID;


    //putting assist toggle
    entity = createHighlight(glm::vec2(246.f, 192.f));
    entity.setLabel("Displays a power estimation when putting at the cost of awarded XP.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVPuttAss);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFOVL, AVUnits);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 194.f, HighlightOffset));
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



    //imperial measurements
    entity = createHighlight(glm::vec2(246.f, 176.f));
    entity.setLabel("Enables imperial measurements in yards, feet and inches.\nDefault is metric (metres and centimetres).");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVUnits);
    entity.getComponent<cro::UIInput>().setNextIndex(AVResolutionL, AVGridL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVResolutionR, AVPuttAss);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 178.f, HighlightOffset));
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
    entity = createHighlight(glm::vec2(263.f, 160.f));
    entity.setLabel("Sets the transparency of the grid on the green when putting.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVGridL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVGridR, AVTreeL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVPixelScale, AVUnits);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.gridTransparency = std::max(0.f, m_sharedData.gridTransparency - 0.1f);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity = createHighlight(glm::vec2(378.f, 160.f));
    entity.setLabel("Sets the transparency of the grid on the green when putting.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVGridR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVPixelScale, AVTreeR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVGridL, AVBeaconR);
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
    auto treeQualityText = createLabel({ 325.f, 153.f }, TreeLabels[m_sharedData.treeQuality]);
    centreText(treeQualityText);

    //prev / next tree quality
    entity = createHighlight(glm::vec2(286.f, 144.f));
    entity.setLabel("Switch between billboard and 3D trees.\nClassic trees are applied when the game is loaded.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTreeL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTreeR, AVShadowL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVertSnap, AVGridL);
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

    entity = createHighlight(glm::vec2(355.f, 144.f));
    entity.setLabel("Switch between billboard and 3D trees.\nClassic trees are applied when the game is loaded.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTreeR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVertSnap, AVShadowR);
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


    auto shadowQualityText = createLabel({ 325.f, 137.f }, ShadowText[m_sharedData.shadowQuality]);
    centreText(shadowQualityText);

    //prev / next shadow quality
    auto shadowUpdate = [&, shadowQualityText]() mutable
        {
            shadowQualityText.getComponent<cro::Text>().setString(ShadowText[m_sharedData.shadowQuality]);
            centreText(shadowQualityText);
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

            auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
            msg->type = SystemEvent::ShadowQualityChanged;
        };

    const auto shadowChangedUp = uiSystem.addCallback([&, shadowUpdate](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.shadowQuality = (m_sharedData.shadowQuality + 1) % 3;
                    shadowUpdate();
                }
            });
    const auto shadowChangedDown = uiSystem.addCallback([&, shadowUpdate](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.shadowQuality = (m_sharedData.shadowQuality + 2) % 3;
                shadowUpdate();
            }
        });

    entity = createHighlight(glm::vec2(286.f, 128.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVShadowL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVShadowR, AVCrowdL);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVFullScreen, AVTreeL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = shadowChangedDown;

    entity = createHighlight(glm::vec2(355.f, 128.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVShadowR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFullScreen, AVCrowdR);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVShadowL, AVTreeR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = shadowChangedUp;

    //prev/next crowd density
    static const std::array CrowdLabels =
    {
        std::string("Low"),
        std::string("Normal"),
        std::string("High"),
        std::string("Extreme"),
        std::string("None"),
    };
    auto crowdDensityText = createLabel({ 325.f, 121.f }, CrowdLabels[m_sharedData.crowdDensity]);
    centreText(crowdDensityText);

    const auto crowdChanged = uiSystem.addCallback([&, crowdDensityText](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto id = e.getComponent<cro::UIInput>().getSelectionIndex();

                if (id == AVCrowdL)
                {
                    m_sharedData.crowdDensity = (m_sharedData.crowdDensity + (CrowdDensityCount - 1)) % CrowdDensityCount;
                }
                else
                {
                    m_sharedData.crowdDensity = (m_sharedData.crowdDensity + 1) % CrowdDensityCount;
                }
                crowdDensityText.getComponent<cro::Text>().setString(CrowdLabels[m_sharedData.crowdDensity]);
                centreText(crowdDensityText);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                msg->type = SystemEvent::CrowdDensityChanged;
            }
        });

    entity = createHighlight(glm::vec2(286.f, 112.f));
    entity.setLabel("Very high density crowds may cause a drop in performance.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVCrowdL);
    entity.getComponent<cro::UIInput>().setNextIndex(AVCrowdR, AVLargePower);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVFullScreenMode, AVShadowL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = crowdChanged;

    entity = createHighlight(glm::vec2(355.f, 112.f));
    entity.setLabel("Very high density crowds may cause a drop in performance.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVCrowdR);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFullScreenMode, AVLargePower);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVCrowdL, AVShadowR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = crowdChanged;


    //highlight for large power bar
    entity = createHighlight(glm::vec2(355.f, 96.f));
    entity.setLabel("Use larger power bar at the bottom of the screen.\nDefaults to ON on Steam Deck.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVLargePower);
    entity.getComponent<cro::UIInput>().setNextIndex(AVVSync, AVContrastPower);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVVSync, AVCrowdR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.useLargePowerBar = !m_sharedData.useLargePowerBar;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //centre for large power bar
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(357.f, 98.f, HighlightOffset));
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
            float scale = m_sharedData.useLargePowerBar ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //highlight for high contrast bar
    entity = createHighlight(glm::vec2(355.f, 80.f));
    entity.setLabel("Use high contrast colours for power bar.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVContrastPower);
    entity.getComponent<cro::UIInput>().setNextIndex(AVBeacon, AVDecPower);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVBeaconR, AVLargePower);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.useContrastPowerBar = !m_sharedData.useContrastPowerBar;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //centre for high contrast bar
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(357.f, 82.f, HighlightOffset));
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
            float scale = m_sharedData.useContrastPowerBar ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //highlight for decimated power bar
    entity = createHighlight(glm::vec2(355.f, 64.f));
    entity.setLabel("Divide the power bar into 10 segments instead of quarters and eighths");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVDecPower);
    entity.getComponent<cro::UIInput>().setNextIndex(AVLensFlare, AVDecDist);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVLensFlare, AVContrastPower);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.decimatePowerBar = !m_sharedData.decimatePowerBar;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //centre for decimated power bar
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(357.f, 66.f, HighlightOffset));
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
            float scale = m_sharedData.decimatePowerBar ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    
    //highlight for decimated distance
    entity = createHighlight(glm::vec2(355.f, 48.f));
    entity.setLabel("All distances appear as decimalised values eg 0.5m instead of smaller denominations\nsuch as 50cm");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVDecDist);
    entity.getComponent<cro::UIInput>().setNextIndex(AVLightMap, AVFixedPutter);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVLightMap, AVDecPower);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.decimateDistance = !m_sharedData.decimateDistance;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //centre for decimated distance
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(357.f, 50.f, HighlightOffset));
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
            float scale = m_sharedData.decimateDistance ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //highlight for fixed putter
    entity = createHighlight(glm::vec2(355.f, 32.f));
    entity.setLabel("Fixes the range of the putter at 10m/33ft.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVFixedPutter);
    entity.getComponent<cro::UIInput>().setNextIndex(AVTextToSpeech, WindowClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVTextToSpeech, AVDecDist);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.fixedPuttingRange = !m_sharedData.fixedPuttingRange;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });

    //centre for fixed putter
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(357.f, 34.f, HighlightOffset));
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
            float scale = m_sharedData.fixedPuttingRange ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //text to speech
    createLabel({ 12.f, 41.f }, "Use Chat TTS");

    //checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 34.f, HighlightOffset));
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
            float scale = m_sharedData.useTTS ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createHighlight(glm::vec2(81.f, 32.f));
#ifdef __linux__
    entity.setLabel("Enables text-to-speech for in-game chat text.");
#else
    entity.setLabel("Enables text-to-speech for in-game chat text.\nUse the Speech option in Windows Control Panel to set advanced options.");
#endif
    entity.getComponent<cro::UIInput>().setSelectionIndex(AVTextToSpeech);
    entity.getComponent<cro::UIInput>().setNextIndex(AVFixedPutter, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(AVFixedPutter, AVLightMap);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.useTTS = !m_sharedData.useTTS;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }
            });
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void OptionsState::buildControlMenu(cro::Entity parent, cro::Entity buttonEnt, const cro::SpriteSheet& spriteSheet)
{
    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();

    const auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    const auto& uiFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, 174.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(uiFont).setString("Controls");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());

    //displays help/info at bottom
    auto infoEnt = m_scene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, -74.f, TextOffset });
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(infoFont).setAlignment(cro::Text::Alignment::Centre);
    if (!Social::isSteamdeck(false)) //layout ent hasn't been assigned yet...
    {
        infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
    }
    else
    {
        infoEnt.getComponent<cro::Text>().setString(" ");
    }
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    infoEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    infoEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::InfoString;
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

    //displays list of controllers if there's more than one connected
    m_controllerInfoEnt = m_scene.createEntity();
    m_controllerInfoEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, -14.f, TextOffset });
    m_controllerInfoEnt.addComponent<cro::Drawable2D>();
    m_controllerInfoEnt.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
    m_controllerInfoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    //m_controllerInfoEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    m_controllerInfoEnt.addComponent<cro::Callback>().setUserData<const float>(parentBounds.width / 2.f);
    m_controllerInfoEnt.getComponent<cro::Callback>().active = true;
    m_controllerInfoEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            if (m_layoutEnt.getComponent<cro::SpriteAnimation>().id == LayoutID::Keyboard)
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            }
            else
            {
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            }
        };
    parent.getComponent<cro::Transform>().addChild(m_controllerInfoEnt.getComponent<cro::Transform>());
    refreshControllerList();

    //displays keybind info at top
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
        [&, infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            if (m_layoutEnt.getComponent<cro::SpriteAnimation>().id == LayoutID::Keyboard)
            {
                infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
            }
            else
            {
                infoEnt.getComponent<cro::Text>().setString(" ");
            }

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    auto highlightSelectID = uiSystem.addCallback(
        [&](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);

            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });
    auto highlightUnselectID = uiSystem.addCallback(
        [&](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    cro::SpriteSheet controllerSheet;
    controllerSheet.loadFromFile("assets/golf/sprites/control_layout.spt", m_sharedData.sharedResources->textures);
    auto layoutEnt = m_scene.createEntity();
    layoutEnt.addComponent<cro::Transform>().setPosition(glm::vec3((parentBounds.width / 2.f) + 2.f, 4.f, TextOffset));
    layoutEnt.addComponent<cro::Drawable2D>();
    layoutEnt.addComponent<cro::Sprite>() = controllerSheet.getSprite("controls");
    layoutEnt.addComponent<cro::SpriteAnimation>().play(LayoutID::Keyboard);
    parent.getComponent<cro::Transform>().addChild(layoutEnt.getComponent<cro::Transform>());
    m_layoutEnt = layoutEnt;


    if (!Social::isSteamdeck(false))
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition({ 45.f, -8.f, 0.1f });
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        e.getComponent<cro::Text>().setString("F2: Toggle Ball Labels\nF3: Toggle UI\nF4: Toggle Chat Window");
        e.getComponent<cro::Text>().setFillColour(TextGoldColour);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour, 4);
        e.getComponent<cro::Text>().setFillColour(TextGoldColour, 23);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour, 27);
        e.getComponent<cro::Text>().setFillColour(TextGoldColour, 37);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour, 41);
        e.getComponent<cro::Text>().setVerticalSpacing(-3.f);

        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function =
            [&](cro::Entity f, float)
            {
                const float scale = m_layoutEnt.getComponent<cro::SpriteAnimation>().id == LayoutID::Keyboard ? 1.f : 0.f;
                f.getComponent<cro::Transform>().setScale(glm::vec2(scale));
            };
        layoutEnt.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    }


    auto createHighlight = [&, infoEnt, layoutEnt](glm::vec2 position, std::int32_t keyIndex) mutable
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, HighlightOffset));
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = controllerSheet.getSprite("key_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = highlightSelectID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectID;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
            [&, infoEnt, keyIndex](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if ((evt.type == SDL_KEYUP || evt.type == SDL_MOUSEBUTTONUP)
                    && activated(evt))
                {
                    m_updatingKeybind = true;

                    infoEnt.getComponent<cro::Text>().setString("Press a Key (Escape to Cancel)");

                    m_bindingIndex = keyIndex;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

        layoutEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        
        if (keyIndex > -1)
        {
            bindingEnts[keyIndex] = entity;
        }

        //add child ent which renders the actual keybind text
        auto textEnt = m_scene.createEntity();
        textEnt.addComponent<cro::Transform>().setPosition(glm::vec3(bounds.width / 2.f, 10.f, -0.01f));
        textEnt.addComponent<cro::Drawable2D>();
        textEnt.addComponent<cro::Text>(uiFont).setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[keyIndex]));
        textEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
        textEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        textEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        textEnt.addComponent<cro::Callback>().active = true;
        textEnt.getComponent<cro::Callback>().setUserData<std::int32_t>(m_sharedData.inputBinding.keys[keyIndex]);
        textEnt.getComponent<cro::Callback>().function =
            [&, keyIndex](cro::Entity e, float)
            {
                auto& lastKey = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
                if (lastKey != m_sharedData.inputBinding.keys[keyIndex])
                {
                    //update string
                    e.getComponent<cro::Text>().setString(cro::Keyboard::keyString(m_sharedData.inputBinding.keys[keyIndex]));
                    lastKey = m_sharedData.inputBinding.keys[keyIndex];
                }
            };
        entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());

        return entity;
    };

    glm::vec2 highlightPos = glm::vec2(1.f, 28.f);
    //hm, not in the same order as the enum...
    const std::array KeyIndices =
    {
        InputBinding::Action,
        InputBinding::CancelShot,
        InputBinding::SpinMenu,
        InputBinding::EmoteMenu,
        InputBinding::Down,
        InputBinding::Up,
        InputBinding::Right,
        InputBinding::Left,
        InputBinding::NextClub,
        InputBinding::PrevClub,
    };

    std::int32_t selectionIndex = CtrlA;
    for (auto i : KeyIndices)
    {
        auto h = createHighlight(highlightPos, i);
        h.getComponent<cro::UIInput>().setSelectionIndex(selectionIndex);
        if (selectionIndex < CtrlUp)
        {
            h.getComponent<cro::UIInput>().setNextIndex(CtrlLookL, selectionIndex - 1);
            h.getComponent<cro::UIInput>().setPrevIndex(CtrlLookR, selectionIndex + 1);
        }
        else
        {
            h.getComponent<cro::UIInput>().setNextIndex(CtrlThreshL, selectionIndex - 1);
            h.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshR, selectionIndex + 1);
        }

        selectionIndex++;
        highlightPos.y += 12.f;
    }

    bindingEnts[KeyIndices[0]].getComponent<cro::UIInput>().setNextIndex(CtrlLookL, WindowApply);
    bindingEnts[KeyIndices.back()].getComponent<cro::UIInput>().setPrevIndex(CtrlLayout, TabAchievements);


    //switch between keyboard + cotroller view
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(12.f, 138.f, TextOffset));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = controllerSheet.getSprite("selection_buttons");
    entity.addComponent<cro::SpriteAnimation>();
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto selectionEnt = entity;

    //highlight for keyboard/controller display
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(94.f, -1.f, TextOffset));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = controllerSheet.getSprite("controller_button");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLayout);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlLB, CtrlThreshR);
    entity.getComponent<cro::UIInput>().setPrevIndex(TabAV, TabAV);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = highlightSelectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected]= highlightUnselectID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, selectionEnt, infoEnt, buttonEnt](cro::Entity ent, cro::ButtonEvent evt) mutable
        {
            //buttonEnt has the component containing the entities for taband window buttons

            if (activated(evt))
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                if (m_layoutEnt.getComponent<cro::SpriteAnimation>().id == LayoutID::Keyboard)
                {
                    //switch to controller
                    for (auto e : bindingEnts)
                    {
                        e.getComponent<cro::UIInput>().enabled = false;
                        e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        e.getComponent<cro::UIInput>().setGroup(MenuID::Dummy);
                    }

                    auto controllerIndex = evt.type == SDL_CONTROLLERBUTTONDOWN ?
                        cro::GameController::controllerID(evt.cbutton.which) : 0;

                    std::size_t i = Social::isSteamdeck(false) ? LayoutID::Deck
                        : cro::GameController::getControllerCount() == 0 ? LayoutID::XBox :
                        hasPSLayout(controllerIndex) ? LayoutID::PS : LayoutID::XBox;
                    m_layoutEnt.getComponent<cro::SpriteAnimation>().play(i);
                    selectionEnt.getComponent<cro::SpriteAnimation>().play(1);

                    ent.getComponent<cro::Transform>().setPosition(glm::vec2(-1.f));
                    ent.getComponent<cro::UIInput>().setNextIndex(CtrlThreshL, CtrlThreshL);

                    infoEnt.getComponent<cro::Text>().setString(" ");

                    updateControlIndices(false);

                    buttonEnt.getComponent<PageButtons>().tabs[TabID::Achievements].getComponent<cro::UIInput>().setNextIndex(TabStats, WindowApply);
                    buttonEnt.getComponent<PageButtons>().tabs[TabID::Stats].getComponent<cro::UIInput>().setNextIndex(TabAV, WindowClose);

                    buttonEnt.getComponent<PageButtons>().buttons[ButtonID::Apply].getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, TabAchievements);
                    buttonEnt.getComponent<PageButtons>().buttons[ButtonID::Quit].getComponent<cro::UIInput>().setPrevIndex(WindowApply, TabStats);
                }
                else
                {
                    //switch to keyboard
                    for (auto e : bindingEnts)
                    {
                        e.getComponent<cro::UIInput>().enabled = true;
                        e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        e.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
                    }
                    m_layoutEnt.getComponent<cro::SpriteAnimation>().play(LayoutID::Keyboard);
                    selectionEnt.getComponent<cro::SpriteAnimation>().play(0);

                    ent.getComponent<cro::Transform>().setPosition(glm::vec2(94.f, -1.f));
                    ent.getComponent<cro::UIInput>().setNextIndex(CtrlRight, CtrlLookR);

                    infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");

                    updateControlIndices(true);

                    //TODO move these to above func
                    buttonEnt.getComponent<PageButtons>().tabs[TabID::Achievements].getComponent<cro::UIInput>().setNextIndex(TabStats, CtrlLB);
                    buttonEnt.getComponent<PageButtons>().tabs[TabID::Stats].getComponent<cro::UIInput>().setNextIndex(TabAV, CtrlLB);

                    buttonEnt.getComponent<PageButtons>().buttons[ButtonID::Apply].getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, CtrlA);
                    buttonEnt.getComponent<PageButtons>().buttons[ButtonID::Quit].getComponent<cro::UIInput>().setPrevIndex(WindowApply, CtrlA);
                }
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true; //refresh the display
            }
        });
    selectionEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //default to controller layout on deck
    if (Social::isSteamdeck()
        || cro::GameController::getControllerCount() != 0)
    {
        for (auto e : bindingEnts)
        {
            e.getComponent<cro::UIInput>().enabled = false;
            e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            e.getComponent<cro::UIInput>().setGroup(MenuID::Dummy);
        }

        m_layoutEnt.getComponent<cro::SpriteAnimation>().play(Social::isSteamdeck(false) ? LayoutID::Deck : LayoutID::XBox);
        selectionEnt.getComponent<cro::SpriteAnimation>().play(1);

        entity.getComponent<cro::Transform>().setPosition(glm::vec2(-1.f));
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlLookR, CtrlLookR);

        buttonEnt.getComponent<PageButtons>().tabs[TabID::Achievements].getComponent<cro::UIInput>().setNextIndex(TabStats, WindowApply);
        buttonEnt.getComponent<PageButtons>().tabs[TabID::Stats].getComponent<cro::UIInput>().setNextIndex(TabAV, WindowClose);

        buttonEnt.getComponent<PageButtons>().buttons[ButtonID::Apply].getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, TabAchievements);
        buttonEnt.getComponent<PageButtons>().buttons[ButtonID::Quit].getComponent<cro::UIInput>().setPrevIndex(WindowApply, TabStats);

        //we haven't finished the menu yet so this needs to be delayed
        auto te = m_scene.createEntity();
        te.addComponent<cro::Callback>().active = true;
        te.getComponent<cro::Callback>().function =
            [&](cro::Entity ent, float)
            {
                updateControlIndices(false);
                ent.getComponent<cro::Callback>().active = false;
                m_scene.destroyEntity(ent);
            };
    }



    //contronller sliders
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
    ss << "Thumbstick Deadzone: " << cro::GameController::LeftThumbDeadZone.getOffset();
    auto deadzoneText = createText(glm::vec2(20.f, 128.f), ss.str());


    //createText(glm::vec2(20.f, 156.f), "Zoom Map:\nDrone View:\nFree Cam:\nRotate Camera:");
    //createText(glm::vec2(100.f, 156.f), "R3 or F6\nDPad Up or 1\nDPad Down or 3\nRight Stick or 4/5");

    std::stringstream st;
    st.precision(2);
    st << "Look Sensitivity: " << m_sharedData.mouseSpeed;
    auto mouseText = createText(glm::vec2(20.f, 96.f), st.str());
    createText(glm::vec2(26.f, 63.f), "Invert X");
    createText(glm::vec2(26.f, 47.f), "Invert Y");
    createText(glm::vec2(26.f, 31.f), "Mouse Action");
    createText(glm::vec2(112.f, 63.f), "Use Vibration");
    createText(glm::vec2(112.f, 47.f), "Hold For Power");
    createText(glm::vec2(112.f, 31.f), "Enable Swingput");

    //TODO don't duplicate these as they already exist in the AV menu
    auto selectedID = uiSystem.addCallback([&, infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            if (e.getLabel().empty())
            {
                if (m_layoutEnt.getComponent<cro::SpriteAnimation>().id != LayoutID::Keyboard)
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
            st << "Look Sensitivity: " << m_sharedData.mouseSpeed;

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

    static const auto MinDeadZone = -3000;// -(cro::GameController::LeftThumbDeadZone / 2);
    static const auto MaxDeadzone = 24000;// ((cro::GameController::AxisMax - 100) - cro::GameController::LeftThumbDeadZone.size);// / 2;

    auto deadzoneSlider = createSlider({ 29.f, 109.f });
    deadzoneSlider.getComponent<cro::Callback>().getUserData<SliderData>().onActivate =
        [&, deadzoneText](float distance) mutable
    {
        cro::GameController::LeftThumbDeadZone.setOffset(MinDeadZone + std::int16_t(static_cast<float>(MaxDeadzone - MinDeadZone) * distance));

        std::stringstream ss;
        ss.precision(2);
        ss << "Thumbstick Deadzone: " << cro::GameController::LeftThumbDeadZone.getOffset();
        deadzoneText.getComponent<cro::Text>().setString(ss.str());
    };
    deadzoneSlider.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        const auto& [pos, width, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
        const float amount = static_cast<float>(cro::GameController::LeftThumbDeadZone.getOffset() - MinDeadZone) / (MaxDeadzone - MinDeadZone);

        e.getComponent<cro::Transform>().setPosition({ pos.x + (width * amount), pos.y });
    };

    //mouse speed slider
    createSlider(glm::vec2(29.f, 78.f));//77


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

    static constexpr std::int16_t DeadzoneStep = 1000;

    //deadzone down
    entity = createSquareHighlight(glm::vec2(11.f, 103.f));
    entity.setLabel("Adjusts the minimum movement of the thumbstick before input\nis accepted by the game: larger values require more movement.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlThreshL);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlThreshR, CtrlLookL);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlRight, CtrlLayout);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, deadzoneText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto val = std::max(MinDeadZone, cro::GameController::LeftThumbDeadZone.getOffset() - DeadzoneStep);
                val /= DeadzoneStep;
                val *= DeadzoneStep;
                cro::GameController::LeftThumbDeadZone.setOffset(val);

                std::stringstream ss;
                ss.precision(2);
                ss << "Thumbstick Deadzone: " << cro::GameController::LeftThumbDeadZone.getOffset();
                deadzoneText.getComponent<cro::Text>().setString(ss.str());

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_controlEntities[ControlButtons::ThreshL] = entity;

    //deadzone up
    entity = createSquareHighlight(glm::vec2(178.f, 103.f));
    entity.setLabel("Adjusts the minimum movement of the thumbstick before input\nis accepted by the game: larger values require more movement.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlThreshR);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlRight, CtrlLookR);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshL, CtrlLayout);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, deadzoneText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto val = std::min(MaxDeadzone, cro::GameController::LeftThumbDeadZone.getOffset() + DeadzoneStep);
                val /= DeadzoneStep;
                val *= DeadzoneStep;
                cro::GameController::LeftThumbDeadZone.setOffset(val);

                std::stringstream ss;
                ss.precision(2);
                ss << "Thumbstick Deadzone: " << cro::GameController::LeftThumbDeadZone.getOffset();
                deadzoneText.getComponent<cro::Text>().setString(ss.str());

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_controlEntities[ControlButtons::ThreshR] = entity;

    //mouse speed down
    entity = createSquareHighlight(glm::vec2(11.f, 72.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLookL);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlLookR, CtrlInvX);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlY, CtrlThreshL);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, mouseText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.mouseSpeed = std::max(ConstVal::MinMouseSpeed, m_sharedData.mouseSpeed - 0.1f);

                std::stringstream st;
                st.precision(2);
                st << "Look Sensitivity: " << m_sharedData.mouseSpeed;
                mouseText.getComponent<cro::Text>().setString(st.str());

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_controlEntities[ControlButtons::LookL] = entity;

    //mouse speed up
    entity = createSquareHighlight(glm::vec2(178.f, 72.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlLookR);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlY, CtrlVib);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLookL, CtrlThreshR);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, mouseText](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.mouseSpeed = std::min(ConstVal::MaxMouseSpeed, m_sharedData.mouseSpeed + 0.1f);

                std::stringstream st;
                st.precision(2);
                st << "Look Sensitivity " << m_sharedData.mouseSpeed;
                mouseText.getComponent<cro::Text>().setString(st.str());

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_controlEntities[ControlButtons::LookR] = entity;


    //invert X
    entity = createSquareHighlight(glm::vec2(11.f, 54.f));
    entity.setLabel("Invert the controller X axis when in camera mode");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlInvX);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlVib, CtrlInvY);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlX, CtrlLookL);
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
    m_controlEntities[ControlButtons::InvX] = entity;

    //centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(13.f, 56.f, HighlightOffset));
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
    entity = createSquareHighlight(glm::vec2(97.f, 54.f));
    entity.setLabel("Enable or disable controller vibration");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlVib);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlX, CtrlAltPower);
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
    m_controlEntities[ControlButtons::Vib] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(99.f, 56.f, HighlightOffset));
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
    entity = createSquareHighlight(glm::vec2(97.f, 38.f));
    entity.setLabel("When enabled press and hold Action to select stroke power\nelse use the default 3-tap method when disabled");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlAltPower);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlA, CtrlSwg);
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
    m_controlEntities[ControlButtons::AltPower] = entity;
    
    //y'know if we defined these first we could capture them and update them in the button callback...
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(99.f, 40.f, HighlightOffset));
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


    //swingput enable
    entity = createSquareHighlight(glm::vec2(97.f, 22.f));
    entity.setLabel("With either trigger held, pull back on a thumbstick to charge the power.\nPush forward on the stick to make your shot. Timing is important!");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlSwg);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlMouseAction, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlMouseAction, CtrlAltPower);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.useSwingput = !m_sharedData.useSwingput;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(99.f, 24.f, HighlightOffset));
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
            float scale = m_sharedData.useSwingput ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //invert Y
    entity = createSquareHighlight(glm::vec2(11.f, 38.f));
    entity.setLabel("Invert the controller Y axis when in camera mode");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlInvY);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlAltPower, CtrlMouseAction);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlA, CtrlInvX);
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
    m_controlEntities[ControlButtons::InvY] = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(13.f, 40.f, HighlightOffset));
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


    //enable mouse click for action button
    entity = createSquareHighlight(glm::vec2(11.f, 22.f));
    entity.setLabel("Allow using Left Mouse Button as Action Button");
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlMouseAction);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlSwg, CtrlReset);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlSwg, CtrlInvY);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.useMouseAction = !m_sharedData.useMouseAction;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(13.f, 24.f, HighlightOffset));
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
            float scale = m_sharedData.useMouseAction ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //reset to defaults
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(32.f, -1.f, HighlightOffset));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("small_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().setSelectionIndex(CtrlReset);
    entity.getComponent<cro::UIInput>().setNextIndex(CtrlSwg, WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(CtrlA, CtrlMouseAction);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            if (!Social::isSteamdeck(false))
            {
                infoEnt.getComponent<cro::Text>().setString("Reset All Keybinds To Their Default Values");
            }

            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = uiSystem.addCallback(
        [&,infoEnt](cro::Entity e) mutable
        {
            if (m_layoutEnt.getComponent<cro::SpriteAnimation>().id == LayoutID::Keyboard)
            {
                infoEnt.getComponent<cro::Text>().setString("Click On A Keybind To Change It");
            }
            else
            {
                infoEnt.getComponent<cro::Text>().setString(" ");
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

void OptionsState::buildSettingsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto bgBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //title
    const auto& titleFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 260.f, TextOffset });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(titleFont).setString("Game Settings");
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    titleEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());

    //info text
    auto helpEnt = m_scene.createEntity();
    helpEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, -9.f, 0.1f });
    helpEnt.addComponent<cro::Drawable2D>();
    helpEnt.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
    helpEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    helpEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    parent.getComponent<cro::Transform>().addChild(helpEnt.getComponent<cro::Transform>());

    const auto createLabel = [&](glm::vec2 pos, const std::string& str)
        {
            auto ent = m_scene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(glm::vec3(pos, TextOffset));
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
            ent.getComponent<cro::Text>().setString(str);
            ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            return ent;
        };
    auto portLabel = createLabel(glm::vec2(166.f, 232.f), std::to_string(m_sharedData.webPort));
    centreText(portLabel);

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    const auto selectedID = uiSystem.addCallback([&, helpEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            const auto& str = e.getLabel();

            auto pos = helpEnt.getComponent<cro::Transform>().getPosition();
            if (str.find('\n') != std::string::npos)
            {
                pos.y = 17.f;
            }
            else
            {
                pos.y = 11.f;
            }
            helpEnt.getComponent<cro::Transform>().setPosition(pos);
            helpEnt.getComponent<cro::Text>().setString(str);

            m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });
    const auto unselectedID = uiSystem.addCallback([helpEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);

            helpEnt.getComponent<cro::Text>().setString(" ");
        });

    const auto createHighlight = [&](glm::vec2 pos)
        {
            auto ent = m_scene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(pos);
            ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("square_highlight");
            ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            ent.addComponent<cro::UIInput>().setGroup(MenuID::Settings);
            auto bounds = ent.getComponent<cro::Sprite>().getTextureBounds();
            ent.getComponent<cro::UIInput>().area = bounds;
            //ent.getComponent<cro::UIInput>().area.width *= 6.f;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

            ent.addComponent<cro::Callback>().function = HighlightAnimationCallback();
            ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f, -HighlightOffset });
            ent.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

            parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

            return ent;
        };

    //port select down
    auto entity = createHighlight(glm::vec2(140.f, 223.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettWebPortDown);
    entity.getComponent<cro::UIInput>().setNextIndex(SettWebPortUp, ResetCareer);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettWebsockEnable, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, portLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.webPort = std::max(m_sharedData.webPort - 1, WebSock::MinPort);
                portLabel.getComponent<cro::Text>().setString(std::to_string(m_sharedData.webPort));
                centreText(portLabel);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //port select up
    entity = createHighlight(glm::vec2(182.f, 223.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettWebPortUp);
    entity.getComponent<cro::UIInput>().setNextIndex(SettFlagDown, ResetCareer);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettWebPortDown, TabController);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&, portLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.webPort = std::min(m_sharedData.webPort + 1, WebSock::MaxPort);
                portLabel.getComponent<cro::Text>().setString(std::to_string(m_sharedData.webPort));
                centreText(portLabel);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });


    //enable socket
    entity = createHighlight(glm::vec2(12.f, 223.f));
    entity.setLabel("See https://github.com/fallahn/svs for more info");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettWebsockEnable);
    entity.getComponent<cro::UIInput>().setNextIndex(SettWebPortDown, SettCSVLog);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettFlagLabelUp, TabAV);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.webSocket = !m_sharedData.webSocket;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                
                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    //checkbox centre
    const auto createCheckbox = [&](glm::vec2 pos)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(glm::vec3(pos, HighlightOffset));
        ent.addComponent<cro::Drawable2D>().getVertexData() =
        {
            cro::Vertex2D(glm::vec2(0.f, 7.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(7.f), TextGoldColour),
            cro::Vertex2D(glm::vec2(7.f, 0.f), TextGoldColour)
        };
        ent.getComponent<cro::Drawable2D>().updateLocalBounds();
        ent.addComponent<cro::Callback>().active = true;
        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };
    
    entity = createCheckbox(glm::vec2(14.f, 225.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            float scale = m_sharedData.webSocket ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };


    //CSV log
    entity = createHighlight(glm::vec2(12.f, 207.f));
    entity.setLabel("Log Stroke Play scores to a CSV file in Free Play\nFiles are saved to your User Directory");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettCSVLog);
    entity.getComponent<cro::UIInput>().setNextIndex(SettInGameChat, SettInGameChat);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettWebsockEnable, SettWebsockEnable);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.logCSV = !m_sharedData.logCSV;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 209.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = m_sharedData.logCSV ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };


    //chat disable
    entity = createHighlight(glm::vec2(12.f, 191.f));
    entity.setLabel("Disable text chat in online games");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettInGameChat);
    entity.getComponent<cro::UIInput>().setNextIndex(SettLogChat, SettLogChat);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettCSVLog, SettCSVLog);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.blockChat = !m_sharedData.blockChat;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 193.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = m_sharedData.blockChat ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };


    //chat log
    entity = createHighlight(glm::vec2(12.f, 175.f));
    entity.setLabel("Logs in-game and lobby chat to a text file in the install directory");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettLogChat);
    entity.getComponent<cro::UIInput>().setNextIndex(SettRemoteContent, SettRemoteContent);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettInGameChat, SettInGameChat);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.logChat = !m_sharedData.logChat;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 177.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            const float scale = m_sharedData.logChat ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };


    //remote content
    entity = createHighlight(glm::vec2(12.f, 159.f));
    entity.setLabel("Allow downloading remote content such as Workshop items from other\nplayers when (re)connecting to a network game");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettRemoteContent);
    entity.getComponent<cro::UIInput>().setNextIndex(SettShowRival, SettShowRival);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettLogChat, SettLogChat);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.remoteContent = !m_sharedData.remoteContent;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 161.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            float scale = m_sharedData.remoteContent ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };


    //show monthly rival
    entity = createHighlight(glm::vec2(12.f, 143.f));
    entity.setLabel("Display scores of the current monthly leader on the scoreboard in Free Play\nif they are available. Takes effect on round start.");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettShowRival);
    entity.getComponent<cro::UIInput>().setNextIndex(SettPost, SettPuttFollow);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettPostR, SettRemoteContent);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.showRival = !m_sharedData.showRival;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 145.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            float scale = m_sharedData.showRival ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };



    //follow cam when putting
    entity = createHighlight(glm::vec2(12.f, 127.f));
    entity.setLabel("Use the ball follower camera instead of the top-down camera\nin the preview window top-left of the UI when putting");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettPuttFollow);
    entity.getComponent<cro::UIInput>().setNextIndex(SettPost, SettPuttZoom);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettPostR, SettShowRival);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.puttFollowCam = !m_sharedData.puttFollowCam;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 129.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            float scale = m_sharedData.puttFollowCam ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };


    //zoom follow cam
    entity = createHighlight(glm::vec2(12.f, 111.f));
    entity.setLabel("Automatically zoom the follower camera when the ball is in motion");
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettPuttZoom);
    entity.getComponent<cro::UIInput>().setNextIndex(SettPost, ResetCareer);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettPostR, SettPuttFollow);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                m_sharedData.zoomFollowCam = !m_sharedData.zoomFollowCam;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
            }
        });

    entity = createCheckbox(glm::vec2(14.f, 113.f));
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
        {
            float scale = m_sharedData.zoomFollowCam ? 1.f : 0.f;
            e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };



    //available flag textures
    const std::string flagDir = "assets/golf/images/flags/";

    auto flags = cro::FileSystem::listFiles(flagDir);
    std::vector<std::pair<std::string, std::string>> mappedFlags;

    if (auto pos = std::find(flags.begin(), flags.end(), "flag.png");
        pos != flags.end() && pos != flags.begin())
    {
        std::iter_swap(flags.begin(), pos);
    }

    //hmm this hasn't checked it's a valid size yet...
    if (m_sharedData.flagPath.empty()
        /*|| !cro::FileSystem::fileExists(m_sharedData.flagPath)*/)
    {
        m_sharedData.flagPath = flagDir + flags[0];
    }

    for (const auto& flag : flags)
    {
        mappedFlags.emplace_back(std::make_pair(flagDir, flag));
    }

    const auto userDir = Content::getUserContentPath(Content::UserContent::Flag);
    const auto userFlags = cro::FileSystem::listDirectories(userDir);
    const auto MaxUser = m_flagTextures.getLayerCount() - flags.size();
    for (auto i = 0u; i < MaxUser && i < userFlags.size(); ++i)
    {
        const auto files = cro::FileSystem::listFiles(userDir + userFlags[i]);
        for (auto j = 0u; j < files.size(); ++j)
        {
            //just grab the first png we find
            if (cro::FileSystem::getFileExtension(files[j]) == ".png")
            {
                mappedFlags.emplace_back(std::make_pair(userDir + userFlags[i] + "/", files[j]));
                break;
            }
        }
    }


    std::uint32_t loadedCount = 0;
    cro::Image tmp(true);
    m_flagTextures.create(FlagTextureSize.x, FlagTextureSize.y);

    for (const auto& [path, flag] : mappedFlags)
    {
        const auto fullPath = path + flag;
        if (tmp.loadFromFile(fullPath)
            && tmp.getSize() == FlagTextureSize)
        {
            if (flag == cro::FileSystem::getFileName(m_sharedData.flagPath))
            {
                m_flagIndex = loadedCount;
            }
            m_flagPaths.push_back(fullPath);
            m_flagTextures.insertLayer(tmp, loadedCount++);
        }

        if (loadedCount == m_flagTextures.getLayerCount())
        {
            break;
        }
    }

    //this just checks that the path was found, else resets it
    //if the file from the path loaded in the config is missing
    if (m_flagIndex == 0)
    {
        m_sharedData.flagPath = m_flagPaths[0];
    }


    if (loadedCount)
    {
        auto& shader = m_sharedData.sharedResources->shaders.get(ShaderID::FlagPreview);

        const auto shaderID = shader.getGLHandle();
        const auto uniformID = shader.getUniformID("u_textureIndex");
        const auto setTexture = 
            [shaderID, uniformID](std::uint32_t idx)
            {
                glUseProgram(shaderID);
                glUniform1f(uniformID, static_cast<float>(idx));
            };
        setTexture(m_flagIndex);

        const glm::vec2 PreviewSize(FlagTextureSize / 4u);
        const float u = 320.f / FlagTextureSize.x;

        //flag preview
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 263.f, 144.f, 0.2f });
        entity.addComponent<cro::Drawable2D>().setShader(&shader);
        entity.getComponent<cro::Drawable2D>().setTexture(m_flagTextures, FlagTextureSize);
        entity.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f, PreviewSize.y), glm::vec2(0.f, 1.f)),
                cro::Vertex2D(glm::vec2(0.f), glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(PreviewSize), glm::vec2(u, 1.f)),
                cro::Vertex2D(glm::vec2(PreviewSize.x, 0.f), glm::vec2(u, 0.f))
            });
        auto flagEnt = entity;
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //flag number
        const auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ (PreviewSize.x / 2.f) - 8.f, 40.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(largeFont).setCharacterSize(UITextSize * 3);
        entity.getComponent<cro::Text>().setString("1");
        flagEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        auto numEnt = entity;

        //flag text
        auto flagLabel = createLabel(glm::vec2(271.f, 232.f), std::to_string(m_flagIndex + 1));
        centreText(flagLabel);

        //flag print text
        auto printLabel = createLabel({362.f, 232.f}, "None");

        auto setNumberColour =
            [&, numEnt, printLabel]() mutable
            {
                if (m_sharedData.flagText == 0)
                {
                    numEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                else
                {
                    numEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    numEnt.getComponent<cro::Text>().setFillColour(m_sharedData.flagText == 1 ? CD32::Colours[CD32::Black] : CD32::Colours[CD32::BeigeLight]);

                    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
                }

                switch (m_sharedData.flagText)
                {
                default:
                case 0:
                    printLabel.getComponent<cro::Text>().setString("None");
                    break;
                case 1: 
                    printLabel.getComponent<cro::Text>().setString("Black");
                    break;
                case 2: 
                    printLabel.getComponent<cro::Text>().setString("White");
                    break;
                }
                centreText(printLabel);
            };
        setNumberColour();



        //flag down
        entity = createHighlight({ 245.f, 223.f });
        entity.getComponent<cro::UIInput>().setSelectionIndex(SettFlagDown);
        entity.getComponent<cro::UIInput>().setNextIndex(SettFlagUp, SettPost);
        entity.getComponent<cro::UIInput>().setPrevIndex(SettWebPortUp, TabAchievements);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
            [&, flagLabel, setTexture](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_flagIndex = (m_flagIndex + (m_flagPaths.size() - 1)) % m_flagPaths.size();
                    m_sharedData.flagPath = m_flagPaths[m_flagIndex];
                    setTexture(m_flagIndex);

                    flagLabel.getComponent<cro::Text>().setString(std::to_string(m_flagIndex + 1));
                    centreText(flagLabel);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });

        //flag up
        entity = createHighlight({ 287.f, 223.f });
        entity.getComponent<cro::UIInput>().setSelectionIndex(SettFlagUp);
        entity.getComponent<cro::UIInput>().setNextIndex(SettFlagLabelDown, SettPostL);
        entity.getComponent<cro::UIInput>().setPrevIndex(SettFlagDown, TabAchievements);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
            [&, flagLabel, setTexture](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_flagIndex = (m_flagIndex + 1) % m_flagPaths.size();
                    m_sharedData.flagPath = m_flagPaths[m_flagIndex];
                    setTexture(m_flagIndex);

                    flagLabel.getComponent<cro::Text>().setString(std::to_string(m_flagIndex + 1));
                    centreText(flagLabel);

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });


        //flag label down
        entity = createHighlight({ 336.f, 223.f });
        entity.getComponent<cro::UIInput>().setSelectionIndex(SettFlagLabelDown);
        entity.getComponent<cro::UIInput>().setNextIndex(SettFlagLabelUp, SettPostL);
        entity.getComponent<cro::UIInput>().setPrevIndex(SettFlagUp, TabStats);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
            [&, setNumberColour, printLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.flagText = (m_sharedData.flagText + 2) % 3;
                    setNumberColour();

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });


        //flag print up
        entity = createHighlight({ 378.f, 223.f });
        entity.getComponent<cro::UIInput>().setSelectionIndex(SettFlagLabelUp);
        entity.getComponent<cro::UIInput>().setNextIndex(SettWebsockEnable, SettPostR);
        entity.getComponent<cro::UIInput>().setPrevIndex(SettFlagLabelDown, TabStats);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
            [&, setNumberColour](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.flagText = (m_sharedData.flagText + 1) % 3;
                    setNumberColour();
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });

#ifdef USE_GNS
        createLabel({ 210.f, 218.f }, "Find more flags in the Steam Workshop");
#endif
    }



    //post process label
    createLabel({ 204.f, 120.f }, "Post FX");


    //post process checkbox
    entity = createHighlight(glm::vec2(246.f, 111.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettPost);
    entity.getComponent<cro::UIInput>().setNextIndex(SettPostL, ResetStats);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettPuttZoom, SettFlagDown);
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(248.f, 113.f, HighlightOffset));
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

    auto shaderLabel = createLabel(glm::vec2(325.f, 120.f), ShaderNames[m_sharedData.postProcessIndex]);
    centreText(shaderLabel);

    //prev/next post process
    entity = createHighlight(glm::vec2(263.f, 111.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettPostL);
    entity.getComponent<cro::UIInput>().setNextIndex(SettPostR, ResetStats);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettPost, SettFlagUp);
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

    entity = createHighlight(glm::vec2(378.f, 111.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(SettPostR);
    entity.getComponent<cro::UIInput>().setNextIndex(SettPuttZoom, WindowClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(SettPostL, SettFlagLabelUp);
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







    auto createbuttonHighlight = [&](glm::vec2 pos, const std::string& sprName)
        {
            auto ent = m_scene.createEntity();
            ent.addComponent<cro::Transform>().setPosition(pos);
            ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            ent.addComponent<cro::Drawable2D>();
            ent.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprName);
            ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            ent.addComponent<cro::UIInput>().setGroup(MenuID::Settings);
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

    //reset career button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::round(bgBounds.width / 3.f), 32.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("reset_career");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createbuttonHighlight({ std::round(bgBounds.width / 3.f), 32.f }, "reset_button_highlight");
    entity.setLabel("Resets all Career progress and CPU opponents.\nPreviously awarded bonuses remain unlocked.");
    entity.getComponent<cro::Transform>().move(-entity.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::UIInput>().setSelectionIndex(ResetCareer);
    entity.getComponent<cro::UIInput>().setNextIndex(ResetStats, WindowAdvanced);
    entity.getComponent<cro::UIInput>().setPrevIndex(ResetStats, SettPuttZoom);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_sharedData.errorMessage = "reset_career";
                    requestStackPush(StateID::MessageOverlay);
                }
            });


    //reset profile button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ std::round((bgBounds.width / 3.f) * 2.f), 32.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("reset_button");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = createbuttonHighlight({ std::round((bgBounds.width / 3.f) * 2.f), 32.f }, "reset_button_highlight");
    entity.setLabel("WARNING! This will completely reset your profile including XP and current level!");
    entity.getComponent<cro::Transform>().move(-entity.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::UIInput>().setSelectionIndex(ResetStats);
    entity.getComponent<cro::UIInput>().setNextIndex(ResetCareer, WindowApply);
    entity.getComponent<cro::UIInput>().setPrevIndex(ResetCareer, SettPost);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    m_sharedData.errorMessage = "reset_profile";
                    requestStackPush(StateID::MessageOverlay);
                }
            });
}

void OptionsState::buildAchievementsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    //render details to buffer
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, 174.f + 70.f, TextOffset });
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

    const float ScrollTop = (bufferSize.y - textureRect.height);
    const float ScrollBottom = ScrollTop - (std::floor(ScrollTop / VerticalSpacing) * VerticalSpacing);
    const float ScrollHeight = ScrollTop - ScrollBottom;

    //scroll functions (kept here to hook mouse scroll callback)
    m_scrollFunctions[ScrollID::AchUp] = 
        [&, parent, ScrollTop, ScrollBottom](cro::Entity, const cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto& target = parent.getComponent<cro::Callback>().getUserData<float>();
                //if (target < ScrollTop)
                {
                    const auto dst = target + VerticalSpacing;
                    target = dst > ScrollTop ? ScrollBottom : dst;

                    parent.getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }

                if (evt.type == SDL_CONTROLLERBUTTONDOWN)
                {
                    m_scrollPresses[ScrollID::AchUp].pressed = true;
                }
            }
        };

    m_scrollFunctions[ScrollID::AchDown] = 
        [&, parent, ScrollBottom, ScrollTop](cro::Entity, const cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto& target = parent.getComponent<cro::Callback>().getUserData<float>();

                //if (target > VerticalSpacing)
                {
                    const auto dst = target - VerticalSpacing;
                    target = dst < ScrollBottom ? ScrollTop : dst;

                    parent.getComponent<cro::Callback>().active = true;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }

                if (evt.type == SDL_CONTROLLERBUTTONDOWN)
                {
                    m_scrollPresses[ScrollID::AchDown].pressed = true;
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

    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    //scrollbar
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cropping.width - 18.f, 7.f + bounds.height, TextOffset - 0.05f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, ScrollBarSize.y), ScrollBarColour),
            cro::Vertex2D(glm::vec2(0.f), ScrollBarColour),
            cro::Vertex2D(ScrollBarSize, ScrollBarColour),
            cro::Vertex2D(glm::vec2(ScrollBarSize.x, 0.f), ScrollBarColour)
        });
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    glm::vec2 sliderPos = entity.getComponent<cro::Transform>().getPosition();

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(sliderPos, TextOffset + 0.05f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("scrollbar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ /*std::floor*/(bounds.width / 2.f), std::floor(bounds.height / 2.f), -TextOffset });

    const auto offset = glm::vec2( entity.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::Transform>().move(offset);

    auto userData = SliderData(sliderPos + offset, ScrollBarSize.y - bounds.height);
    userData.onActivate = [ScrollHeight, ScrollBottom, parent](float sliderPosNorm) mutable
        {
            const float pos = std::floor(sliderPosNorm * ScrollHeight) + ScrollBottom;

            auto bounds = parent.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom = pos;
            parent.getComponent<cro::Sprite>().setTextureRect(bounds);
            parent.getComponent<cro::Callback>().setUserData<float>(pos);
        };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(userData);
    entity.getComponent<cro::Callback>().function =
        [ScrollBottom, ScrollHeight, parent](cro::Entity e, float)
        {
            auto scrollPos = parent.getComponent<cro::Sprite>().getTextureRect().bottom - ScrollBottom;

            const auto& [pos, height, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
            float amount = (scrollPos / ScrollHeight);

            e.getComponent<cro::Transform>().setPosition({ pos.x, pos.y + (height * amount)});
        };

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_scrollBars.push_back(entity);



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
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                //if (deactivated(evt))
                {
                    m_scrollPresses[ScrollID::AchUp].pressed = false;
                    m_scrollPresses[ScrollID::AchUp].pressedTimer = 0.f;
                    m_scrollPresses[ScrollID::AchUp].active = false;
                }
            });

    auto downHighlight = createHighlight({ cropping.width - 19.f, 7.f });
    downHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollDown);
    downHighlight.getComponent<cro::UIInput>().setNextIndex(WindowAdvanced, WindowClose);
    downHighlight.getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, ScrollUp);
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::AchDown]);
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                //if (deactivated(evt))
                {
                    m_scrollPresses[ScrollID::AchDown].pressed = false;
                    m_scrollPresses[ScrollID::AchDown].pressedTimer = 0.f;
                    m_scrollPresses[ScrollID::AchDown].active = false;
                }
            });
}

void OptionsState::buildStatsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ parentBounds.width / 2.f, 174.f + 70.f, TextOffset });
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

    const float ScrollTop = (bufferSize.y - textureRect.height);
    const float ScrollBottom = ScrollTop - (std::floor(ScrollTop / VerticalSpacing) * VerticalSpacing);
    const float ScrollHeight = ScrollTop - ScrollBottom;

    //scroll functions (kept here to hook mouse scroll callback)
    m_scrollFunctions[ScrollID::StatUp] = 
        [&, parent, ScrollTop, ScrollBottom](cro::Entity, const cro::ButtonEvent evt) mutable
    {
        if (activated(evt))
        {
            auto& target = parent.getComponent<cro::Callback>().getUserData<float>();
            //if (target < ScrollTop)
            {
                const auto dst = target + VerticalSpacing;
                target = dst > ScrollTop ? ScrollBottom : dst;

                parent.getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }

            if (evt.type == SDL_CONTROLLERBUTTONDOWN)
            {
                m_scrollPresses[ScrollID::StatUp].pressed = true;
            }
        }
    };

    m_scrollFunctions[ScrollID::StatDown] = 
        [&, parent, ScrollBottom, ScrollTop](cro::Entity, const cro::ButtonEvent evt) mutable
    {
        if (activated(evt))
        {
            auto& target = parent.getComponent<cro::Callback>().getUserData<float>();

            //if (target > VerticalSpacing)
            {
                const auto dst = target - VerticalSpacing;
                target = dst < ScrollBottom ? ScrollTop : dst;

                parent.getComponent<cro::Callback>().active = true;
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }

            if (evt.type == SDL_CONTROLLERBUTTONDOWN)
            {
                m_scrollPresses[ScrollID::StatDown].pressed = true;
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

    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();

    //scrollbar
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ cropping.width - 18.f, 7.f + bounds.height, TextOffset - 0.05f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(0.f, ScrollBarSize.y), ScrollBarColour),
            cro::Vertex2D(glm::vec2(0.f), ScrollBarColour),
            cro::Vertex2D(ScrollBarSize, ScrollBarColour),
            cro::Vertex2D(glm::vec2(ScrollBarSize.x, 0.f), ScrollBarColour)
        });
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    glm::vec2 sliderPos = entity.getComponent<cro::Transform>().getPosition();
    
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(sliderPos, TextOffset + 0.05f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("scrollbar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ /*std::floor*/(bounds.width / 2.f), std::floor(bounds.height / 2.f), -TextOffset });

    const auto offset = glm::vec2(entity.getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::Transform>().move(offset);

    auto userData = SliderData(sliderPos + offset, ScrollBarSize.y - bounds.height);
    userData.onActivate = [ScrollHeight, ScrollBottom, parent](float sliderPosNorm) mutable
        {
            const float pos = std::floor(sliderPosNorm * ScrollHeight) + ScrollBottom;

            auto bounds = parent.getComponent<cro::Sprite>().getTextureRect();
            bounds.bottom = pos;
            parent.getComponent<cro::Sprite>().setTextureRect(bounds);
            parent.getComponent<cro::Callback>().setUserData<float>(pos);
        };
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<SliderData>(userData);
    entity.getComponent<cro::Callback>().function =
        [ScrollBottom, ScrollHeight, parent](cro::Entity e, float)
        {
            auto scrollPos = parent.getComponent<cro::Sprite>().getTextureRect().bottom - ScrollBottom;

            const auto& [pos, height, _] = e.getComponent<cro::Callback>().getUserData<SliderData>();
            float amount = (scrollPos / ScrollHeight);

            e.getComponent<cro::Transform>().setPosition({ pos.x, pos.y + (height * amount) });
        };

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_scrollBars.push_back(entity);
    



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

    auto createHighlight = [&](glm::vec2 pos, const std::string& sprName)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(pos);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprName);
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

    auto upHighlight = createHighlight({ cropping.width - 19.f, cropping.height - 19.f }, "square_highlight");
    upHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollUp);
    upHighlight.getComponent<cro::UIInput>().setNextIndex(TabAV, ScrollDown);
    upHighlight.getComponent<cro::UIInput>().setPrevIndex(TabAchievements, TabAchievements);
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::StatUp]);
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                //if (deactivated(evt))
                {
                    m_scrollPresses[ScrollID::StatUp].pressed = false;
                    m_scrollPresses[ScrollID::StatUp].pressedTimer = 0.f;
                    m_scrollPresses[ScrollID::StatUp].active = false;
                }
            });


    auto downHighlight = createHighlight({ cropping.width - 19.f, 7.f }, "square_highlight");
    downHighlight.getComponent<cro::UIInput>().setSelectionIndex(ScrollDown);
    downHighlight.getComponent<cro::UIInput>().setNextIndex(WindowClose, WindowClose);
    downHighlight.getComponent<cro::UIInput>().setPrevIndex(ScrollUp, ScrollUp);
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::StatDown]);
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem.addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                //if (deactivated(evt))
                {
                    m_scrollPresses[ScrollID::StatDown].pressed = false;
                    m_scrollPresses[ScrollID::StatDown].pressedTimer = 0.f;
                    m_scrollPresses[ScrollID::StatDown].active = false;
                }
            });
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

    glm::vec2 offset = glm::vec2(0.f); //ugh such frikkin hack
    switch (menuID)
    {
    default:
        break;
    case MenuID::Video:
        offset.y = 86.f;
        downLeftA = AVMixerLeft;
        downLeftB = TabController;
        downRightA = TabAchievements;
        downRightB = TabStats;
        upLeftA = AVTextToSpeech;
        upLeftB = AVBeaconL;
        upRightA = AVCrowdL;
        upRightB = AVFixedPutter;
        break;
    case MenuID::Controls:
        downLeftA = TabAV;
        downLeftB = TabAV;
        downRightA = TabAchievements;
        downRightB = TabStats;
        upLeftA = CtrlReset;
        upLeftB = CtrlReset;
        upRightA = CtrlA;
        upRightB = CtrlA;
        break;
    case MenuID::Settings:
        downLeftA = TabAV;
        downLeftB = TabController;
        downRightA = TabAchievements;
        downRightB = TabStats;
        upLeftA = SettPuttZoom;
        upLeftB = ResetCareer;
        upRightA = ResetStats;
        upRightB = SettPostR;
        break;
    case MenuID::Achievements:
        offset.x = 40.f;
        offset.y = 70.f;
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
        offset.x = 40.f;
        offset.y = 70.f;
        downLeftA = TabAV;
        downLeftB = TabController;
        downRightA = TabAchievements;
        downRightB = ScrollUp;
        upLeftA = TabAV;
        upLeftB = TabController;
        upRightA = TabAchievements;
        upRightB = ScrollDown;
        break;
    }
    
    auto& pageButtons = parent.getComponent<PageButtons>();

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto textHideCallback = [&, menuID](cro::Entity e, float)
    {
        auto current = static_cast<std::int32_t>(m_scene.getSystem<cro::UISystem>()->getActiveGroup());
        float scale = (current == menuID || current == MenuID::Dummy) ? 1 : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };

    //credits
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(55.f, 1.f)+ offset);
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("large_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowCredits);
    entity.getComponent<cro::UIInput>().setPrevIndex(WindowClose, upLeftA);
    if (Social::isSteamdeck())
    {
        entity.getComponent<cro::UIInput>().setNextIndex(WindowApply, downLeftA);
    }
    else
    {
        entity.getComponent<cro::UIInput>().setNextIndex(WindowAdvanced, downLeftA);
    }
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
    pageButtons.buttons[ButtonID::Credits] = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //advanced
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(glm::vec2(131.f, 1.f) + offset, 0.2f));
    entity.addComponent<cro::Drawable2D>();
    pageButtons.buttons[ButtonID::Advanced] = entity;
    
    if (Social::isSteamdeck())
    {
        entity.getComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f, 18.f), CD32::Colours[CD32::Brown]),
                cro::Vertex2D(glm::vec2(0.f), CD32::Colours[CD32::Brown]),
                cro::Vertex2D(glm::vec2(75.f, 18.f), CD32::Colours[CD32::Brown]),
                cro::Vertex2D(glm::vec2(75.f, 0.f), CD32::Colours[CD32::Brown])
            });
    }
    else
    {
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("large_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().setGroup(menuID);
        entity.getComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setSelectionIndex(WindowAdvanced);
        entity.getComponent<cro::UIInput>().setPrevIndex(WindowCredits, upLeftB);
        entity.getComponent<cro::UIInput>().setNextIndex(WindowApply, downLeftB);
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
    }
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //apply
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(350.f, 1.f) + offset);
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("small_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowApply);
    if (Social::isSteamdeck())
    {
        entity.getComponent<cro::UIInput>().setPrevIndex(WindowCredits, upRightA);
    }
    else
    {
        entity.getComponent<cro::UIInput>().setPrevIndex(WindowAdvanced, upRightA);
    }
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

                applyWebsock();

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;
    pageButtons.buttons[ButtonID::Apply] = entity;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //close
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(401.f, 1.f) + offset);
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("small_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().setGroup(menuID);
    entity.getComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setSelectionIndex(WindowClose);
    entity.getComponent<cro::UIInput>().setPrevIndex(WindowApply, upRightB);
    entity.getComponent<cro::UIInput>().setNextIndex(WindowCredits, downRightB);
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
    pageButtons.buttons[ButtonID::Quit] = entity;
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
    const auto group = static_cast<std::uint8_t>(m_scene.getSystem<cro::UISystem>()->getActiveGroup());
    const auto& entities = m_scene.getSystem<cro::CallbackSystem>()->getEntities();
    for (auto entity : entities)
    {
        if (entity.hasComponent<cro::UIInput>())
        {
            entity.getComponent<cro::Callback>().active = (entity.getComponent<cro::UIInput>().getGroup() & (1 << group)) != 0;
        }
    }
}

void OptionsState::resetScroll()
{
    m_scrollPresses[ScrollID::AchUp].pressed = false;
    m_scrollPresses[ScrollID::AchUp].pressedTimer = 0.f;
    m_scrollPresses[ScrollID::AchUp].active = false;

    m_scrollPresses[ScrollID::AchDown].pressed = false;
    m_scrollPresses[ScrollID::AchDown].pressedTimer = 0.f;
    m_scrollPresses[ScrollID::AchDown].active = false;

    m_scrollPresses[ScrollID::StatUp].pressed = false;
    m_scrollPresses[ScrollID::StatUp].pressedTimer = 0.f;
    m_scrollPresses[ScrollID::StatUp].active = false;

    m_scrollPresses[ScrollID::StatDown].pressed = false;
    m_scrollPresses[ScrollID::StatDown].pressedTimer = 0.f;
    m_scrollPresses[ScrollID::StatDown].active = false;
}

void OptionsState::applyAudioDevice()
{
    const auto& devices = cro::AudioDevice::getDeviceList();
    if (devices.empty())
    {
        return;
    }

    audioDeviceIndex %= devices.size();

    cro::AudioDevice::setActiveDevice(devices[audioDeviceIndex]);
    assertDeviceIndex();
    refreshDeviceLabel();
}

void OptionsState::assertDeviceIndex()
{
    const auto& devices = cro::AudioDevice::getDeviceList();
    if (!devices.empty() &&
        cro::AudioDevice::getActiveDevice() != devices[audioDeviceIndex])
    {
        //we couldn't apply the device so correct the label / index
        if (auto result = std::find(devices.begin(), devices.end(), cro::AudioDevice::getActiveDevice()); result != devices.end())
        {
            audioDeviceIndex = std::distance(devices.begin(), result);
        }
        else
        {
            audioDeviceIndex = 0;
        }
    }
}

void OptionsState::refreshDeviceLabel()
{
    std::string str;
    if (cro::AudioDevice::getDeviceList().empty())
    {
        str = "No Device Available";
    }
    else
    {
        /*if (Social::isSteamdeck())
        {
            str = "Default";
        }
        else*/
        {
            str = cro::AudioDevice::getActiveDevice();
        }
    }

    static const std::string RemoveMe("OpenAL Soft on ");

    if (str.find(RemoveMe) != std::string::npos)
    {
        str = str.substr(RemoveMe.size());
    }

    m_deviceLabel.getComponent<cro::Text>().setString(str);
    auto bounds = cro::Text::getLocalBounds(m_deviceLabel);

    static constexpr float VerticalPos = 242.f;

    if (bounds.width > LabelCrop.width)
    {
        m_deviceLabel.getComponent<cro::Callback>().active = true;
        m_deviceLabel.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Left);
        m_deviceLabel.getComponent<cro::Transform>().setPosition({ 244.f, VerticalPos });
        m_deviceLabel.getComponent<cro::Drawable2D>().setCroppingArea(LabelCrop);
    }
    else
    {
        m_deviceLabel.getComponent<cro::Callback>().active = false;
        m_deviceLabel.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
        m_deviceLabel.getComponent<cro::Transform>().setPosition({ 308.f, VerticalPos });
        bounds.left = -(bounds.width / 2.f);
        m_deviceLabel.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
    }
    m_deviceLabel.getComponent<cro::Transform>().setOrigin({ 0.f, 0.f });
    m_deviceLabel.getComponent<cro::Callback>().getUserData<TextScrollData>().maxWidth = bounds.width;
}

void OptionsState::refreshControllerList()
{
    if (cro::GameController::getControllerCount() < 2)
    {
        m_controllerInfoEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
    else
    {
        cro::String str;
        for (auto i = 0; i < std::max(4, cro::GameController::getControllerCount()); ++i)
        {
            str += std::to_string(i + 1) + ". ";
            str += cro::GameController::getPrintableName(i);
            str += "\n";
        }
        m_controllerInfoEnt.getComponent<cro::Text>().setString(str);

        auto bounds = cro::Text::getLocalBounds(m_controllerInfoEnt);
        auto posX = m_controllerInfoEnt.getComponent<cro::Callback>().getUserData<const float>() - std::round(bounds.width / 2.f);
        auto pos = m_controllerInfoEnt.getComponent<cro::Transform>().getPosition();
        pos.x = posX;
        m_controllerInfoEnt.getComponent<cro::Transform>().setPosition(pos);

        m_controllerInfoEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }
    m_scene.getActiveCamera().getComponent<cro::Camera>().active = true;
}

void OptionsState::updateControlIndices(bool isKeyboard)
{
    if (isKeyboard)
    {
        auto entity = m_controlEntities[ControlButtons::ThreshL];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlThreshR, CtrlLookL);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlRight, CtrlLayout);

        entity = m_controlEntities[ControlButtons::ThreshR];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlRight, CtrlLookR);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshL, CtrlLayout);

        entity = m_controlEntities[ControlButtons::LookL];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlLookR, CtrlInvX);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlY, CtrlThreshL);

        entity = m_controlEntities[ControlButtons::LookR];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlY, CtrlVib);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLookL, CtrlThreshR);

        entity = m_controlEntities[ControlButtons::InvX];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlVib, CtrlInvY);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlX, CtrlLookL);

        entity = m_controlEntities[ControlButtons::InvY];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlAltPower, CtrlMouseAction);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlA, CtrlInvX);

        entity = m_controlEntities[ControlButtons::Vib];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlX, CtrlAltPower);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlInvX, CtrlLookR);

        entity = m_controlEntities[ControlButtons::AltPower];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlA, CtrlSwg);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlInvY, CtrlVib);
    }
    else
    {
        auto entity = m_controlEntities[ControlButtons::ThreshL];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlThreshR, CtrlLookL);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshR, CtrlLayout);

        entity = m_controlEntities[ControlButtons::ThreshR];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlThreshL, CtrlLookR);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlThreshL, CtrlLayout);

        entity = m_controlEntities[ControlButtons::LookL];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlLookR, CtrlInvX);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLookR, CtrlThreshL);

        entity = m_controlEntities[ControlButtons::LookR];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlLookL, CtrlVib);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlLookL, CtrlThreshR);

        entity = m_controlEntities[ControlButtons::InvX];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlVib, CtrlInvY);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlVib, CtrlLookL);

        entity = m_controlEntities[ControlButtons::InvY];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlAltPower, CtrlMouseAction);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlAltPower, CtrlInvX);

        entity = m_controlEntities[ControlButtons::Vib];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlInvX, CtrlAltPower);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlInvX, CtrlLookR);

        entity = m_controlEntities[ControlButtons::AltPower];
        entity.getComponent<cro::UIInput>().setNextIndex(CtrlInvY, CtrlSwg);
        entity.getComponent<cro::UIInput>().setPrevIndex(CtrlInvY, CtrlVib);
    }
}

void OptionsState::quitState()
{
    //m_scene.setSystemActive<cro::AudioPlayerSystem>(false);
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void OptionsState::applyWebsock()
{
    if (m_sharedData.webSocket)
    {
        if (WebSock::getPort() && WebSock::getPort() != m_sharedData.webPort)
        {
            WebSock::stop();
        }

        if (!WebSock::isRunning())
        {
            WebSock::start(m_sharedData.webPort);
        }
    }
    else
    {
        if (WebSock::isRunning())
        {
            WebSock::stop();
        }
    }
}