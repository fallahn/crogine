/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

#include "NewsState.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "TextAnimCallback.hpp"
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
#include <crogine/util/String.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <AchievementStrings.hpp>

namespace
{
    struct MenuID final
    {
        enum
        {
            Main, Confirm
        };
    };
    constexpr std::size_t TitleButtonIndex = 0;
    constexpr std::size_t QuitButtonIndex = 1000;
}

NewsState::NewsState(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd),
    m_viewScale (2.f)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool NewsState::handleEvent(const cro::Event& evt)
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
        if (evt.cbutton.button == cro::GameController::ButtonB
            || evt.cbutton.button == cro::GameController::ButtonStart
            || evt.cbutton.button == cro::GameController::ButtonBack)
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

void NewsState::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool NewsState::simulate(float dt)
{
    m_scene.simulate(dt);
    return true;
}

void NewsState::render()
{
    m_scene.render();
}

//private
void NewsState::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    m_scene.addSystem<cro::UISystem>(mb);// ->setActiveControllerID(m_sharedData.inputBinding.controllerID);
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

                m_scene.getSystem<cro::UISystem>()->selectByIndex(TitleButtonIndex);
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
    spriteSheet.loadFromFile("assets/golf/sprites/news.spt", m_sharedData.sharedResources->textures);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("border");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    rootNode.getComponent<cro::Transform >().addChild(entity.getComponent<cro::Transform>());

    auto menuEntity = m_scene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    rootNode.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());


    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 143.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("News");
    entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(entity);
    menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -108.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont);
#ifdef USE_GNS
    entity.getComponent<cro::Text>().setString("Click title to see more (opens in Steam overlay)");
#else
    entity.getComponent<cro::Text>().setString("Click title to see more (opens in your default browser)");
#endif
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    centreText(entity);
    menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto& uiSystem = *m_scene.getSystem<cro::UISystem>();

    auto selectedID = uiSystem.addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour); 
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;

            auto [rect, ent] = e.getComponent<cro::Callback>().getUserData<std::pair<cro::FloatRect, cro::Entity>>();
            if (ent.isValid())
            {
                ent.getComponent<cro::Sprite>().setTextureRect(rect);
            }
        });
    auto unselectedID = uiSystem.addCallback(
        [](cro::Entity e) 
        { 
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });
    
    const auto createItem = [&, selectedID, unselectedID](glm::vec2 position, const std::string& label, cro::Entity parent) 
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
        //used by selection callback to see if we need to set a thumbnail
        e.getComponent<cro::Callback>().setUserData<std::pair<cro::FloatRect, cro::Entity>>();

        parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
        return e;
    };

    const auto createSmallItem = [&, selectedID, unselectedID](glm::vec2 position, const std::string& label, cro::Entity parent)
        {
            auto e = m_scene.createEntity();
            e.addComponent<cro::Transform>().setPosition(position);
            e.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
            e.addComponent<cro::Drawable2D>();
            e.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
            e.getComponent<cro::Text>().setString(label);
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
            e.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(e);
            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectedID;
            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectedID;

            e.addComponent<cro::Callback>().function = MenuTextCallback();
            e.getComponent<cro::Callback>().setUserData<std::pair<cro::FloatRect, cro::Entity>>();

            parent.getComponent<cro::Transform>().addChild(e.getComponent<cro::Transform>());
            return e;
        };

    spriteSheet.loadFromFile("assets/golf/sprites/connect_menu.spt", m_sharedData.sharedResources->textures);

    auto balls = m_scene.createEntity();
    balls.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    balls.addComponent<cro::Drawable2D>();
    balls.addComponent<cro::Sprite>() = spriteSheet.getSprite("bounce");
    balls.addComponent<cro::SpriteAnimation>().play(0);
    bounds = balls.getComponent<cro::Sprite>().getTextureBounds();
    balls.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    rootNode.getComponent<cro::Transform>().addChild(balls.getComponent<cro::Transform>());

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 24.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Fetching News...");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto newsEnt = entity;

#ifdef RSS_ENABLED
    m_feed.fetchAsync(Social::RSSFeed);
