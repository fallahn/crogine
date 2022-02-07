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

#include <crogine/core/Window.hpp>
#include <crogine/core/Mouse.hpp>
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

#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>
#include <crogine/audio/AudioMixer.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Video, Controls, Dummy
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

    constexpr float SliderWidth = 142.f;
    constexpr glm::vec3 ToolTipOffset(10.f, 10.f, 0.f);

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
}

OptionsState::OptionsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State        (ss, ctx),
    m_scene             (ctx.appInstance.getMessageBus(), 128, cro::INFO_FLAG_SYSTEMS_ACTIVE | cro::INFO_FLAG_SYSTEM_TIME),
    m_sharedData        (sd),
    m_updatingKeybind   (false),
    m_lastMousePos      (0.f),
    m_bindingIndex      (-1),
    m_currentTabFunction(1),
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
        case cro::GameController::ButtonLeftShoulder:
        case cro::GameController::ButtonRightShoulder:
            if (!m_updatingKeybind)
            {
                m_tabFunctions[m_currentTabFunction]();
                m_currentTabFunction = (m_currentTabFunction + 1) % 2;
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

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/options.spt", m_sharedData.sharedResources->textures);

    //options background
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
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
    entity.addComponent<cro::Transform>().setPosition({ 4.f, 20.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("audio_video");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto videoEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(-10000.f));
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Video, selectedID, unselectedID);
    auto videoButtonEnt = entity;

    //control options
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 4.f, 20.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("input");
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto controlEnt = entity;

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    createButtons(entity, MenuID::Controls, selectedID, unselectedID);
    auto controlButtonEnt = entity;

    //tab bar header
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_bar");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ 0.f, bgSize.y - bounds.height, 0.1f });
    entity.addComponent<cro::Callback>();
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(1.f, 1);
    entity.getComponent<cro::Callback>().function =
        [&uiSystem, videoEnt, controlEnt](cro::Entity e, float dt) mutable
    {
        auto& [currPos, direction] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (direction == 0)
        {
            //grow
            currPos = std::min(1.f, currPos + dt);
            if (currPos == 1)
            {
                direction = 1;
                uiSystem.setActiveGroup(MenuID::Controls);
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            //shrink
            currPos = std::max(0.f, currPos - dt);
            if (currPos == 0)
            {
                direction = 0;
                uiSystem.setActiveGroup(MenuID::Video);
                e.getComponent<cro::Callback>().active = false;
            }
        }

        auto progress = cro::Util::Easing::easeInOutQuint(currPos);

        auto area = e.getComponent<cro::Sprite>().getTextureBounds();
        area.width *= progress;
        e.getComponent<cro::Drawable2D>().setCroppingArea(area);

        controlEnt.getComponent<cro::Transform>().setScale({ progress, 1.f });

        videoEnt.getComponent<cro::Transform>().setScale({ 1.f - progress, 1.f });
        auto bounds = videoEnt.getComponent<cro::Sprite>().getTextureBounds();
        auto pos = videoEnt.getComponent<cro::Transform>().getPosition();
        pos.x = (bounds.width * progress) + 4.f; //need to tie the 4 in with the position above
        videoEnt.getComponent<cro::Transform>().setPosition(pos);
    };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto tabEnt = entity;

    //dummy input to consume events during animation
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    selectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::White); e.getComponent<cro::AudioEmitter>().play(); });
    unselectedID = uiSystem.addCallback([](cro::Entity e) {e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });

    //tab button to switch to video
    m_tabFunctions[0] = [&, tabEnt, videoButtonEnt, controlButtonEnt]() mutable
    {
        tabEnt.getComponent<cro::Callback>().active = true;
        controlButtonEnt.getComponent<cro::Transform>().move(glm::vec2(-10000.f));

        //reset position for video button ent
        videoButtonEnt.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));

        //set the active group to a dummy until animation is complete
        uiSystem.setActiveGroup(MenuID::Dummy);

        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(0.f, 130.f, 0.2f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Controls);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)
    {
            if (activated(evt))
            {
                m_tabFunctions[0]();
                m_currentTabFunction = 1;
            }
    });
    controlButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_tabFunctions[1] = [&, tabEnt, videoButtonEnt, controlButtonEnt]() mutable
    {
        tabEnt.getComponent<cro::Callback>().active = true;
        videoButtonEnt.getComponent<cro::Transform>().move(glm::vec2(-10000.f));

        //reset position for control button ent
        controlButtonEnt.getComponent<cro::Transform>().setPosition(glm::vec2(0.f));

        //set the active group to a dummy until animation is complete
        uiSystem.setActiveGroup(MenuID::Dummy);

        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    //tab button to switch to controls
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec3(100.f, 130.f, 0.2f));
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("tab_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Video);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [&](cro::Entity e, cro::ButtonEvent evt)  mutable
        {
            if (activated(evt))
            {
                m_tabFunctions[1]();
                m_currentTabFunction = 0;
            }
        });
    videoButtonEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    buildAVMenu(videoEnt, spriteSheet);
    buildControlMenu(controlEnt, spriteSheet);


    //tool tips for options
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto createToolTip = [&](const cro::String& tip)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({0.f, 0.f, 0.2f});
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(tip);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        
        constexpr float Padding = 10.f;
        auto bounds = cro::Text::getLocalBounds(entity);
        bounds.width += Padding;
        bounds.height += Padding;
        bounds.left -= (Padding / 2.f);
        bounds.bottom -= (Padding / 2.f);

        auto colour = cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha);
        auto bgEnt = m_scene.createEntity();
        bgEnt.addComponent<cro::Transform>().setPosition({0.f, 0.f, - 0.5f});
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
    entity.addComponent<cro::Transform>();
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
        entity.addComponent<cro::Transform>().setPosition(pos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setString(str);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

    //audio label
    auto audioLabel = createLabel(glm::vec2(bgBounds.width / 2.f, 98.f), "Music Volume");
    centreText(audioLabel);

    //FOV label
    auto fovLabel = createLabel(glm::vec2(6.f, 59.f), "FOV: " + std::to_string(static_cast<std::int32_t>(m_sharedData.fov)));

    //resolution label
    auto resLabel = createLabel(glm::vec2(6.f, 45.f), "Resolution");
    //centreText(resLabel);

    //resolution value text
    resLabel = createLabel(glm::vec2(130.f, 45.f), m_sharedData.resolutionStrings[m_videoSettings.resolutionIndex]);
    centreText(resLabel);

    //pixel scale label
    auto pixelLabel = createLabel(glm::vec2(6.f, 31.f), "Pixel Scaling");
    pixelLabel.addComponent<cro::Callback>().active = true;
    pixelLabel.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float) mutable
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        auto bounds = e.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = e.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            mousePos.x = std::floor(mousePos.x);
            mousePos.y = std::floor(mousePos.y);
            mousePos.z = 1.f;

            m_tooltips[ToolTipID::Pixel].getComponent<cro::Transform>().setPosition(mousePos + (ToolTipOffset * m_viewScale.x));
            m_tooltips[ToolTipID::Pixel].getComponent<cro::Transform>().setScale(m_viewScale);
        }
        else
        {
            m_tooltips[ToolTipID::Pixel].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    };

    //full screen label
    createLabel(glm::vec2(6.f, 15.f), "Full Screen");



    auto createSlider = [&](glm::vec2 position)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("slider");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), /*std::floor*/(bounds.height / 2.f) });


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
    auto volSlider = createSlider(glm::vec2(25.f, 80.f));
    auto tipEnt = m_scene.createEntity();
    tipEnt.addComponent<cro::Transform>();
    tipEnt.addComponent<cro::Callback>().active = true;
    tipEnt.getComponent<cro::Callback>().function =
        [&, volSlider](cro::Entity, float)
    {
        auto mousePos = m_scene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        auto bounds = volSlider.getComponent<cro::Drawable2D>().getLocalBounds();
        bounds = volSlider.getComponent<cro::Transform>().getWorldTransform() * bounds;

        if (bounds.contains(mousePos))
        {
            mousePos.x = std::floor(mousePos.x);
            mousePos.y = std::floor(mousePos.y);
            mousePos.z = 1.f;

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
    auto fovPos = glm::vec2(93.f, 56.f);
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
            mousePos.z = 1.f;

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
        ent.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        ent.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

        parent.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

        return ent;
    };

    //channel select down
    auto entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 56.f, 89.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [audioLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                mixerChannelIndex = (mixerChannelIndex + (MixerChannel::Count - 1)) % MixerChannel::Count;
                audioLabel.getComponent<cro::Text>().setString(MixerLabels[mixerChannelIndex]);
                centreText(audioLabel);
            }
        });

    //channel select up
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) + 45.f, 89.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(
        [audioLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                mixerChannelIndex = (mixerChannelIndex + 1) % MixerChannel::Count;
                audioLabel.getComponent<cro::Text>().setString(MixerLabels[mixerChannelIndex]);
                centreText(audioLabel);
            }
        });


    //audio down
    entity = createHighlight(glm::vec2(7.f, 74.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderDownCallback(m_audioEnts[AudioID::Back]));

    //audio up
    entity = createHighlight(glm::vec2(174.f, 74.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = uiSystem.addCallback(SliderUpCallback(m_audioEnts[AudioID::Accept]));

    //FOV down
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 21.f, 50.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) + 80.f, 50.f));
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback(
            [&, fovLabel](cro::Entity e, cro::ButtonEvent evt) mutable
        {
            if (activated(evt))
            {
                auto fov = m_sharedData.fov;
                fov = std::min(MaxFOV, fov + 5.f);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) - 21.f, 36.f));
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
    entity = createHighlight(glm::vec2((bgBounds.width / 2.f) + 80.f, 36.f));
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
    entity = createHighlight(glm::vec2(75.f, 22.f));
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(77.f, 24.f));
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


    //full screen check box
    entity = createHighlight(glm::vec2(75.f, 6.f));
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
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(77.f, 8.f));
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
}

