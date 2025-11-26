/*-----------------------------------------------------------------------

Matt Marchant 2025
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

#include "OptionsStateV2.hpp"
#include "SharedStateData.hpp"
#include "CommonConsts.hpp"
#include "CommandIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "../GolfGame.hpp"

#include <Achievements.hpp>
#include <AchievementStrings.hpp>

#include <crogine/core/Window.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/graphics/SimpleText.hpp>

#include <crogine/ecs/components/Transform.hpp>
//#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/UIElement.hpp>
//#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

//#include <crogine/ecs/systems/UISystem.hpp>
#include <crogine/ecs/systems/UIElementSystem.hpp>
//#include <crogine/ecs/systems/CommandSystem.hpp>
#include <crogine/ecs/systems/CallbackSystem.hpp>
#include <crogine/ecs/systems/SpriteSystem2D.hpp>
#include <crogine/ecs/systems/TextSystem.hpp>
#include <crogine/ecs/systems/CameraSystem.hpp>
#include <crogine/ecs/systems/RenderSystem2D.hpp>
#include <crogine/ecs/systems/AudioPlayerSystem.hpp>

#include <crogine/util/Easings.hpp>

#include <crogine/detail/glm/gtc/matrix_transform.hpp>
#include <crogine/detail/OpenGL.hpp>

#include <filesystem>

namespace
{
    const std::array ItemLabels =
    {
        "Settings", "Keyboard", "Controller",
        "Display", "Audio", "Achievements",
        "Stats"
    };

    constexpr float TabBarHeight = 16.f;

    constexpr float ItemHeight = TabBarHeight * 3.f;
    constexpr float ItemSpacing = 4.f;

    constexpr float InfoBarHeight = 24.f; //space at the bottom
}

OptionsStateV2::OptionsStateV2(cro::StateStack& ss, cro::State::Context ctx, SharedStateData& sd)
    : cro::State(ss, ctx),
    m_scene     (ctx.appInstance.getMessageBus()),
    m_sharedData(sd)
{
    ctx.mainWindow.setMouseCaptured(false);

    buildScene();
}

//public
bool OptionsStateV2::handleEvent(const cro::Event& evt)
{
    if (ImGui::GetIO().WantCaptureKeyboard
        || ImGui::GetIO().WantCaptureMouse
        || m_rootNode.getComponent<cro::Callback>().active)
    {
        return false;
    }

    const auto setActiveInput =
        [&](bool mouse, std::int32_t controllerIndex)
        {
            if (mouse)
            {
                m_sharedData.activeInput = SharedStateData::ActiveInput::Keyboard;
                cro::App::getWindow().setMouseCaptured(false);
            }
            else
            {
                cro::App::getWindow().setMouseCaptured(true);
                m_sharedData.activeInput = cro::GameController::hasPSLayout(controllerIndex)
                    ? SharedStateData::ActiveInput::PS : SharedStateData::ActiveInput::XBox;
            }
        };


    if (evt.type == SDL_KEYUP)
    {
        setActiveInput(true, 0);

        if (evt.key.keysym.sym == SDLK_BACKSPACE
            || evt.key.keysym.sym == SDLK_ESCAPE
            || evt.key.keysym.sym == SDLK_p)
        {
            quitState();
            return false;
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::NextClub])
        {
            nextTab();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::PrevClub])
        {
            prevTab();
        }

        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Down]
            || evt.key.keysym.sym == SDLK_DOWN)
        {
            nextItem();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Up]
            || evt.key.keysym.sym == SDLK_UP)
        {
            prevItem();
        }

        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Left]
            || evt.key.keysym.sym == SDLK_LEFT)
        {
            activateLeft();
        }
        else if (evt.key.keysym.sym == m_sharedData.inputBinding.keys[InputBinding::Right]
            || evt.key.keysym.sym == SDLK_RIGHT)
        {
            activateRight();
        }

    }
    else if (evt.type == SDL_CONTROLLERBUTTONUP)
    {
        setActiveInput(false, cro::GameController::controllerID(evt.cbutton.which));

        switch (evt.cbutton.button)
        {
        default: break;
        case cro::GameController::DPadUp:
            prevItem();
            break;
        case cro::GameController::DPadDown:
            nextItem();
            break;
        case cro::GameController::DPadLeft:
            activateLeft();
            break;
        case cro::GameController::DPadRight:
            activateRight();
            break;
        case cro::GameController::ButtonLeftShoulder:
            prevTab();
            break;
        case cro::GameController::ButtonRightShoulder:
            nextTab();
            break;
        case cro::GameController::ButtonX:
            //TODO credits
            break;
        case cro::GameController::ButtonY:
            //TODO how to play
            break;
        case cro::GameController::ButtonB:
            quitState();
            return false;
        }
    }

    else if (evt.type == SDL_MOUSEBUTTONUP)
    {
        //TODO test clicks for tab bar buttons
        //TODO test clicks for menu items

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

    else if (evt.type == SDL_MOUSEMOTION)
    {
        setActiveInput(true, 0);
    }
    else if (evt.type == SDL_CONTROLLERAXISMOTION)
    {
        setActiveInput(false, cro::GameController::controllerID(evt.caxis.which));

        //TODO parse cursor movement
    }
    else if (evt.type == SDL_MOUSEWHEEL)
    {
        if (evt.wheel.y > 0)
        {
            prevItem();
        }
        else if (evt.wheel.y < 0)
        {
            nextItem();
        }
    }

    //m_scene.getSystem<cro::UISystem>()->handleEvent(evt);
    m_scene.forwardEvent(evt);
    return false;
}

void OptionsStateV2::handleMessage(const cro::Message& msg)
{
    m_scene.forwardMessage(msg);
}

bool OptionsStateV2::simulate(float dt)
{
    //TODO update menu scroll position using ORIGIN offset
    //remember to account for window/sprite scale

    m_scene.simulate(dt);
    return true;
}

void OptionsStateV2::render()
{
    m_scene.render();
}

//private
void OptionsStateV2::buildScene()
{
    auto& mb = getContext().appInstance.getMessageBus();
    //m_scene.addSystem<cro::UISystem>(mb);
    m_scene.addSystem<cro::UIElementSystem>(mb);
    //m_scene.addSystem<cro::CommandSystem>(mb);
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
            e.getComponent<cro::Transform>().setScale(glm::vec2(cro::Util::Easing::easeOutQuint(currTime)));
            if (currTime == 1)
            {
                state = RootCallbackData::FadeOut;
                e.getComponent<cro::Callback>().active = false;
            }
            break;
        case RootCallbackData::FadeOut:
            currTime = std::max(0.f, currTime - (dt * 2.f));
            e.getComponent<cro::Transform>().setScale(glm::vec2(cro::Util::Easing::easeOutQuint(currTime)));
            if (currTime == 0)
            {
                state = RootCallbackData::FadeIn;
                e.getComponent<cro::Callback>().active = false;
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
        scale = std::min(1.f, scale);

        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(BackgroundAlpha * scale);
        }
    };

   
    //TODO background needs a 9-patch?
    

    //tab bar - we only create here, cahedPush() will update the drawable
    m_tabBar.background = m_scene.createEntity();
    m_tabBar.background.addComponent<cro::Transform>();
    m_tabBar.background.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    m_tabBar.background.addComponent<cro::UIElement>(cro::UIElement::Position, true);
    m_tabBar.background.getComponent<cro::UIElement>().relativePosition = { -0.5f, 0.5f };
    m_tabBar.background.getComponent<cro::UIElement>().absolutePosition = { 0.f, -(TabBarHeight * 2.f) };
    rootNode.getComponent<cro::Transform>().addChild(m_tabBar.background.getComponent<cro::Transform>());

    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info); //TODO insert the controller icon font into this one?
    const float Spacing = 1.f / (TabBar::Item::Count + 2); //leave equivalent of a tab either end
    for (auto i = 0; i < TabBar::Item::Count; ++i)
    {
        auto& item = m_tabBar.items[i];
        item.text = m_scene.createEntity();
        item.text.addComponent<cro::Transform>();
        item.text.addComponent<cro::Drawable2D>();
        item.text.addComponent<cro::Text>(smallFont).setString(ItemLabels[i]);
        item.text.getComponent<cro::Text>().setFillColour(TextNormalColour);
        item.text.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);

        auto& uiElement = item.text.addComponent<cro::UIElement>(cro::UIElement::Text, true);
        uiElement.characterSize = InfoTextSize;
        uiElement.depth = 0.1f;
        //hmm this is ignored for text types, have to update manually in callback
        //uiElement.relativePosition = { Spacing + (Spacing * i), 0.f };
        const float offset = (Spacing * 1.5f) + (Spacing * i);
        uiElement.resizeCallback = 
            [&, offset](cro::Entity e)
            {
                const auto x = std::round((static_cast<float>(cro::App::getWindow().getSize().x) / cro::UIElementSystem::getViewScale()) * offset);
                const auto y = 12.f;
                e.getComponent<cro::UIElement>().absolutePosition = { x,y };
            };

        m_tabBar.background.getComponent<cro::Transform>().addChild(item.text.getComponent<cro::Transform>());
    }


    //menu layout
    createSettingsItems();
    createKeyboardItems();
    createControllerItems();
    createDisplayItems();
    createAudioItems();
    createAchievementItems();
    createStatItems();

    m_menuLayout.sprite = m_scene.createEntity();
    m_menuLayout.sprite.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.2f });
    m_menuLayout.sprite.addComponent<cro::Drawable2D>();
    m_menuLayout.sprite.addComponent<cro::Sprite>();
    rootNode.getComponent<cro::Transform>().addChild(m_menuLayout.sprite.getComponent<cro::Transform>());

    updateTabBar(); //this also updates the menu items

    //camera settings
    auto updateView = [&, rootNode](cro::Camera& cam) mutable
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        rootNode.getComponent<cro::Transform>().setPosition(size / 2.f);
        auto& tx = m_menuLayout.sprite.getComponent<cro::Transform>();
        auto pos = tx.getPosition();
        pos.x = -(size.x / 2.f);
        pos.y = -(size.y / 2.f);
        tx.setPosition(pos);

        refreshView();
    };

    entity = m_scene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_scene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void OptionsStateV2::createSettingsItems()
{
    //TODO specialise menu items eg for section titles
    const auto count = 31;

    for (auto i = 0; i < count; ++i)
    {
        auto& item = m_menuLayout.items[0].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createKeyboardItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[1].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createControllerItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[2].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createDisplayItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[3].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createAudioItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[4].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createAchievementItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[5].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::createStatItems()
{
    for (auto i = 0; i < 5; ++i)
    {
        auto& item = m_menuLayout.items[6].emplace_back();
        item.itemTitle = "Dummy Item";
        item.description = "This is the item description for " + std::to_string(i + 1);
        item.callback = [](Menu::Item& i) {LogI << "Callback!" << std::endl; };
        item.itemCount = cro::Util::Random::value(2, 4);
        for (auto j = 0; j < item.itemCount; ++j)
        {
            item.itemLabels.push_back("Option " + std::to_string(j + 1));
        }
    }
}

void OptionsStateV2::onCachedPush()
{
    refreshView();

    m_rootNode.getComponent<cro::Callback>().active = true;
}

void OptionsStateV2::onCachedPop()
{

}

void OptionsStateV2::updateTabBar()
{
    const auto WindowX = static_cast<float>(cro::App::getWindow().getSize().x);

    const float Spacing = 1.f / (TabBar::Item::Count + 2); //leave equivalent of a tab either end
    const float TabWidth = std::round(Spacing * WindowX);

    std::vector<cro::Vertex2D> verts;
    const auto addQuad = 
        [&](cro::Colour c, glm::vec2 position, glm::vec2 size)
        {
            verts.emplace_back(glm::vec2(position.x, position.y + size.y), c);
            verts.emplace_back(position, c);
            verts.emplace_back(position + size, c);

            verts.emplace_back(position + size, c);
            verts.emplace_back(position, c);
            verts.emplace_back(glm::vec2(position.x + size.x, position.y), c);
        };

    //update the verts for the tab bar.
    const auto viewScale = cro::UIElementSystem::getViewScale();
    for (auto i = 0u; i < m_tabBar.items.size(); ++i)
    {
        const auto active = m_tabBar.activeIndex;

        const auto colour = i == active ? CD32::Colours[CD32::Brown] : CD32::Colours[CD32::TanDarkest];
        addQuad(colour, { TabWidth + (i * TabWidth), 0.f }, { TabWidth - viewScale, TabBarHeight * viewScale});

        m_tabBar.items[i].text.getComponent<cro::Text>().setFillColour(i == active ? TextNormalColour : CD32::Colours[CD32::BeigeMid]);
    }

    addQuad(CD32::Colours[CD32::Brown], { 0.f, -viewScale }, { WindowX, viewScale });

    m_tabBar.background.getComponent<cro::Drawable2D>().setVertexData(verts);

    updateMenuItems();
}

void OptionsStateV2::nextTab()
{
    m_tabBar.activeIndex = (m_tabBar.activeIndex + 1) % TabBar::Item::Count;
    refreshView();
    LogI << "Add sound here" << std::endl;
}

void OptionsStateV2::prevTab()
{
    m_tabBar.activeIndex = (m_tabBar.activeIndex + (TabBar::Item::Count - 1)) % TabBar::Item::Count;
    refreshView();
    LogI << "Add sound here" << std::endl;
}

void OptionsStateV2::updateMenuItems()
{
    //NOTE this is all done 1:1 scale and the resulting sprite set to window scale
    const auto& items = m_menuLayout.items[m_tabBar.activeIndex];
    const auto viewScale = cro::UIElementSystem::getViewScale();

    //calc max texture size and resize first if necessary
    const auto texHeight = static_cast<std::uint32_t>(((ItemHeight + ItemSpacing) * items.size() + ItemSpacing));
    const auto texWidth = static_cast<std::uint32_t>(static_cast<float>(cro::App::getWindow().getSize().x) / viewScale) / 2u;

    if (!m_menuLayout.texture.available()
        || texWidth > m_menuLayout.texture.getSize().x
        || texHeight > m_menuLayout.texture.getSize().y)
    {
        m_menuLayout.texture.create(texWidth, texHeight, false);
    }

    //if we didn't resize the actual size might be bigger than we expect on other tabs...
    const glm::vec2 renderSize = glm::vec2(m_menuLayout.texture.getSize());

    m_menuLayout.sprite.getComponent<cro::Sprite>().setTexture(m_menuLayout.texture.getTexture());
    m_menuLayout.sprite.getComponent<cro::Transform>().setScale(glm::vec2(viewScale));

    cro::FloatRect crop = { 0.f, InfoBarHeight * viewScale,
                            renderSize.x * viewScale, 
                            (m_tabBar.background.getComponent<cro::Transform>().getPosition().y - (InfoBarHeight * viewScale)) + (cro::App::getWindow().getSize().y / 2)};
    m_menuLayout.sprite.getComponent<cro::Drawable2D>().setCroppingArea(crop, true);

    const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    cro::SimpleText text(font);
    text.setFillColour(TextNormalColour);
    text.setCharacterSize(InfoTextSize);

    constexpr float LineSpacing = 12.f;
    const auto renderItem =
        [&](const Menu::Item& item, glm::vec2 pos)
        {
            text.setPosition(pos);
            text.setString(item.itemTitle);
            text.draw();
            text.move({ 0.f, -LineSpacing });
            text.setString(item.itemLabels[item.itemIndex]);
            text.draw();
        };

    constexpr float Stride = ItemHeight + ItemSpacing;
    glm::vec2 pos = { 4.f, renderSize.y - Stride };

    m_menuLayout.texture.clear(cro::Colour::Blue);
    //render current item selection to render texture
    //this includes either setting item highlight colour or rendering a highlight box
    
    for (const auto& item : items)
    {
        renderItem(item, pos);
        pos.y -= Stride;
    }

    m_menuLayout.texture.display();
}

void OptionsStateV2::nextItem()
{
    m_menuLayout.itemIndex = (m_menuLayout.itemIndex + 1) % m_menuLayout.items[m_tabBar.activeIndex].size();
    updateMenuItems();

    LogI << "Play Sound Here" << std::endl;
}

void OptionsStateV2::prevItem()
{
    m_menuLayout.itemIndex = (m_menuLayout.itemIndex + (m_menuLayout.items[m_tabBar.activeIndex].size() - 1)) % m_menuLayout.items[m_tabBar.activeIndex].size();
    updateMenuItems();

    LogI << "Play Sound Here" << std::endl;
}

void OptionsStateV2::activateLeft()
{
    m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activateLeft();
    updateMenuItems();

    //TODO check item count is > 1
    LogI << "Play Sound Here" << std::endl;
}

void OptionsStateV2::activateRight()
{
    m_menuLayout.items[m_tabBar.activeIndex][m_menuLayout.itemIndex].activateRight();
    updateMenuItems();

    //TODO check item count is > 1
    LogI << "Play Sound Here" << std::endl;
}

void OptionsStateV2::refreshView()
{
    updateTabBar();
}

void OptionsStateV2::quitState()
{
    m_rootNode.getComponent<cro::Callback>().active = true;
    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}