#endif


    entity = m_scene.createEntity();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, menuEntity, createItem, balls, newsEnt](cro::Entity e, float) mutable
    {
#ifdef RSS_ENABLED
        if (m_feed.fetchComplete())
        {
            const auto& items = m_feed.getItems();
            glm::vec3 position(0.f, 24.f, 0.1f);

            if (!items.empty())
            {
                auto title = items[0].title;
                auto rowCount = cro::Util::String::wordWrap(title, 25);

                auto ent = createItem({ 0.f, ((rowCount - 1) * 8.f) + 110.f }, title, menuEntity);
                auto url = items[0].url;
                ent.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
                ent.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
                ent.getComponent<cro::Text>().setVerticalSpacing(1.f);
                ent.getComponent<cro::Text>().setFillColour(TextGoldColour);
                ent.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
                ent.getComponent<cro::Transform>().setOrigin(glm::vec2(0.f));
                ent.getComponent<cro::UIInput>().setGroup(MenuID::Main);
                ent.getComponent<cro::UIInput>().setSelectionIndex(TitleButtonIndex);
                ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
                    uiSystem.addCallback([&, url](cro::Entity e, cro::ButtonEvent evt)
                        {
                            if (activated(evt))
                            {
#ifdef USE_GNS
                                Social::showWebPage(url);
#else
                                cro::Util::String::parseURL(url);
#endif
                            }
                        });

                ent = m_scene.createEntity();
                ent.addComponent<cro::Transform>().setPosition({ -170.f, 94.f, 0.1f });
                ent.addComponent<cro::Drawable2D>();
                ent.addComponent<cro::Text>(smallFont).setString(items[0].date);
                ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
                ent.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
                menuEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

                auto description = items[0].description.substr(0, 180) + "...";
                cro::Util::String::wordWrap(description, 60);


                ent = m_scene.createEntity();
                ent.addComponent<cro::Transform>().setPosition({ -170.f, 74.f, 0.1f });
                ent.addComponent<cro::Drawable2D>();
                ent.addComponent<cro::Text>(smallFont).setString(description);
                ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
                ent.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
                menuEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());

#ifdef USE_GNS
                static constexpr std::size_t MaxItems = 1;
#else
                static constexpr std::size_t MaxItems = 6;
#endif
                for (auto i = 1u; i < items.size() && i < MaxItems; ++i)
                {
                    ent = createItem(position, items[i].title, menuEntity);
                    url = items[i].url;
                    ent.getComponent<cro::UIInput>().setGroup(MenuID::Main);
                    ent.getComponent<cro::UIInput>().setSelectionIndex(i);
                    ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
                        uiSystem.addCallback([&, url](cro::Entity e, cro::ButtonEvent evt)
                            {
                                if (activated(evt))
                                {
                                    cro::Util::String::parseURL(url);
                                }
                            });

                    position.y -= 10.f;

                    ent = m_scene.createEntity();
                    ent.addComponent<cro::Transform>().setPosition(position);
                    ent.addComponent<cro::Drawable2D>();
                    ent.addComponent<cro::Text>(smallFont).setString(items[i].date);
                    ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
                    ent.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
                    centreText(ent);
                    menuEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                    position.y -= 14.f;
                }
            }
            else
            {
                auto ent = m_scene.createEntity();
                ent.addComponent<cro::Transform>().setPosition(position);
                ent.addComponent<cro::Drawable2D>();
                ent.addComponent<cro::Text>(font).setString("No news found");
                ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
                ent.getComponent<cro::Text>().setCharacterSize(UITextSize);
                centreText(ent);
                menuEntity.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
            }

            e.getComponent<cro::Callback>().active = false;
            m_scene.destroyEntity(e);
            m_scene.destroyEntity(balls);
            m_scene.destroyEntity(newsEnt);
            m_scene.getSystem<cro::UISystem>()->selectByIndex(TitleButtonIndex);
        }
#endif
    };

#ifdef USE_GNS
    //section thumbnail
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -6.f, -86.f, 0.1f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.25f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_sharedData.sharedResources->textures.get("assets/golf/images/news_thumbs.png"));
    bounds = entity.getComponent<cro::Sprite>().getTextureRect();
    bounds.height /= 2.f;
    entity.getComponent<cro::Sprite>().setTextureRect(bounds);
    menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto thumbEnt = entity;

    //section titles
    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -170.f, 8.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextEditColour);
    entity.getComponent<cro::Text>().setString("Community\n\n\n\n\n\nGuides");
    menuEntity.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto titleEnt = entity;
    entity = createSmallItem(glm::vec2(8.f, -12.f), "Join The Chat Room", titleEnt);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitButtonIndex - 5);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    Social::joinChatroom();
                }
            });
    entity.getComponent<cro::Callback>().setUserData<std::pair<cro::FloatRect, cro::Entity>>(bounds,thumbEnt);

    entity = createSmallItem(glm::vec2(8.f, -23.f), "Join The Clubhouse", titleEnt);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitButtonIndex - 4);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    Social::showWebPage("https://steamcommunity.com/groups/supervideoclubhouse");
                }
            });
    bounds.bottom += bounds.height;
    entity.getComponent<cro::Callback>().setUserData<std::pair<cro::FloatRect, cro::Entity>>(bounds, thumbEnt);

    entity = createSmallItem(glm::vec2(8.f, -60.f), "Getting Started", titleEnt);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitButtonIndex - 3);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    Social::showWebPage("https://steamcommunity.com/sharedfiles/filedetails/?id=2910207511");
                }
            });
    entity = createSmallItem(glm::vec2(8.f, -71.f), "How To Add Custom Music", titleEnt);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitButtonIndex - 2);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    Social::showWebPage("https://steamcommunity.com/sharedfiles/filedetails/?id=3013809801");
                }
            });
    entity = createSmallItem(glm::vec2(8.f, -82.f), "Using The Steam Workshop", titleEnt);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitButtonIndex - 1);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    Social::showWebPage("https://steamcommunity.com/sharedfiles/filedetails/?id=2925549958");
                }
            });

#endif


    //quit state
    entity = createItem(glm::vec2(0.f, -125.f), "Let's go!", menuEntity);
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
    entity.getComponent<cro::UIInput>().setSelectionIndex(QuitButtonIndex);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        uiSystem.addCallback([&](cro::Entity e, cro::ButtonEvent evt)
            {
                if (activated(evt))
                {
                    quitState();
                }
            });
    centreText(entity);

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

void NewsState::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}
