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

#include "UnlockState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "UnlockItems.hpp"

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/gui/Gui.hpp>

#include <crogine/ecs/components/Transform.hpp>
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

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace
{
    struct ItemCallbackData final
    {
        enum
        {
            In, Hold, Out
        }state = In;
        float stateTime = 0.f;

        bool close()
        {
            if (state == Hold)
            {
                state = Out;
                stateTime = 0.f;
                return true;
            }
            return false;
        }
    };
}

UnlockState::UnlockState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool UnlockState::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    const auto dismissItem =
        [&]()
    {
        if (m_itemIndex < m_unlockCollections.size())
        {
            auto& item = m_unlockCollections[m_itemIndex].root.getComponent<cro::Callback>().getUserData<ItemCallbackData>();
            if (item.close())
            {
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    };

    if (evt.type == SDL_KEYUP)
    {
        switch (evt.key.keysym.sym)
        {
        default: break;
        case SDLK_BACKSPACE:
        case SDLK_ESCAPE:
            quitState();
            return false;
        case SDLK_SPACE: //TODO probably should check keybinds but that breaks my lovely switch block :(
            dismissItem();
            break;
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
        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::ButtonA:
            dismissItem();
            break;
        case cro::GameController::ButtonB:
        case cro::GameController::ButtonBack:
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

    m_scene.forwardEvent(evt);
    return false;
}

void UnlockState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool UnlockState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void UnlockState::render()
{
    m_scene.render();
}

//private
void UnlockState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
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
    m_audioEnts[AudioID::Fireworks] = m_scene.createEntity();
    m_audioEnts[AudioID::Fireworks].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("firework");
    m_audioEnts[AudioID::Cheer] = m_scene.createEntity();
    m_audioEnts[AudioID::Cheer].addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("unlock");

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


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/unlocks.spt", m_sharedData.sharedResources->textures);

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    const auto createItem = [&](std::int32_t unlockID)
    {
        auto& collection = m_unlockCollections.emplace_back();

        auto entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
        entity.getComponent<cro::Transform>().setPosition({ 0.f, -30.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
        auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.addComponent<cro::Callback>().setUserData<ItemCallbackData>();
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float dt)
        {
            static constexpr float InTime = 0.8f;
            static constexpr float HoldTime = 10.f;
            static constexpr float OutTime = InTime;

            auto& collection = m_unlockCollections[m_itemIndex];

            auto& data = e.getComponent<cro::Callback>().getUserData<ItemCallbackData>();
            data.stateTime += dt;

            switch (data.state)
            {
            default: break;
            case ItemCallbackData::In:
            {
                const float scale = std::min(1.f, data.stateTime / InTime);
                
                const float bgScale = cro::Util::Easing::easeOutElastic(scale);
                e.getComponent<cro::Transform>().setScale(glm::vec2(bgScale));
                e.getComponent<cro::Transform>().setRotation(bgScale * cro::Util::Const::TAU);

                collection.title.getComponent<cro::Transform>().setScale(glm::vec2(bgScale));

                const float windowWidth = static_cast<float>(cro::App::getWindow().getSize().x) * 2.f;
                const float textScale = cro::Util::Easing::easeInExpo(1.f - scale);
                auto pos = collection.description.getComponent<cro::Transform>().getPosition();
                pos.x = std::floor(windowWidth * textScale);
                collection.description.getComponent<cro::Transform>().setPosition(pos);
                collection.description.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                pos = collection.name.getComponent<cro::Transform>().getPosition();
                pos.x = std::floor(-windowWidth * textScale);
                collection.name.getComponent<cro::Transform>().setPosition(pos);
                collection.name.getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                if (data.stateTime > InTime)
                {
                    data.stateTime = 0.f;
                    data.state = ItemCallbackData::Hold;

                    //TODO activate other callbacks
                    //TODO activate fireworks

                    m_audioEnts[AudioID::Fireworks].getComponent<cro::AudioEmitter>().play();
                }
            }
                break;
            case ItemCallbackData::Hold:
                if (data.stateTime > HoldTime)
                {
                    data.stateTime = 0.f;
                    data.state = ItemCallbackData::Out;
                }
                break;
            case ItemCallbackData::Out:
            {
                const float scale = std::min(1.f, data.stateTime / OutTime);
                const float bgScale = cro::Util::Easing::easeOutElastic(1.f - scale);
                e.getComponent<cro::Transform>().setScale(glm::vec2(bgScale));
                e.getComponent<cro::Transform>().setRotation(bgScale * cro::Util::Const::TAU);

                collection.title.getComponent<cro::Transform>().setScale(glm::vec2(bgScale));

                const float windowWidth = static_cast<float>(cro::App::getWindow().getSize().x) * 2.f;
                const float textScale = cro::Util::Easing::easeInExpo(scale);
                auto pos = collection.description.getComponent<cro::Transform>().getPosition();
                pos.x = std::floor(-windowWidth * textScale);
                collection.description.getComponent<cro::Transform>().setPosition(pos);

                pos = collection.name.getComponent<cro::Transform>().getPosition();
                pos.x = std::floor(windowWidth * textScale);
                collection.name.getComponent<cro::Transform>().setPosition(pos);

                if (data.stateTime > OutTime)
                {
                    m_scene.destroyEntity(collection.root);
                    m_scene.destroyEntity(collection.description);
                    m_scene.destroyEntity(collection.name);
                    m_scene.destroyEntity(collection.title);
                    //TODO tidy up other items in collection

                    m_itemIndex++;

                    if (m_itemIndex < m_unlockCollections.size())
                    {
                        m_unlockCollections[m_itemIndex].root.getComponent<cro::Callback>().active = true;
                    }
                    else
                    {
                        quitState();
                    }
                }
            }
                break;
            }
        };
        collection.root = entity;

        m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


        //title text
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 132.f, 0.1f });
        entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        collection.title = entity;

        //unlock description
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, 82.f, 0.1f });
        entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(largeFont).setString(ul::Items[unlockID].description);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize * 2);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        centreText(entity);
        m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        collection.description = entity;

        //item preview
        //TODO

        //unlock name
        entity = m_scene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, -102.f, 0.1f });
        entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString(ul::Items[unlockID].name);
        entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize * 2);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        centreText(entity);
        m_rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        collection.name = entity;
    };


    for (auto i : m_sharedData.unlockedItems)
    {
        createItem(i);
    }

    //small delay
    entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime += dt;
        if (currTime > 1.f)
        {
            m_audioEnts[AudioID::Cheer].getComponent<cro::AudioEmitter>().play();

            m_unlockCollections[0].root.getComponent<cro::Callback>().active = true;
            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
        }
    };


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

void UnlockState::quitState()
{
    m_sharedData.unlockedItems.clear();

    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}