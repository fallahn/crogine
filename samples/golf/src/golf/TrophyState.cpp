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

#include "TrophyState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
#include "../AchievementStrings.hpp"
#include "../GolfGame.hpp"

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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{

}

TrophyState::TrophyState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State    (ss, ctx),
    m_scene         (ctx.appInstance.getMessageBus()),
    m_sharedData    (sd),
    m_trophyIndex   (1),
    m_viewScale     (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool TrophyState::handleEvent(const cro::Event& evt)
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
    else if (evt.type == SDL_CONTROLLERBUTTONUP
        && evt.cbutton.which == cro::GameController::deviceID(m_sharedData.inputBinding.controllerID))
    {
        if (evt.cbutton.button == cro::GameController::ButtonB
            || evt.cbutton.button == cro::GameController::ButtonStart)
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

    m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void TrophyState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool TrophyState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void TrophyState::render()
{
    m_scene.render();
}

//private
void TrophyState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb)->setActiveControllerID(m_sharedData.inputBinding.controllerID);
    m_scene.addSystem<cro::CommandSystem>(mb);
    m_scene.addSystem<cro::CallbackSystem>(mb);
    m_scene.addSystem<cro::SpriteSystem2D>(mb);
    m_scene.addSystem<cro::SpriteAnimator>(mb);
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
    spriteSheet.loadFromFile("assets/golf/sprites/billiards_ui.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("trophy_background");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto backgroundEnt = entity;

    auto& uiFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& infoFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White); 
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e) 
        { 
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    
    auto bgBounds = bounds;


    auto titleEnt = m_scene.createEntity();
    titleEnt.addComponent<cro::Transform>().setPosition({ bgBounds.width / 2.f, 293.f, 0.1f });
    titleEnt.addComponent<cro::Drawable2D>();
    titleEnt.addComponent<cro::Text>(uiFont).setCharacterSize(UITextSize);
    titleEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    backgroundEnt.getComponent<cro::Transform>().addChild(titleEnt.getComponent<cro::Transform>());

    auto descEnt = m_scene.createEntity();
    descEnt.addComponent<cro::Transform>().setPosition({ 58.f, 266.f, 0.1f });
    descEnt.addComponent<cro::Drawable2D>();
    descEnt.addComponent<cro::Text>(infoFont).setCharacterSize(InfoTextSize);
    descEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    backgroundEnt.getComponent<cro::Transform>().addChild(descEnt.getComponent<cro::Transform>());

    auto iconEnt = m_scene.createEntity();
    iconEnt.addComponent<cro::Transform>().setPosition({ 18.f, 238.f, 0.1f });
    iconEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.5f));
    iconEnt.addComponent<cro::Drawable2D>();
    iconEnt.addComponent<cro::Sprite>().setTexture(m_sharedData.sharedResources->textures.get("assets/images/achievements.png"));
    backgroundEnt.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());

    auto updateText = [&, titleEnt, descEnt, iconEnt]() mutable
    {
        titleEnt.getComponent<cro::Text>().setString(AchievementLabels[m_trophyIndex]);
        centreText(titleEnt);

        auto [descString, hidden] = AchievementDesc[m_trophyIndex];
        if (!hidden || Achievements::getAchievement(AchievementStrings[m_trophyIndex])->achieved)
        {
            auto end = std::min(descString.length() - 1, std::size_t(26));
            auto pos = descString.substr(0, end).find_last_of(' ');
            if (pos != std::string::npos)
            {
                descString[pos] = '\n';
            }
        }
        else
        {
            descString = "????";
        }
        descEnt.getComponent<cro::Text>().setString(descString);

        static constexpr std::size_t RowCount = 5;
        auto x = m_trophyIndex % RowCount;
        auto y = m_trophyIndex / RowCount;

        if (hidden ||
            !Achievements::getAchievement(AchievementStrings[m_trophyIndex])->achieved)
        {
            x = y = 0;
        }

        static constexpr float IconWidth = 64.f;
        glm::vec2 texSize(iconEnt.getComponent<cro::Sprite>().getTexture()->getSize());
        cro::FloatRect textureRect(x * IconWidth, y * IconWidth, IconWidth, IconWidth);

        iconEnt.getComponent<cro::Sprite>().setTextureRect(textureRect);
    };
    updateText();


    //arrow left
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 19.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_left");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown ] =
        uiSystem.addCallback([&, updateText](cro::Entity e, const cro::ButtonEvent& evt) mutable
    {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_trophyIndex = ((m_trophyIndex + (AchievementID::Count - 1)) % AchievementID::Count);
                    if (m_trophyIndex == 0)
                    {
                        m_trophyIndex = AchievementID::Count - 1;
                    }
                    updateText();
                }
    });
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //quit button
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bgBounds.width / 2.f, 24.f, 0.1f});
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("close_button");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    auto highlightBounds = spriteSheet.getSprite("close_highlight").getTextureRect();
    auto normalBounds = spriteSheet.getSprite("close_button").getTextureRect();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem.addCallback([highlightBounds](cro::Entity e) 
            {
                e.getComponent<cro::Sprite>().setTextureRect(highlightBounds);
                e.getComponent<cro::AudioEmitter>().play();
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem.addCallback([normalBounds](cro::Entity e) 
            {
                e.getComponent<cro::Sprite>().setTextureRect(normalBounds);
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });

    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //arrow right
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 168.f, 19.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_right");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&, updateText](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_trophyIndex = ((m_trophyIndex + 1) % AchievementID::Count);
                    
                    //there's probably a smart way to do this, but brain.
                    if (m_trophyIndex == 0)
                    {
                        m_trophyIndex++;
                    }
                    
                    updateText();
                }
            });
    backgroundEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
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

void TrophyState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}