/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "GameConsts.hpp"
#include "MessageIDs.hpp"
#include "../AchievementStrings.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
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
#include <crogine/util/String.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/detail/OpenGL.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <sstream>
#include <iomanip>

namespace
{
    constexpr float CameraDepth = 3.f;

    constexpr float BackgroundDepth = -0.5f;
    constexpr float OptionsDepth = -0.4f;
    constexpr float TabWindowDepth = 0.1f;
    constexpr float TabBarDepth = 0.15f;

    constexpr glm::vec3 PanelPosition(4.f, 20.f, TabWindowDepth);
    constexpr glm::vec3 HiddenPosition(-10000.f);

    constexpr float HighlightOffset = 0.2f;
    constexpr float TextOffset = 0.1f;

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
        "Voice Volume"
    };
    //generally static vars would be a bad idea, but in this case
    //a static index value will remember the last channel between
    //showing instances of options, as well as being available to
    //the slider callbacks :3
    std::uint8_t mixerChannelIndex = MixerChannel::Music;

    static constexpr float SliderWidth = 142.f;
    static constexpr glm::vec3 ToolTipOffset(10.f, 10.f, 0.f);

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
}

OptionsState::OptionsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 256/*, cro::INFO_FLAG_SYSTEMS_ACTIVE | cro::INFO_FLAG_SYSTEM_TIME*/),
    m_sharedData        (sd),
    m_updatingKeybind   (false),
    m_lastMousePos      (0.f),
    m_bindingIndex      (-1),
    m_currentTabFunction(0),
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

    //updateActiveCallbacks();
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
            if (!m_updatingKeybind)
            {
                quitState();
            }
            else
            {
                //cancel the input
                updateKeybind(evt.key.keysym.sym);
            }
            return false;
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(0))
        {
        
            switch (evt.cbutton.button)
            {
            default: break;
            case cro::GameController::ButtonB:
                if (!m_updatingKeybind)
                {
                    quitState();
                }
                else
                {
                    updateKeybind(SDLK_ESCAPE);
                }
                return false;
            }
        }
    }
    else if (evt.type == SDL_CONTROLLERBUTTONDOWN)
    {
        if (evt.cbutton.which == cro::GameController::deviceID(0))
        {

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
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONDOWN)
    {
        if (evt.button.button == SDL_BUTTON_LEFT
            && !m_updatingKeybind)
        {
            pickSlider();
        }
    }
    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        if (evt.button.button == SDL_BUTTON_LEFT)
        {
            m_activeSlider = {};
        }
    }
    else if (evt.type == SDL_MOUSEMOTION)
    {
        updateSlider();
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            cro::ButtonEvent fakeEvent;
            fakeEvent.type = SDL_MOUSEBUTTONDOWN;
            fakeEvent.button.button = SDL_BUTTON_LEFT;

            //up
            auto menuID = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
            if (menuID == MenuID::Achievements)
            {
                m_scrollFunctions[ScrollID::AchUp](cro::Entity(), fakeEvent);
            }
            else if (menuID == MenuID::Stats)
            {
                m_scrollFunctions[ScrollID::StatUp](cro::Entity(), fakeEvent);
            }
        }
        else if (evt.wheel.y < 0)
        {
            cro::ButtonEvent fakeEvent;
            fakeEvent.type = SDL_MOUSEBUTTONDOWN;
            fakeEvent.button.button = SDL_BUTTON_LEFT;

            //down
            auto menuID = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
            if (menuID == MenuID::Achievements)
            {
                m_scrollFunctions[ScrollID::AchDown](cro::Entity(), fakeEvent);
            }
            else if (menuID == MenuID::Stats)
            {
                m_scrollFunctions[ScrollID::StatDown](cro::Entity(), fakeEvent);
            }
        }
    }

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
    auto& keys = m_sharedData.inputBinding.keys;
    if (auto result = std::find(keys.begin(), keys.end(), key); result != keys.end())
    {
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::PlayerConfig;
        cmd.action = [key](cro::Entity e, float)
        {
            //briefly shoes error message before returning to old string
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
    cmd.action = [index](cro::Entity e, float)
    {
        switch (index)
        {
        default: break;
        case InputBinding::PrevClub:
            e.getComponent<cro::Text>().setString("Next Club");
            break;
        case InputBinding::NextClub:
            e.getComponent<cro::Text>().setString("Previous Club");
            break;
        case InputBinding::Left:
            e.getComponent<cro::Text>().setString("Aim Left");
            break;
        case InputBinding::Right:
            e.getComponent<cro::Text>().setString("Aim Right");
            break;
        case InputBinding::Action:
            e.getComponent<cro::Text>().setString("Take Shot");
            break;
        }
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
    //only problem here is if player changes between opening
    //pause menu and opening options menu changes the active controller
    //as the game isn't actually paused. Not much we can do about this though?
    m_scene.addSystem<cro::UISystem>(mb)->setActiveControllerID(m_sharedData.inputBinding.controllerID);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::TextSystem>(mb);
    m_scene.addSystem<cro::CameraSystem>(mb);
    m_scene.addSystem<cro::RenderSystem2D>(mb);
    m_scene.addSystem<cro::AudioPlayerSystem>(mb);

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

                m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Video);
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
    auto selectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Text>().setFillColour(TextGoldColour); e.getComponent<cro::AudioEmitter>().play(); });
    auto unselectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Text>().setFillColour(TextNormalColour); });

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

    //tab callback functions
    m_tabFunctions[0] = [&, showVideo, hideControls, hideAchievements, hideStats]() mutable
    {
        //hide other tabs
        hideControls();
        hideAchievements();
        hideStats();

        //show video tab
        showVideo();
        uiSystem.setActiveGroup(MenuID::Video);

        m_currentTabFunction = 0;
    };

    m_tabFunctions[1] = [&, hideVideo, showControls, hideAchievements, hideStats]() mutable
    {
        //hide other tabs
        hideVideo();
        hideAchievements();
        hideStats();

        //reset position for control button ent
        showControls();

        uiSystem.setActiveGroup(MenuID::Controls);

        m_currentTabFunction = 1;
    };

    m_tabFunctions[2] = [&, hideVideo, hideControls, showAchievements, hideStats]() mutable
    {
        //hide other tabs
        hideVideo();
        hideControls();
        hideStats();

        //show achievment tab
        showAchievements();
        uiSystem.setActiveGroup(MenuID::Achievements);

        m_currentTabFunction = 2;
    };

    m_tabFunctions[3] = [&, hideVideo, hideControls, hideAchievements, showStats]() mutable
    {
        //hide other tabs
        hideVideo();
        hideControls();
        hideAchievements();

        //show stats tab
        showStats();
        uiSystem.setActiveGroup(MenuID::Stats);

        m_currentTabFunction = 3;
    };


    const std::array<glm::vec3, 4u> TabPositions =
    {
        glm::vec3(1.f, 171.f, TabBarDepth + HighlightOffset),
        glm::vec3(101.f, 171.f, TabBarDepth + HighlightOffset),
        glm::vec3(201.f, 171.f, TabBarDepth + HighlightOffset),
        glm::vec3(301.f, 171.f, TabBarDepth + HighlightOffset)
    };

    auto createTab = [&, spriteSelectedID, spriteUnselectedID](cro::Entity parent, std::size_t index, std::int32_t menuID)
    {
        auto ent = m_scene.createEntity();
        ent.addComponent<cro::Transform>().setPosition(TabPositions[index]);
        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        ent.addComponent<cro::Drawable2D>();
        ent.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
        ent.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        ent.addComponent<cro::UIInput>().area = ent.getComponent<cro::Sprite>().getTextureBounds();
        ent.getComponent<cro::UIInput>().setGroup(menuID);
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
    };
    createTab(videoButtonEnt, 1, MenuID::Video);
    createTab(videoButtonEnt, 2, MenuID::Video);
    createTab(videoButtonEnt, 3, MenuID::Video);

    createTab(controlButtonEnt, 0, MenuID::Controls);
    createTab(controlButtonEnt, 2, MenuID::Controls);
    createTab(controlButtonEnt, 3, MenuID::Controls);

    createTab(achButtonEnt, 0, MenuID::Achievements);
    createTab(achButtonEnt, 1, MenuID::Achievements);
    createTab(achButtonEnt, 3, MenuID::Achievements);

    createTab(statsButtonEnt, 0, MenuID::Stats);
    createTab(statsButtonEnt, 1, MenuID::Stats);
    createTab(statsButtonEnt, 2, MenuID::Stats);

    
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
    m_tooltips[ToolTipID::FOV] = createToolTip("FOV: 60");
    m_tooltips[ToolTipID::Pixel] = createToolTip("Scale up pixels to match\nthe current resolution.");
    m_tooltips[ToolTipID::VertSnap] = createToolTip("Snaps vertices to the nearest\nwhole pixel for a retro \'wobble\'.");
    m_tooltips[ToolTipID::Beacon] = createToolTip("Shows a beacon to indicate flag position\nat far distances.");
    m_tooltips[ToolTipID::BeaconColour] = createToolTip("Display colour of the beacon.");
    m_tooltips[ToolTipID::MouseSpeed] = createToolTip("1.00");
    m_tooltips[ToolTipID::Video] = createToolTip("Sound & Video Settings");
    m_tooltips[ToolTipID::Controls] = createToolTip("Controls");
    m_tooltips[ToolTipID::Achievements] = createToolTip("Achievements");
    m_tooltips[ToolTipID::Stats] = createToolTip("Stats");

    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, CameraDepth });
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
        m_scene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Controls);
        e.getComponent<cro::Callback>().active = false;
        m_scene.destroyEntity(e);
    };
}