void OptionsState::buildControlMenu(cro::Entity parent, const cro::SpriteSheet& spriteSheet)
{
    auto parentBounds = parent.getComponent<cro::Sprite>().getTextureBounds();

    auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto infoEnt = m_scene.createEntity();
    infoEnt.addComponent<cro::Transform>().setPosition(glm::vec2(parentBounds.width / 2.f, parentBounds.height - 4.f));
    infoEnt.addComponent<cro::Drawable2D>();
    infoEnt.addComponent<cro::Text>(infoFont);// .setString("Info Text");
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
            centreText(e);
            e.getComponent<cro::Callback>().active = false;
        }
    };

    parent.getComponent<cro::Transform>().addChild(infoEnt.getComponent<cro::Transform>());

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();
    auto unselectID = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            infoEnt.getComponent<cro::Text>().setString(" ");
            centreText(infoEnt);
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    auto createHighlight = [&, infoEnt](glm::vec2 position, std::int32_t keyIndex)
    {
        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(position);
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

    //prev club
    auto entity = createHighlight(glm::vec2(58.f, 80.f), InputBinding::PrevClub);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString("Previous Club");
            centreText(infoEnt);
        });


    //next club
    entity = createHighlight(glm::vec2(124.f, 80.f), InputBinding::NextClub);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString("Next Club");
            centreText(infoEnt);
        });

    //aim left
    entity = createHighlight(glm::vec2(30.f, 42.f), InputBinding::Left);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString("Aim Left");
            centreText(infoEnt);
        });

    //aim right
    entity = createHighlight(glm::vec2(30.f, 27.f), InputBinding::Right);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString("Aim Right");
            centreText(infoEnt);
        });

    //swing
    entity = createHighlight(glm::vec2(152.f, 41.f), InputBinding::Action);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = uiSystem.addCallback(
        [infoEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            infoEnt.getComponent<cro::Text>().setString("Take Shot");
            centreText(infoEnt);
        });


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
        e.addComponent<cro::Transform>().setPosition(position);
        e.addComponent<cro::Drawable2D>();
        e.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
        e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function = LabelCallback(bindingIndex, m_sharedData.inputBinding);
        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
    };

    //prev club
    createLabel(glm::vec2(52.f, 94.f), InputBinding::PrevClub);

    //next club
    createLabel(glm::vec2(140.f, 94.f), InputBinding::NextClub);

    //stroke
    createLabel(glm::vec2(160.f, 59.f), InputBinding::Action);

    //left
    createLabel(glm::vec2(20.f, 50.f), InputBinding::Left);
    
    //right
    createLabel(glm::vec2(20.f, 35.f), InputBinding::Right);
}

void OptionsState::createButtons(cro::Entity parent, std::int32_t menuID, std::uint32_t selectedID, std::uint32_t unselectedID)
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

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

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //apply
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(103.f, 14.f));
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

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //close
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(glm::vec2(154.f, 14.f));
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

    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void OptionsState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
}