void OptionsState::buildAVMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto bgBounds = parent.getComponent<cro::Sprite>().getTextureBounds();
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);

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
    auto audioLabel = createLabel(glm::vec2(bgBounds.width / 2.f, 137.f), "Music Volume");
    centreText(audioLabel);

    //FOV label
    auto fovLabel = createLabel(glm::vec2(12.f, 98.f), "FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));

    //resolution label
    auto resLabel = createLabel(glm::vec2(12.f, 84.f), "Resolution");
    //centreText(resLabel);

    //resolution value text
    resLabel = createLabel(glm::vec2(136.f, 84.f), m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
    centreText(resLabel);

    //pixel scale label
    auto pixelLabel = createLabel(glm::vec2(12.f, 70.f), "Pixel Scaling");
    pixelLabel.addComponent<cro::Callback>().active = true;
    pixelLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::Pixel);
    };

    //vertex snap label
    auto vertLabel = createLabel(glm::vec2(12.f, 55.f), "Vertex Snap      (requires restart)");
    vertLabel.addComponent<cro::Callback>().active = true;
    vertLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::VertSnap);
    };

    //full screen label
    createLabel(glm::vec2(12.f, 39.f), "Full Screen");

    //beacon label
    auto beaconLabel = createLabel(glm::vec2(12.f, 23.f), "Flag Beacon");
    beaconLabel.addComponent<cro::Callback>().active = true;
    beaconLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        updateToolTip(e, ToolTipID::Beacon);
    };

    //post process label
    createLabel({ 204.f, 98.f }, "Post FX");


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
    auto volSlider = createSlider(glm::vec2(125.f, 119.f));
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



    //TODO this is repeated for each creation function - we could reduce this to one instance
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
    auto entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 56.f, 128.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) + 45.f, 128.f));
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
    entity = createHighlight(glm::vec2(107.f, 113.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderDownCallback(m_audioEnts[AudioID::Accept]));

    //audio up
    entity = createHighlight(glm::vec2(274.f, 113.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderUpCallback(m_audioEnts[AudioID::Back]));

    //FOV down
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 89.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 115.f, 75.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 14.f, 75.f));
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
    entity = createHighlight(glm::vec2(81.f, 61.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    togglePixelScale(m_sharedData, !m_sharedData.pixelScale);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //pixel scale checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 63.f, HighlightOffset));
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
    entity = createHighlight(glm::vec2(81.f, 45.f));
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 47.f, HighlightOffset));
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
    entity = createHighlight(glm::vec2(81.f, 29.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_videoSettings.fullScreen = !m_videoSettings.fullScreen;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //full screen checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 31.f, HighlightOffset));
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
    entity = createHighlight(glm::vec2(81.f, 13.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    m_sharedData.showBeacon = !m_sharedData.showBeacon;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //beacon checkbox centre
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(83.f, 15.f, HighlightOffset));
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(99.f, 15.f, HighlightOffset));
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

    entity = createHighlight(glm::vec2(114.f, 13.f));
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

    entity = createHighlight(glm::vec2(182.f, 13.f));
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
    auto colourPos = glm::vec2(131.f, 19.f);
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


    //post process checkbox
    entity = createHighlight(glm::vec2(246.f, 89.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessToggled;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    //post process checkbox centre
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
        float scale = m_sharedData.usePostProcess ? 1.f : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto shaderLabel = createLabel(glm::vec2(325.f, 98.f), ShaderNames[m_sharedData.postProcessIndex]);
    centreText(shaderLabel);

    //prev/next post process
    entity = createHighlight(glm::vec2(262.f, 89.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, shaderLabel](cro::Entity e, cro::ButtonEvent evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.postProcessIndex = (m_sharedData.postProcessIndex + (ShaderNames.size() - 1)) % ShaderNames.size();
                    auto* msg = getContext().appInstance.getMessageBus().post<SystemEvent>(MessageID::SystemMessage);
                    msg->type = SystemEvent::PostProcessIndexChanged;

                    shaderLabel.getComponent<cro::Text>().setString(ShaderNames[m_sharedData.postProcessIndex]);
                    centreText(shaderLabel);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

    entity = createHighlight(glm::vec2(378.f, 89.f));
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
}

void OptionsState::buildControlMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();

    auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& uiFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto infoEnt = m_scene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition(glm::vec3(parentBounds.width / 2.f, parentBounds.height - 8.f, TextOffset));
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(uiFont);// .setString("Info Text");
    infoEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    infoEnt.getComponent<cro::Text>().setCharacterSize(UITextSize);
    //infoEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerConfig; //not the best description, just recycling existing members here...

    infoEnt.addComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& [currTime, str] = e.getComponent<cro::Callback>().getUserData<std::pair<float, cro::String>>();
        currTime -= dt;
        if (currTime < 0)
        {
            e.getComponent<cro::Text>().setString(str);
            centreText(e);
            e.getComponent<cro::Callback>().active = false;
        }
    };

    parent.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());


    auto buttonChangeEnt = m_scene.createEntity();
    buttonChangeEnt.addComponent<cro::Transform>().setPosition(glm::vec3((parentBounds.width / 4.f) * 3.f, 12.f, TextOffset));
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
            infoEnt.getComponent<cro::Text>().setString(" ");
            centreText(infoEnt);
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
                if (evt.type != SDL_MOUSEBUTTONDOWN
                    && evt.type != SDL_MOUSEBUTTONUP
                    && activated(evt))
                {
                    m_updatingKeybind = true;

                    //we have to copy this because GCC thinks getComponent<>()
                    //still returns an immutable reference
                    auto b = infoEnt;
                    b.getComponent<cro::Text>().setString("Press a Key");
                    centreText(infoEnt);

                    m_bindingIndex = keyIndex;

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });

        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

    static std::vector<std::string> labelStrings;
    if (m_sharedData.baseState == StateID::Clubhouse)
    {
        labelStrings =
        {
            "Take Shot",
            "Power Up",
            "Power Down",
            "Rotate Camera (Hold)",
            "Switch View",
            "Side Spin",
            "Side Spin",
            "Top Spin",
            "Back Spin"
        };

        //add overlay from sprite sheet
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 236.f, 32.f, HighlightOffset });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("extra_buttons");
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
    else
    {
        labelStrings =
        {
            "Take Shot",
            "Next Club",
            "Previous Club",
            "",
            "",
            "Aim Left",
            "Aim Right"
        };
    }


    //prev club
    auto entity = createHighlight(glm::vec2(258.f, 96.f), InputBinding::PrevClub);
    entity.getComponent<cro::UIInput>().setSelectionIndex(5);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::PrevClub]);
            centreText(infoEnt);

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    //next club
    entity = createHighlight(glm::vec2(324.f, 96.f), InputBinding::NextClub);
    entity.getComponent<cro::UIInput>().setSelectionIndex(6);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::NextClub]);
            centreText(infoEnt);

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    //aim left
    entity = createHighlight(glm::vec2(230.f, 58.f), InputBinding::Left);
    entity.getComponent<cro::UIInput>().setSelectionIndex(8);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::Left]);
            centreText(infoEnt);

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    //aim right
    entity = createHighlight(glm::vec2(230.f, 43.f), InputBinding::Right);
    entity.getComponent<cro::UIInput>().setSelectionIndex(9);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::Right]);
            centreText(infoEnt);

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    //swing
    entity = createHighlight(glm::vec2(352.f, 47.f), InputBinding::Action);
    entity.getComponent<cro::UIInput>().setSelectionIndex(11);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt, buttonChangeEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::Action]);
            centreText(infoEnt);

            buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });

    if (m_sharedData.baseState == StateID::Clubhouse)
    {
        //top spin
        entity = createHighlight(glm::vec2(230.f, 73.f), InputBinding::Up);
        entity.getComponent<cro::UIInput>().setSelectionIndex(7);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [infoEnt, buttonChangeEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::Up]);
                centreText(infoEnt);

                buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            });

        //back spin
        entity = createHighlight(glm::vec2(230.f, 28.f), InputBinding::Down);
        entity.getComponent<cro::UIInput>().setSelectionIndex(10);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [infoEnt, buttonChangeEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::Down]);
                centreText(infoEnt);

                buttonChangeEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
            });

        //switch view
        entity = createHighlight(glm::vec2(338.f, 84.f), InputBinding::SwitchView);
        entity.getComponent<cro::UIInput>().setSelectionIndex(12);
        entity.getComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight_white");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 0; //don't rebind this
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [infoEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::SwitchView]);
                centreText(infoEnt);
            });

        //rotate cam
        entity = createHighlight(glm::vec2(292.f, 81.f), InputBinding::CamModifier);
        entity.getComponent<cro::UIInput>().setSelectionIndex(13);
        entity.getComponent<cro::Sprite>() = spriteSheet.getSprite("round_highlight_white");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 0; //don't rebind this
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
            [infoEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                e.getComponent<cro::AudioEmitter>().play();
                infoEnt.getComponent<cro::Text>().setString(labelStrings[InputBinding::CamModifier]);
                centreText(infoEnt);
            });
    }


    //label entities
    struct LabelCallback final
    {
        std::int32_t labelIndex = 0;
        const InputBinding& binding;
        LabelCallback(std::int32_t idx, const InputBinding& b) : labelIndex(idx), binding(b) {}

        void operator() (cro::Entity e, float)
        {
            e.getComponent<cro::Text>().setString(cro::Keyboard::keyString(binding.keys[labelIndex]));
            centreText(e);
        }
    };

    auto createLabel = [&](glm::vec2 position, std::int32_t bindingIndex)
    {
        auto e = m_scene.createEntity();
        e.addComponent<cro::Transform>().setPosition(glm::vec3(position, TextOffset));
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function = LabelCallback(bindingIndex, m_sharedData.inputBinding);
        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    };

    //prev club
    createLabel(glm::vec2(252.f, 110.f), InputBinding::PrevClub);

    //next club
    createLabel(glm::vec2(340.f, 110.f), InputBinding::NextClub);

    //stroke
    createLabel(glm::vec2(366.f, 45.f), InputBinding::Action);

    //left
    createLabel(glm::vec2(220.f, 66.f), InputBinding::Left);
    
    //right
    createLabel(glm::vec2(220.f, 51.f), InputBinding::Right);

    if (m_sharedData.baseState == StateID::Clubhouse)
    {
        //up
        createLabel(glm::vec2(220.f, 81.f), InputBinding::Up);

        //down
        createLabel(glm::vec2(220.f, 36.f), InputBinding::Down);

        //rotate cam
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 288.f, 101.f, TextOffset });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setString("RMB");
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        //switch view
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 351.f, 92.f, TextOffset });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setString("Num 1");
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
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
    };

    createText(glm::vec2(20.f, 92.f), "Look Speed (Billiards)");
    createText(glm::vec2(32.f, 63.f), "Invert X");
    createText(glm::vec2(32.f, 47.f), "Invert Y");

    //TODO don't duplicate these as they already exist in the AV menu
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


    auto createSlider = [&](glm::vec2 position)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slider");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), /*std::floor*/(bounds.height / 2.f), -TextOffset });

        auto userData = SliderData(position);
        userData.onActivate = [&](float distance)
        {
            //distance = 0-1
            m_sharedData.mouseSpeed = ConstVal::MinMouseSpeed + ((ConstVal::MaxMouseSpeed - ConstVal::MinMouseSpeed) * distance);
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

    auto mouseSlider = createSlider(glm::vec2(35.f, 77.f));

    auto tipEnt = m_scene.createEntity();
    tipEnt.addComponent<cro::Transform>();
    tipEnt.addComponent<cro::Callback>().active = true;
    tipEnt.getComponent<cro::Callback>().function =
        [&, mouseSlider](cro::Entity, float)
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        auto bounds = mouseSlider.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = mouseSlider.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            mousePos.x = std::floor(mousePos.x);
            mousePos.y = std::floor(mousePos.y);
            mousePos.z = ToolTipDepth;

            m_tooltips[ToolTipID::MouseSpeed].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
            m_tooltips[ToolTipID::MouseSpeed].getComponent<cro::Transform>().setScale(m_viewScale);

            std::stringstream ss;
            ss.precision(2);
            ss << std::setw(2) << m_sharedData.mouseSpeed;
            m_tooltips[ToolTipID::MouseSpeed].getComponent<cro::Text>().setString(ss.str());
        }
        else
        {
            m_tooltips[ToolTipID::MouseSpeed].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };
    mouseSlider.getComponent<cro::Transform>().addChild(tipEnt.getComponent<cro::Transform>());


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


    //mouse speed down
    entity = createSquareHighlight(glm::vec2(17.f, 71.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(14);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.mouseSpeed = std::max(ConstVal::MinMouseSpeed, m_sharedData.mouseSpeed - 0.1f);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //mouse speed up
    entity = createSquareHighlight(glm::vec2(184.f, 71.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(15);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.mouseSpeed = std::min(ConstVal::MaxMouseSpeed, m_sharedData.mouseSpeed + 0.1f);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    //invert X
    entity = createSquareHighlight(glm::vec2(17.f, 54.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(16);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.invertX = !m_sharedData.invertX;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
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

    //invert Y
    entity = createSquareHighlight(glm::vec2(17.f, 38.f));
    entity.getComponent<cro::UIInput>().setSelectionIndex(17);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                m_sharedData.invertY = !m_sharedData.invertY;
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
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
}

void OptionsState::buildAchievementsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    //render details to buffer
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
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

    glm::vec2 position(60.f, bufferSize.y - (4 + VerticalSpacing));
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

        title.setPosition(position + TitleOffset);
        title.setString(AchievementLabels[i]);
        title.draw();

        desc.setPosition(position + DescOffset);
        desc.setString(AchievementDesc[i].first);
        desc.draw();

        auto ts = Achievements::getAchievement(AchievementStrings[i])->timestamp;
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
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::AchUp]);

    auto downHighlight = createHighlight({ cropping.width - 19.f, 7.f });
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::AchDown]);
}

void OptionsState::buildStatsMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
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
    const glm::vec2 TitleOffset(40.f, 24.f);
    const glm::vec2 DescOffset(40.f, 13.f);


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
    upHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::StatUp]);

    auto downHighlight = createHighlight({ cropping.width - 19.f, 7.f });
    downHighlight.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(m_scrollFunctions[ScrollID::StatDown]);
}

void OptionsState::createButtons(cro::Entity parent, std::int32_t menuID, std::uint32_t selectedID, std::uint32_t unselectedID, const cro::SpriteSheet& spriteSheet)
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto textHideCallback = [&, menuID](cro::Entity e, float)
    {
        auto current = static_cast<std::int32_t>(m_scene.getSystem<cro::UISystem>()->getActiveGroup());
        float scale = (current == menuID || current == MenuID::Dummy) ? 1 : 0.f;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };

    //advanced
    auto entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(8.f, 14.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
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


    //website
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(88.f, 12.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("www");
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
                cro::Util::String::parseURL("https://fallahn.itch.io/vga-golf");
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    //hack to stop multiple instances covering each other when not active
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //apply
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(303.f, 14.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
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
        [&](cro::Entity e, cro::ButtonEvent evt)
        {
            if (activated(evt))
            {
                cro::App::getWindow().setSize(m_sharedData.resolutions[m_videoSettings.resolutionIndex]);
                cro::App::getWindow().setFullScreen(m_videoSettings.fullScreen);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = textHideCallback;
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //close
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(354.f, 14.f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
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

        m_tooltips[tipID].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
        m_tooltips[tipID].getComponent<cro::Transform>().setScale(m_viewScale);
    }
    else
    {
        m_tooltips[tipID].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    }
}

void OptionsState::updateActiveCallbacks()
{
    auto group = m_scene.getSystem<cro::UISystem>()->getActiveGroup();
    auto& entities = m_scene.getSystem<cro::CallbackSystem>()->getEntities();
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
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}