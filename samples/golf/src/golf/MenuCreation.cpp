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

#include "MenuState.hpp"
#include "MenuCallbacks.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "Utility.hpp"
#include "CommandIDs.hpp"
#include "spooky2.hpp"
#include "../ErrorCheck.hpp"
#include "server/ServerPacketData.hpp"

#include <AchievementStrings.hpp>

#include <crogine/detail/GlobalConsts.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/Mouse.hpp>
#include <crogine/audio/AudioScape.hpp>

#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Text.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Camera.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>

#include <crogine/ecs/systems/UISystem.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Random.hpp>
#include <crogine/util/Wavetable.hpp>

#include <crogine/detail/OpenGL.hpp>

#include <cstring>

namespace
{
#include "RandNames.hpp"

    struct CursorAnimationCallback final
    {
        static std::vector<float> WaveTable;
        std::size_t index = 0;

        void operator()(cro::Entity e, float dt)
        {
            e.getComponent<cro::Transform>().move({ WaveTable[index] * dt, 0.f });
            index = (index + 1) % WaveTable.size();
        }

        CursorAnimationCallback()
        {
            if (WaveTable.empty())
            {
                WaveTable = cro::Util::Wavetable::sine(3.f, 40.f);
            }
        }
    };
    std::vector<float> CursorAnimationCallback::WaveTable;

    const std::array<std::string, ScoreType::Count> RuleDescriptions =
    {
        "Player with fewest overall strokes wins",
        "Player wins a hole with fewest strokes,\nPlayer with most holes wins",
        "Player wins a hole with fewest strokes,\nskins pot rolls over if hole\nis tied. Player with most skins wins"
    };
}

constexpr std::array<glm::vec2, MenuState::MenuID::Count> MenuState::m_menuPositions =
{
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, 0.f),
    glm::vec2(0.f, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, MenuSpacing.y),
    glm::vec2(-MenuSpacing.x, 0.f),
    glm::vec2(0.f, 0.f)
};

void MenuState::parseCourseDirectory()
{
    static const std::string rootDir("assets/golf/courses");
    auto directories = cro::FileSystem::listDirectories(cro::FileSystem::getResourcePath() + rootDir);

    //at least be consistent across platforms
    std::sort(directories.begin(), directories.end(), [](const  std::string& a, const std::string& b) {return a < b; });

    for (const auto& dir : directories)
    {
        if (dir == "tutorial")
        {
            continue;
        }

        auto courseFile = rootDir + "/" + dir + "/course.data";
        if (cro::FileSystem::fileExists(cro::FileSystem::getResourcePath() + courseFile))
        {
            std::string title;
            std::string description;
            std::int32_t holeCount = 0;

            cro::ConfigFile cfg;
            cfg.loadFromFile(courseFile);

            const auto& props = cfg.getProperties();
            for (const auto& prop : props)
            {
                const auto& propName = prop.getName();
                if (propName == "description")
                {
                    description = prop.getValue<std::string>();
                }
                else if (propName == "hole")
                {
                    holeCount++;
                }
                else if (propName == "title")
                {
                    title = prop.getValue<std::string>();
                }
                //TODO we could validate the hole files exist
                //but that's done when loading the game anyway
                //here we just want some lobby info and an
                //early indication if a client is missing the
                //data the host has
            }

            if (holeCount > 0)
            {
                auto& data = m_courseData.emplace_back();
                if (!title.empty())data.title = title;
                if(!description.empty()) data.description = description;
                data.directory = dir;
                data.holeCount = "Holes: " + std::to_string(std::min(holeCount, 18));
            }
        }
    }

    if (!m_courseData.empty())
    {
        m_sharedData.courseIndex = std::min(m_sharedData.courseIndex, m_courseData.size() - 1);
    }
}

void MenuState::createToolTip()
{
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font);// .setString("buns");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(cro::Colour::Black);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<cro::Callback>().function = 
        [&](cro::Entity e, float)
    {
        auto position = m_uiScene.getActiveCamera().getComponent<cro::Camera>().pixelToCoords(cro::Mouse::getPosition());
        position.x = std::floor(position.x);
        position.y = std::floor(position.y);
        position.z = 2.f;

        static constexpr glm::vec3 Offset(8.f, -4.f, 0.f);
        position += (Offset * m_viewScale.x);

        e.getComponent<cro::Transform>().setPosition(position);
        e.getComponent<cro::Transform>().setScale(m_viewScale);
    };
    m_toolTip = entity;
}

void MenuState::showToolTip(const std::string& label)
{
    if (label != m_toolTip.getComponent<cro::Text>().getString())
    {
        m_toolTip.getComponent<cro::Text>().setString(label);
    }

    m_toolTip.getComponent<cro::Callback>().active = true;
}

void MenuState::hideToolTip()
{
    m_toolTip.getComponent<cro::Callback>().active = false;
    m_toolTip.getComponent<cro::Transform>().setPosition(glm::vec3(10000.f));
}

void MenuState::createUI()
{
    parseCourseDirectory();
    parseAvatarDirectory();
    createToolTip();

    auto mouseEnterCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
        });
    auto mouseExitCallback = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
        });


    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({0.f, 0.f, -0.5f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_backgroundTexture.getTexture());
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin(glm::vec2(bounds.width / 2.f, bounds.height / 2.f));
    entity.addComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //this is activated once to make sure the
        //sprite is up to date with any texture buffer resize
        glm::vec2 texSize = e.getComponent<cro::Sprite>().getTexture()->getSize();
        e.getComponent<cro::Sprite>().setTextureRect({ glm::vec2(0.f), texSize });
        e.getComponent<cro::Transform>().setOrigin(texSize / 2.f);
        e.getComponent<cro::Callback>().active = false;
    };
    auto courseEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::RootNode;
    auto rootNode = entity;

    //consumes input during menu animation.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::UIInput>().setGroup(MenuID::Dummy);

    createPlayerConfigMenu();
    createMainMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createAvatarMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createJoinMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createLobbyMenu(rootNode, mouseEnterCallback, mouseExitCallback);

    //diplays version number
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 2.f, 10.f, 1.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Version: " + StringVer);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);

    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        auto vpSize = calcVPSize();

        m_viewScale = glm::vec2(std::floor(size.y / vpSize.y));
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(m_menuPositions[m_currentMenu] * m_viewScale);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));

        //updates any text objects / buttons with a relative position
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::UIElement;
        cmd.action =
            [&,size](cro::Entity e, float)
        {
            const auto& element = e.getComponent<UIElement>();
            auto pos = element.absolutePosition;
            pos += element.relativePosition * size / m_viewScale;

            pos.x = std::floor(pos.x);
            pos.y = std::floor(pos.y);

            e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

        //and resizes banners horizontally
        cmd.targetFlags = CommandID::Menu::UIBanner;
        cmd.action =
            [](cro::Entity e, float)
        {
            //e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>().second = 1;
            e.getComponent<cro::Callback>().active = true;
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Camera>().resizeCallback = updateView;
    m_uiScene.setActiveCamera(entity);
    updateView(entity.getComponent<cro::Camera>());
}

void MenuState::createMainMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>();
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().direction = MenuData::Out;
    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().currentTime = 1.f;
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Main] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/main_menu.spt", m_resources.textures);

    //just store these here to save reloading the same sprite sheet
    m_sprites[SpriteID::ButtonBanner] = spriteSheet.getSprite("banner_small");
    m_sprites[SpriteID::Cursor] = spriteSheet.getSprite("cursor");
    m_sprites[SpriteID::Flag] = spriteSheet.getSprite("flag");
    m_sprites[SpriteID::AddPlayer] = spriteSheet.getSprite("add_player");
    m_sprites[SpriteID::RemovePlayer] = spriteSheet.getSprite("remove_player");
    m_sprites[SpriteID::PrevMenu] = spriteSheet.getSprite("exit");
    m_sprites[SpriteID::NextMenu] = spriteSheet.getSprite("continue");
    m_sprites[SpriteID::ReadyUp] = spriteSheet.getSprite("ready_up");
    m_sprites[SpriteID::StartGame] = spriteSheet.getSprite("start_game");
    m_sprites[SpriteID::Connect] = spriteSheet.getSprite("connect");


    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, 0.f });
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.5f, 0.72f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;

    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto titleEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 1.f, bounds.height });
    entity.getComponent<cro::Transform>().move({ 57.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("flag");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    titleEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), bounds.height, 0.1f });
    entity.addComponent<cro::Drawable2D>().setCroppingArea({0.f, 0.f, 0.f, 0.f});
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("super");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, std::floor(bounds.height * 0.7f) });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [bounds, titleEnt](cro::Entity e, float dt)
    {
        if (!titleEnt.getComponent<cro::Callback>().active)
        {
            auto crop = e.getComponent<cro::Drawable2D>().getCroppingArea();
            crop.height = bounds.height;
            crop.width = std::min(bounds.width, crop.width + (bounds.width * dt));
            e.getComponent<cro::Drawable2D>().setCroppingArea(crop);

            if (crop.width == bounds.width)
            {
                e.getComponent<cro::Callback>().active = false;
            }
        }
    };
    titleEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //menu text background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 26.f, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("banner");
    auto textureRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::pair<float, std::int32_t>>(0.f, 0);
    entity.getComponent<cro::Callback>().function =
        [&, textureRect](cro::Entity e, float dt)
    {
        auto& [currTime, state] = e.getComponent<cro::Callback>().getUserData<std::pair<float, std::int32_t>>();
        if (state == 0)
        {
            //intro anim
            currTime = std::min(1.f, currTime + dt);
            float scale = cro::Util::Easing::easeOutQuint(currTime);
            e.getComponent<cro::Transform>().setScale(glm::vec2(1.f, scale));

            auto rect = textureRect;
            rect.width = currTime * (static_cast<float>(GolfGame::getActiveTarget()->getSize().x) / m_viewScale.x);
            e.getComponent<cro::Sprite>().setTextureRect(rect);

            if (currTime == 1)
            {
                state = 1;
                e.getComponent<cro::Callback>().active = false;

                //only set this if we're not already connected
                //else we'll be going straight to the lobby
                if (!m_sharedData.clientConnection.connected)
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Main);
                }
            }
        }
        else
        {
            //fit to size
            auto rect = textureRect;
            rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) / m_viewScale.x;
            e.getComponent<cro::Sprite>().setTextureRect(rect);
            e.getComponent<cro::Callback>().active = false;
        }
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto bannerEnt = entity;

    //golf cart
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 4.f, 0.01f });
    entity.getComponent<cro::Transform>().setScale({ -1.f, 1.f });
    entity.addComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cart");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::int32_t>(0);
    entity.getComponent<cro::Callback>().function =
        [bannerEnt](cro::Entity e, float dt)
    {
        auto& dir = e.getComponent<cro::Callback>().getUserData<std::int32_t>();
        switch (dir)
        {
        case 0:
        {
            auto width = e.getComponent<cro::Sprite>().getTextureRect().width;
            auto position = e.getComponent<cro::Transform>().getPosition();
            position.x = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width + width;
            e.getComponent<cro::Transform>().setPosition(position);

            if (!bannerEnt.getComponent<cro::Callback>().active)
            {
                //animation stopped
                e.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                dir = 2;
            }
        }
        break;
        case 1:

            break;
        case 2:
        {
            float width = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width;
            width *= 0.7f;

            auto position = e.getComponent<cro::Transform>().getPosition();
            float diff = width - position.x;
            e.getComponent<cro::Transform>().move({ diff * dt, 0.f });
        }
        break;
        }
    };
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto cartEnt = entity;

    //banner header
    bounds = spriteSheet.getSprite("header").getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ textureRect.width / 2.f, textureRect.height - bounds.height, -0.1f });
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("header");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [bannerEnt, textureRect](cro::Entity e, float dt)
    {
        float width = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width;
        auto position = e.getComponent<cro::Transform>().getPosition();
        position.x = width / 2.f;

        if (!bannerEnt.getComponent<cro::Callback>().active)
        {
            float diff = textureRect.height - position.y;
            position.y += diff * (dt * 2.f);
        }

        e.getComponent<cro::Transform>().setPosition(position);
    };
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //banner footer
    bounds = spriteSheet.getSprite("footer").getTextureBounds();
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ textureRect.width / 2.f, 0.f, -0.3f });
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("footer");
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [bannerEnt, bounds](cro::Entity e, float dt)
    {
        float width = bannerEnt.getComponent<cro::Sprite>().getTextureRect().width;
        auto position = e.getComponent<cro::Transform>().getPosition();
        position.x = width / 2.f;

        if (!bannerEnt.getComponent<cro::Callback>().active)
        {
            float diff = -bounds.height - position.y;
            position.y += diff * (dt * 2.f);
        }

        e.getComponent<cro::Transform>().setPosition(position);
    };
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto footerEnt = entity;

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -100.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cursor");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto cursorEnt = entity;

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + glm::vec3(-20.f, -7.f, 0.f));
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        });

    static constexpr float TextOffset = 26.f;
    static constexpr float LineSpacing = 10.f;
    glm::vec3 textPos = { TextOffset, 54.f, 0.1f };

    auto createButton = [&](const std::string& label)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(textPos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(label);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.addComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;

        bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        textPos.y -= LineSpacing;

        return entity;
    };


    if (!m_courseData.empty()
        && !m_sharedData.ballModels.empty()
        && ! m_sharedData.avatarInfo.empty())
    {
        //host
        entity = createButton("Create Game");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        m_sharedData.hosting = true;

                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                        menuEntity.getComponent<cro::Callback>().active = true;

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });


        //join
        entity = createButton("Join Game");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        m_sharedData.hosting = false;

                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                        menuEntity.getComponent<cro::Callback>().active = true;

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });

        //practice menu
        entity = createButton("Practice");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        requestStackPush(StateID::Practice);
                        //m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });
    }
    else
    {
        std::string str = "Error:\n";
        if (m_courseData.empty())
        {
            str += "No course data found\n";
        }
        if (m_sharedData.ballModels.empty())
        {
            str += "Missing Ball Data\n";
        }
        if (m_sharedData.avatarInfo.empty())
        {
            str += "Missing Avatar Data";
        }

        //display error
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(textPos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(str);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);

        bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        textPos.y -= LineSpacing * 3.f;
    }

    //options
    entity = createButton("Options");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    requestStackPush(StateID::Options);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });


    //quit
    entity = createButton("Quit");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    cro::App::quit();
                }
            });


    //if the 19th hole unlocked add a sneaky button to the footer
    if (Achievements::getAchievement(AchievementStrings[AchievementID::JoinTheClub])->achieved)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
        bounds = footerEnt.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setPosition({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f), 0.1f });

        entity.getComponent<cro::UIInput>().setGroup(MenuID::Main);

        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([cursorEnt](cro::Entity e) mutable
                {
                    e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
                    e.getComponent<cro::AudioEmitter>().play();

                    cursorEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                });
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([](cro::Entity e)
                {
                    e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
                });

        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&, cartEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt))
                    {
                        cartEnt.getComponent<cro::Callback>().function = 
                            [&](cro::Entity e, float dt)
                        {
                            e.getComponent<cro::Transform>().move({ -400.f * dt, 0.f, 0.f });
                            if (e.getComponent<cro::Transform>().getPosition().x < -100.f)
                            {
                                //also used as table index so reset this in case
                                //the current value is greater than the number of tables...
                                m_sharedData.courseIndex = 0;

                                requestStackClear();
                                requestStackPush(StateID::Clubhouse);
                            }
                        };

                        Achievements::awardAchievement(AchievementStrings[AchievementID::Socialiser]);
                    }
                });

        footerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
}

void MenuState::createAvatarMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Avatar] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());
    
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/bounce02.spt", m_resources.textures);

    //ball drop
    auto ballEnt = m_uiScene.createEntity();
    ballEnt.addComponent<cro::Transform>().setPosition({ 340.f, -6.f, 0.1f });
    ballEnt.addComponent<cro::Drawable2D>();
    ballEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("bounce");
    ballEnt.addComponent<cro::SpriteAnimation>().play(0);
    ballEnt.addComponent<cro::Callback>().active = true;
    ballEnt.getComponent<cro::Callback>().setUserData<float>(static_cast<float>(cro::Util::Random::value(18, 50)));
    ballEnt.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        if (currTime < 0)
        {
            e.getComponent<cro::SpriteAnimation>().play(0);
            currTime = static_cast<float>(cro::Util::Random::value(18, 50));
        }
    };

    //background
    spriteSheet.loadFromFile("assets/golf/sprites/player_menu.spt", m_resources.textures);

    m_sprites[SpriteID::ArrowLeft] = spriteSheet.getSprite("arrow_l");
    m_sprites[SpriteID::ArrowLeftHighlight] = spriteSheet.getSprite("arrow_l_h");
    m_sprites[SpriteID::ArrowRight] = spriteSheet.getSprite("arrow_r");
    m_sprites[SpriteID::ArrowRightHighlight] = spriteSheet.getSprite("arrow_r_h");
    m_sprites[SpriteID::Controller] = spriteSheet.getSprite("controller");
    m_sprites[SpriteID::Keyboard] = spriteSheet.getSprite("keyboard");
    m_sprites[SpriteID::ThumbBackground] = spriteSheet.getSprite("thumb_bg");
    m_sprites[SpriteID::CPUHighlight] = spriteSheet.getSprite("check_highlight");

    //this entity has the player edit text ents added to it by updateLocalAvatars
    auto avatarEnt = m_uiScene.createEntity();
    avatarEnt.addComponent<cro::Transform>();
    avatarEnt.addComponent<cro::Drawable2D>();
    avatarEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    avatarEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    avatarEnt.getComponent<UIElement>().depth = -0.2f;
    avatarEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    auto bounds = avatarEnt.getComponent<cro::Sprite>().getTextureBounds();
    avatarEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    avatarEnt.getComponent<cro::Transform>().addChild(ballEnt.getComponent<cro::Transform>());
    menuEntity.getComponent<cro::Transform>().addChild(avatarEnt.getComponent<cro::Transform>());
    m_avatarMenu = avatarEnt;

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Avatar]);


    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //banner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ButtonBanner];
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&,spriteRect](cro::Entity e, float)
    {
        auto rect = spriteRect;
        rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
        e.getComponent<cro::Sprite>().setTextureRect(rect);
        e.getComponent<cro::Callback>().active = false;
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -10000.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    //entity.addComponent<cro::Callback>().active = true;
    //entity.getComponent<cro::Callback>().setUserData<std::pair<cro::Entity,glm::vec3>>(cro::Entity(), 0.f);
    //entity.getComponent<cro::Callback>().function =
    //    [avatarEnt](cro::Entity e, float)
    //{
    //    //makes sure to move with current parent on screen resize
    //    const auto& [parent, offset] = e.getComponent<cro::Callback>().getUserData<std::pair<cro::Entity, glm::vec3>>();
    //    if (parent.isValid())
    //    {
    //        auto pos = offset;
    //        if (parent == avatarEnt)
    //        {
    //            pos += avatarEnt.getComponent<cro::Transform>().getPosition();
    //            pos -= avatarEnt.getComponent<cro::Transform>().getOrigin();
    //        }

    //        e.getComponent<cro::Transform>().setPosition(parent.getComponent<cro::Transform>().getPosition() + pos);
    //    }
    //};
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto cursorEnt = entity;

    //this callback is overridden for the avatar previews
    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity, avatarEnt](cro::Entity e) mutable
        {
            auto basePos = avatarEnt.getComponent<cro::Transform>().getPosition();
            basePos -= avatarEnt.getComponent<cro::Transform>().getOrigin();

            static constexpr glm::vec3 Offset(52.f, -7.f, 0.f);
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + basePos + Offset);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(-1.f, 1.f));
            entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            //entity.getComponent<cro::Callback>().setUserData<std::pair<cro::Entity, glm::vec3>>(e, Offset);
        });


    //and this one is used for regular buttons.
    auto mouseEnterCursor = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            //entity.getComponent<cro::Callback>().setUserData<std::pair<cro::Entity, glm::vec3>>(e, CursorOffset);
        });

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 20.f, MenuBottomBorder });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::PrevMenu].getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
                    menuEntity.getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto showTip = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            showToolTip(e.getLabel());
        });

    auto hideTip = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            hideToolTip();
        });

    //add player button
    entity = m_uiScene.createEntity();
    entity.setLabel("Add Player");
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.3334f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::AddPlayer];
    bounds = m_sprites[SpriteID::AddPlayer].getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = hideTip;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Enter] = showTip;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    if (m_sharedData.localConnectionData.playerCount < m_sharedData.localConnectionData.MaxPlayers)
                    {
                        auto index = m_sharedData.localConnectionData.playerCount;
                        
                        if (m_sharedData.localConnectionData.playerData[index].name.empty())
                        {
                            m_sharedData.localConnectionData.playerData[index].name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
                        }
                        m_sharedData.localConnectionData.playerCount++;

                        updateLocalAvatars(mouseEnter, mouseExit);

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //remove player button
    entity = m_uiScene.createEntity();
    entity.setLabel("Remove Player");
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.6667f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::RemovePlayer];
    bounds = m_sprites[SpriteID::RemovePlayer].getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    //entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = hideTip;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Enter] = showTip;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, mouseEnter, mouseExit](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    
                    if (m_sharedData.localConnectionData.playerCount > 1)
                    {
                        m_sharedData.localConnectionData.playerCount--;
                        updateLocalAvatars(mouseEnter, mouseExit);

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    m_courseSelectCallbacks.prevRules = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.scoreType = (m_sharedData.scoreType + (ScoreType::Count - 1)) % ScoreType::Count;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ScoreType, m_sharedData.scoreType, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_courseSelectCallbacks.nextRules = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.scoreType = (m_sharedData.scoreType + 1) % ScoreType::Count;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ScoreType, m_sharedData.scoreType, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });
    
    m_courseSelectCallbacks.prevRadius = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.gimmeRadius = (m_sharedData.gimmeRadius + (ScoreType::Count - 1)) % ScoreType::Count;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::GimmeRadius, m_sharedData.gimmeRadius, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_courseSelectCallbacks.nextRadius = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.gimmeRadius = (m_sharedData.gimmeRadius + 1) % ScoreType::Count;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::GimmeRadius, m_sharedData.gimmeRadius, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_courseSelectCallbacks.prevCourse = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.courseIndex = (m_sharedData.courseIndex + (m_courseData.size() - 1)) % m_courseData.size();

                m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
                auto data = serialiseString(m_sharedData.mapDirectory);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_courseSelectCallbacks.nextCourse = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.courseIndex = (m_sharedData.courseIndex + 1) % m_courseData.size();

                m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
                auto data = serialiseString(m_sharedData.mapDirectory);
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_courseSelectCallbacks.selected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::SpriteAnimation>().play(1);
            e.getComponent<cro::AudioEmitter>().play();
        });
    m_courseSelectCallbacks.unselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::SpriteAnimation>().play(0);
        });

    m_courseSelectCallbacks.showTip = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            showToolTip(RuleDescriptions[m_sharedData.scoreType]);
        });
    m_courseSelectCallbacks.hideTip = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            hideToolTip();
        });

    //continue
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 0.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextMenu];
    bounds = m_sprites[SpriteID::NextMenu].getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    saveAvatars(m_sharedData);
                    
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    if (m_sharedData.hosting)
                    {
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.launch(ConstVal::MaxClients, Server::GameMode::Golf);

                            //small delay for server to get ready
                            cro::Clock clock;
                            while (clock.elapsed().asMilliseconds() < 500) {}

                            m_matchMaking.createGame(Server::GameMode::Golf);
                        }
                    }
                    else
                    {
                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Join;
                        menuEntity.getComponent<cro::Callback>().active = true;
                    }
                }
            });
    entity.getComponent<UIElement>().absolutePosition.x = -bounds.width;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //we need to create the controller icon callbacks here
    //so we can assign/reassign them dynamically if needs be
    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();
    m_controllerCallbackIDs[ControllerCallbackID::EnterLeft] =
        uiSystem.addCallback([&, cursorEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowLeftHighlight];
                e.getComponent<cro::AudioEmitter>().play();

                //hide the cursor
                cursorEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            });
    m_controllerCallbackIDs[ControllerCallbackID::ExitLeft] =
        uiSystem.addCallback([&](cro::Entity e)
            {
                e.getComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowLeft];
            });
    m_controllerCallbackIDs[ControllerCallbackID::EnterRight] =
        uiSystem.addCallback([&, cursorEnt](cro::Entity e) mutable
            {
                e.getComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowRightHighlight];
                e.getComponent<cro::AudioEmitter>().play();

                //hide the cursor
                cursorEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
            });
    m_controllerCallbackIDs[ControllerCallbackID::ExitRight] =
        uiSystem.addCallback([&](cro::Entity e)
            {
                e.getComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowRight];
            });


    for (auto i = 0u; i < 4u; ++i)
    {
        m_controllerCallbackIDs[ControllerCallbackID::Dec01 + i] =
            uiSystem.addCallback([&, i](cro::Entity e, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        auto controllerCount = static_cast<std::int32_t>(cro::GameController::getControllerCount());
                        if (controllerCount > 0)
                        {
                            m_sharedData.controllerIDs[i] = (m_sharedData.controllerIDs[i] + (controllerCount - 1)) % controllerCount;
                        }

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });

        m_controllerCallbackIDs[ControllerCallbackID::Inc01 + i] =
            uiSystem.addCallback([&, i](cro::Entity e, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        auto controllerCount = static_cast<std::int32_t>(cro::GameController::getControllerCount());
                        if (controllerCount > 0)
                        {
                            m_sharedData.controllerIDs[i] = (m_sharedData.controllerIDs[i] + 1) % controllerCount;
                        }

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                });

        m_avatarEditCallbacks[i] =
            uiSystem.addCallback([&, i](cro::Entity e, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        showPlayerConfig(true, i);
                    }
                });
    }

    //we have to store these so the avatars can be updated from elsewhere
    //such as the player config menu when it is closed
    m_avatarCallbacks = { mouseEnter, mouseExit };


    //and these are used when creating the avatar window for CPU toggle
    m_cpuOptionCallbacks[0] = uiSystem.addCallback([&, cursorEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();

            //hide the cursor
            cursorEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    m_cpuOptionCallbacks[1] = uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    m_cpuOptionCallbacks[2] = uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto index = e.getComponent<cro::Callback>().getUserData<const std::uint32_t>();
                m_sharedData.localConnectionData.playerData[index].isCPU =
                    !m_sharedData.localConnectionData.playerData[index].isCPU;

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_cpuOptionCallbacks[3] = uiSystem.addCallback([&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            showToolTip("Make this a computer controlled player");
        });
    m_cpuOptionCallbacks[4] = uiSystem.addCallback([&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            hideToolTip();
        });

    updateLocalAvatars(mouseEnter, mouseExit);
}

void MenuState::createJoinMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Join] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Join]);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/connect_menu.spt", m_resources.textures);

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //ip text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setCroppingArea({ 0.f, 0.f, 200.f, -16.f });
    entity.addComponent<cro::Text>(font).setString(m_sharedData.targetIP);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        //add a cursor to the end of the string when active
        cro::String str = m_sharedData.targetIP;
        if (str.size() < ConstVal::MaxIPChars)
        {
            str += "_";
        }
        e.getComponent<cro::Text>().setString(str);
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    auto highlight = m_uiScene.createEntity();
    highlight.addComponent<cro::Transform>().setPosition({ 11.f, 16.f, 0.1f });
    highlight.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    highlight.addComponent<cro::Drawable2D>();
    highlight.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight");

    auto balls = m_uiScene.createEntity();
    balls.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.1f });
    balls.addComponent<cro::Drawable2D>();
    balls.addComponent<cro::Sprite>() = spriteSheet.getSprite("bounce");
    balls.addComponent<cro::SpriteAnimation>().play(0);
    bounds = balls.getComponent<cro::Sprite>().getTextureBounds();
    balls.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), 0.f });


    //box background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().depth = -0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([highlight](cro::Entity) mutable { highlight.getComponent<cro::Sprite>().setColour(cro::Colour::White); highlight.getComponent<cro::AudioEmitter>().play(); });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([highlight](cro::Entity) mutable { highlight.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        beginTextEdit(textEnt, &m_sharedData.targetIP, ConstVal::MaxIPChars);
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                        if (evt.type == SDL_CONTROLLERBUTTONUP)
                        {
                            requestStackPush(StateID::Keyboard);
                        }
                    }
                    else
                    {
                        applyTextEdit();
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    textEnt.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
    textEnt.getComponent<cro::Transform>().move({ -60.f, -12.f, 0.1f });
    balls.getComponent<cro::Transform>().setPosition(entity.getComponent<cro::Transform>().getOrigin());
    balls.getComponent<cro::Transform>().move({ 0.f, 0.f, 0.1f });
    entity.getComponent<cro::Transform>().addChild(textEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(highlight.getComponent<cro::Transform>());
    entity.getComponent<cro::Transform>().addChild(balls.getComponent<cro::Transform>());
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //banner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ButtonBanner];
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, spriteRect](cro::Entity e, float)
    {
        auto rect = spriteRect;
        rect.width = static_cast<float>(GolfGame::getActiveTarget()->getSize().x) * m_viewScale.x;
        e.getComponent<cro::Sprite>().setTextureRect(rect);
        e.getComponent<cro::Callback>().active = false;
    };
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //join
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -40.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Connect];
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit(); //finish any pending changes

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    if (!m_sharedData.targetIP.empty() &&
                        !m_sharedData.clientConnection.connected)
                    {
                        m_matchMaking.joinGame(0);
                    }
                }
            });
    
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createLobbyMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Lobby] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Lobby]);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_resources.textures);
    m_sprites[SpriteID::PrevCourse] = spriteSheet.getSprite("arrow_left");
    m_sprites[SpriteID::NextCourse] = spriteSheet.getSprite("arrow_right");


    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    entity.getComponent<UIElement>().depth = -0.2f;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto bgEnt = entity;

    //display the score type
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({bounds.width / 2.f, 128.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(ScoreTypes[m_sharedData.scoreType]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ScoreType;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //and score description
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 74.f, 112.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(ScoreDesc[m_sharedData.scoreType]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ScoreDesc;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //gimme radius
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 52.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString("Gimme Radius");
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), 36.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::GimmeDesc;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //display lobby members - updateLobbyAvatars() adds the text ents to this.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, bounds.height - 25.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::LobbyList;
    entity.addComponent<cro::Callback>().setUserData<std::vector<cro::Entity>>(); //abuse this component to store handles to the text children.
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //background for course info
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("course_desc");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().relativePosition = CourseDescPosition;
    entity.getComponent<UIElement>().depth = -0.2f;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor( bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    bgEnt = entity;

    //flag
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f), bounds.height });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Flag];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //bouncy ball
    cro::SpriteSheet sheet;
    sheet.loadFromFile("assets/golf/sprites/bounce.spt", m_resources.textures);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 14.f, bounds.height - 4.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = sheet.getSprite("bounce");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float dt)
    {
        auto& currTime = e.getComponent<cro::Callback>().getUserData<float>();
        currTime -= dt;
        if (currTime < 0)
        {
            e.getComponent<cro::SpriteAnimation>().play(0);
            currTime = static_cast<float>(cro::Util::Random::value(30, 60));
        }
    };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    

    //course title
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 40.f, 01.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseTitle;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //course description
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 28.f, 01.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseDesc;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hole count
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 16.f, 01.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseHoles;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //banner
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, BannerPosition, -0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ButtonBanner];
    auto spriteRect = entity.getComponent<cro::Sprite>().getTextureRect();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIBanner;
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
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -10000.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto enter = mouseEnter;
    auto exit = mouseExit;

    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });


    //quit confirmation
    spriteSheet.loadFromFile("assets/golf/sprites/ui.spt", m_resources.textures);

    struct ConfirmationData final
    {
        float progress = 0.f;
        enum
        {
            In, Out
        }dir = In;
        bool quitWhenDone = false;
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("message_board");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
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
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::ConfirmQuit);
            }
        }
        else
        {
            data.progress = std::max(0.f, data.progress - (dt * 4.f));
            scale = cro::Util::Easing::easeOutQuint(data.progress);
            if (data.progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                if (data.quitWhenDone)
                {
                    quitLobby();
                }
                else
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Lobby);
                }
            }
        }

        
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;

    //quad to darken the screen
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, -0.4f });
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

    //confirmation text
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 56.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("Are You Sure?");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 44.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("This will end the game.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 20.f, 26.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Text>(font).setString("Yes");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::UIInput>().setGroup(MenuID::ConfirmQuit);
    entity.getComponent<cro::UIInput>().area = cro::Text::getLocalBounds(entity);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = enter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = exit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, confirmEnt, shadeEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = true;
                    confirmEnt.getComponent<cro::Callback>().active = true;
                    shadeEnt.getComponent<cro::Callback>().active = true;
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 20.f, 26.f, 0.1f });
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
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, confirmEnt, shadeEnt](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
                    confirmEnt.getComponent<cro::Callback>().active = true;
                    shadeEnt.getComponent<cro::Callback>().active = true;
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //back
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, confirmEnt, shadeEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
                    confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
                    confirmEnt.getComponent<cro::Callback>().active = true;
                    shadeEnt.getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //start
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -16.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ReadyButton;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ReadyUp]; //which sprite is set by sending a message to this ent when we know if we're hosting or joining
    entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::ReadyUp].getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hosting)
                    {
                        //check all members ready
                        bool ready = true;
                        for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                        {
                            if (m_sharedData.connectionData[i].playerCount != 0
                                && !m_readyState[i])
                            {
                                ready = false;
                                break;
                            }
                        }

                        if (ready && m_sharedData.clientConnection.connected
                            && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                        {
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                        }
                    }
                    else
                    {
                        //toggle readyness
                        std::uint8_t ready = m_readyState[m_sharedData.clientConnection.connectionID] ? 0 : 1;
                        m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady, std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | ready),
                            net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                        if (ready)
                        {
                            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                        }
                        else
                        {
                            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                        }
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //server info message
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { 10.f, 0.f };
    entity.getComponent<UIElement>().relativePosition = { 0.f, 0.98f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ServerInfo;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Connected to");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setShadowColour(cro::Colour(std::uint8_t(110), 179, 157));
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //network icon
    spriteSheet.loadFromFile("assets/golf/sprites/scoreboard.spt", m_resources.textures);
    m_sprites[SpriteID::NetStrength] = spriteSheet.getSprite("strength_meter");


    //running scores
    struct ScoreInfo final
    {
        std::uint8_t clientID = 0;
        std::uint8_t playerID = 0;
        std::uint8_t score = 0;
    };

    std::vector<ScoreInfo> scoreInfo;

    for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
    {
        for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
        {
            auto& info = scoreInfo.emplace_back();
            info.clientID = i;
            info.playerID = j;
            info.score = m_sharedData.connectionData[i].playerData[j].score;
        }
    }

    std::sort(scoreInfo.begin(), scoreInfo.end(), [](const ScoreInfo& a, const ScoreInfo& b) {return a.score < b.score; });

    std::vector<cro::String> names;
    for (const auto& score : scoreInfo)
    {
        if (score.score != 0)
        {
            names.push_back(m_sharedData.connectionData[score.clientID].playerData[score.playerID].name);
        }
    }

    cro::String str;
    if (!names.empty())
    {
        str = "Last Round's Top Scorers: " + names[0];
        for (auto i = 1u; i < names.size() && i < 4u; ++i)
        {
            str += " - " + names[i];
        }
    }
    else
    {
        str = "Welcome To Video Golf!";
    }

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString(str);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (m_currentMenu == MenuID::Lobby)
        {
            auto pos = e.getComponent<cro::Transform>().getPosition();

            pos.x -= 20.f * dt;
            pos.y = 15.f;
            pos.z = 0.3f;

            static constexpr float Offset = 50.f;
            if (pos.x < -bounds.width + Offset)
            {
                pos.x = cro::App::getWindow().getSize().x / m_viewScale.x;
                pos.x -= Offset;
            }

            e.getComponent<cro::Transform>().setPosition(pos);

                
            cro::FloatRect cropping = { -pos.x + Offset, -16.f, (cro::App::getWindow().getSize().x / m_viewScale.x) - (Offset * 2.f), 18.f };
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        }
    };

    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createPlayerConfigMenu()
{
    cro::Colour c(0.f, 0.f, 0.f, BackgroundAlpha);
    auto fadeNode = m_uiScene.createEntity();
    fadeNode.addComponent<cro::Transform>(); //relative to bgNode!
    fadeNode.addComponent<cro::Drawable2D>().getVertexData() =
    {
        cro::Vertex2D(glm::vec2(-0.5f, 0.5f), c),
        cro::Vertex2D(glm::vec2(-0.5f), c),
        cro::Vertex2D(glm::vec2(0.5f), c),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), c)
    };
    fadeNode.getComponent<cro::Drawable2D>().updateLocalBounds();


    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player_selection.spt", m_resources.textures);

    auto bgNode = m_uiScene.createEntity();
    bgNode.addComponent<cro::Transform>();
    bgNode.addComponent<cro::Drawable2D>();
    bgNode.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerConfig;
    bgNode.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    auto bounds = bgNode.getComponent<cro::Sprite>().getTextureBounds();
    bgNode.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    bgNode.addComponent<cro::Callback>().active = true;
    bgNode.getComponent<cro::Callback>().setUserData<std::pair<float, float>>(0.f, 0.f);
    bgNode.getComponent<cro::Callback>().function =
        [&, fadeNode](cro::Entity e, float dt) mutable
    {
        auto& [current, target] = e.getComponent<cro::Callback>().getUserData<std::pair<float, float >>();
        float scale = current;

        if (current < target)
        {
            current = std::min(current + dt * 2.f, target);
            scale = cro::Util::Easing::easeOutBounce(current);
        }
        else if (current > target)
        {
            current = std::max(current - dt * 3.f, target);
            scale = cro::Util::Easing::easeOutQuint(current);
        }

        auto size = glm::vec2(GolfGame::getActiveTarget()->getSize());
        e.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, 1.f));
        e.getComponent<cro::Transform>().setScale(m_viewScale * scale);

        fadeNode.getComponent<cro::Transform>().setScale(size * current);
        float alpha = cro::Util::Easing::easeInQuint(current) * BackgroundAlpha;
        auto& verts = fadeNode.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour.setAlpha(alpha);
        }
    };

    fadeNode.getComponent<cro::Transform>().setPosition(bgNode.getComponent<cro::Transform>().getOrigin());
    fadeNode.getComponent<cro::Transform>().move({ 0.f, 0.f, -0.4f });
    bgNode.getComponent<cro::Transform>().addChild(fadeNode.getComponent<cro::Transform>());

    auto selected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
        });
    auto unselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    static constexpr float ButtonDepth = 0.1f;
    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //player name text
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 97.f, 171.f, ButtonDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::PlayerName;

    entity.addComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_textEdit.string != nullptr)
        {
            auto str = *m_textEdit.string;
            if (str.size() < ConstVal::MaxNameChars)
            {
                str += "_";
            }
            e.getComponent<cro::Text>().setString(str);
        }
    };

    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto textEnt = entity;

    auto createButton = [&](glm::vec2 pos, const std::string& sprite)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(pos, ButtonDepth));
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite(sprite);
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::PlayerConfig);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();

        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });

        //bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        return entity;
    };

    //I need to finish the layout editor :3
    entity = createButton({ 92.f, 159.f }, "name_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    auto& callback = textEnt.getComponent<cro::Callback>();
                    callback.active = !callback.active;
                    if (callback.active)
                    {
                        beginTextEdit(textEnt, &m_sharedData.localConnectionData.playerData[callback.getUserData<std::uint8_t>()].name, ConstVal::MaxNameChars);
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                        if (evt.type == SDL_CONTROLLERBUTTONUP)
                        {
                            requestStackPush(StateID::Keyboard);
                        }
                    }
                    else
                    {
                        applyTextEdit();
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //callbacks for colour swap arrows
    auto arrowLeftCallback = [&](const cro::ButtonEvent& evt, pc::ColourKey::Index idx)
    {
        if (activated(evt))
        {
            applyTextEdit();
            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

            auto paletteIdx = m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].avatarFlags[idx];
            paletteIdx = (paletteIdx + (pc::ColourID::Count - 1)) % pc::ColourID::Count;
            m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].avatarFlags[idx] = paletteIdx;

            applyAvatarColours(m_activePlayerAvatar);
        }
    };

    auto arrowRightCallback = [&](const cro::ButtonEvent& evt, pc::ColourKey::Index idx)
    {
        if (activated(evt))
        {
            applyTextEdit();
            m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

            auto paletteIdx = m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].avatarFlags[idx];
            paletteIdx = (paletteIdx + 1) % pc::ColourID::Count;
            m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].avatarFlags[idx] = paletteIdx;

            applyAvatarColours(m_activePlayerAvatar);
        }
    };

    //c1 left
    entity = createButton({ 24.f, 127.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Hair);

                //update the hair model too, if it exists
                auto hairID = m_hairIndices[m_activePlayerAvatar];
                if (m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[hairID].model.isValid())
                {
                    auto colour = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].getColour(pc::ColourKey::Hair).first;
                    m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[hairID].model.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", colour);

                    colour = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].getColour(pc::ColourKey::Hair).second;
                    //m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[hairID].model.getComponent<cro::Model>().setMaterialProperty(0, "u_darkColour", colour);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c1 right
    entity = createButton({ 57.f, 127.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Hair);

                auto hairID = m_hairIndices[m_activePlayerAvatar];
                if (m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[hairID].model.isValid())
                {
                    auto colour = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].getColour(pc::ColourKey::Hair).first;
                    m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[hairID].model.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", colour);

                    colour = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].getColour(pc::ColourKey::Hair).second;
                    //m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[hairID].model.getComponent<cro::Model>().setMaterialProperty(0, "u_darkColour", colour);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c2 left
    entity = createButton({ 24.f, 102.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Skin);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c2 right
    entity = createButton({ 57.f, 102.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Skin);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c3 left
    entity = createButton({ 24.f, 77.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Top);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c3 right
    entity = createButton({ 57.f, 77.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Top);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //c4 left
    entity = createButton({ 24.f, 52.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowLeftCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowLeftCallback(evt, pc::ColourKey::Bottom);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //c4 right
    entity = createButton({ 57.f, 52.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, arrowRightCallback](cro::Entity, const cro::ButtonEvent& evt)
            {
                arrowRightCallback(evt, pc::ColourKey::Bottom);
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //colour preview
    auto position = glm::vec2(41.f, 52.f);
    for (auto i = 0u; i < 4u; ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(position, 0.1f));
        entity.addComponent<cro::Drawable2D>().getVertexData() =
        {
            cro::Vertex2D(glm::vec2(0.f, 15.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(15.f), cro::Colour::White),
            
            cro::Vertex2D(glm::vec2(15.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(0.f), cro::Colour::White),
            cro::Vertex2D(glm::vec2(15, 0.f), cro::Colour::White)
        };
        entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
        entity.getComponent<cro::Drawable2D>().updateLocalBounds();
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float)
        {
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto j = 0u; j < verts.size(); ++j)
            {
                auto idx = m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].avatarFlags[i];
                verts[j].colour = j < 3 ? pc::Palette[idx].light : pc::Palette[idx].dark;
            }
        };

        bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        position.y += 25.f;
    }


    //hair left
    entity = createButton({ 95.f, 77.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                    m_hairIndices[m_activePlayerAvatar] = 
                        (m_hairIndices[m_activePlayerAvatar] + (m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels.size() - 1)) % m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels.size();
                    auto hairID = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[m_hairIndices[m_activePlayerAvatar]].uid;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].hairID = hairID;

                    setPreviewModel(m_activePlayerAvatar);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //hair right
    entity = createButton({ 177.f, 77.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    m_hairIndices[m_activePlayerAvatar] = (m_hairIndices[m_activePlayerAvatar] + 1) % m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels.size();
                    auto hairID = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].hairModels[m_hairIndices[m_activePlayerAvatar]].uid;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].hairID = hairID;

                    setPreviewModel(m_activePlayerAvatar);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //skin left
    entity = createButton({ 95.f, 52.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                    auto flipped = m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].flipped;
                    flipped = !flipped;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].flipped = flipped;
                    
                    if (flipped)
                    {
                        m_avatarIndices[m_activePlayerAvatar] = (m_avatarIndices[m_activePlayerAvatar] + (m_playerAvatars.size() - 1)) % m_playerAvatars.size();
                    }

                    auto skinID = m_sharedData.avatarInfo[m_avatarIndices[m_activePlayerAvatar]].uid;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].skinID = skinID;


                    auto soundSize = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].previewSounds.size();
                    if (soundSize != 0)
                    {
                        auto idx = soundSize > 1 ? cro::Util::Random::value(0u, soundSize - 1) : 0;
                        m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].previewSounds[idx].getComponent<cro::AudioEmitter>().play();
                    }

                    applyAvatarColours(m_activePlayerAvatar);
                    setPreviewModel(m_activePlayerAvatar);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //skin right
    entity = createButton({ 177.f, 52.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    auto flipped = m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].flipped;
                    flipped = !flipped;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].flipped = flipped;

                    if (!flipped)
                    {
                        m_avatarIndices[m_activePlayerAvatar] = (m_avatarIndices[m_activePlayerAvatar] + 1) % m_playerAvatars.size();
                    }

                    auto skinID = m_sharedData.avatarInfo[m_avatarIndices[m_activePlayerAvatar]].uid;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].skinID = skinID;


                    auto soundSize = m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].previewSounds.size();
                    if (soundSize != 0)
                    {
                        auto idx = soundSize > 1 ? cro::Util::Random::value(0u, soundSize - 1) : 0;
                        m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].previewSounds[idx].getComponent<cro::AudioEmitter>().play();
                    }
                    applyAvatarColours(m_activePlayerAvatar);
                    setPreviewModel(m_activePlayerAvatar);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //random
    entity = createButton({ 82.f, 15.f }, "button_highlight");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, textEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    //randomise name
                    const auto& callback = textEnt.getComponent<cro::Callback>();
                    auto idx = cro::Util::Random::value(0u, RandomNames.size() - 1);
                    m_sharedData.localConnectionData.playerData[callback.getUserData<std::uint8_t>()].name = RandomNames[idx];
                    textEnt.getComponent<cro::Text>().setString(RandomNames[idx]);

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();


                    //random skin
                    m_avatarIndices[m_activePlayerAvatar] = cro::Util::Random::value(0u, m_playerAvatars.size() - 1);

                    auto skinID = m_sharedData.avatarInfo[m_avatarIndices[m_activePlayerAvatar]].uid;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].skinID = skinID;
                    bool flipped = cro::Util::Random::value(0, 1) == 0;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].flipped = flipped;

                    //random colours
                    std::int32_t prevIndex = 0;
                    for (auto i = 0; i < 4; ++i)
                    {
                        auto paletteIdx = cro::Util::Random::value(0, pc::ColourID::Count - 1);

                        //prevent getting the same colours in a row
                        if (paletteIdx == prevIndex)
                        {
                            paletteIdx = (paletteIdx + 1) % pc::ColourID::Count;
                        }

                        m_playerAvatars[m_avatarIndices[m_activePlayerAvatar]].setColour(pc::ColourKey::Index(i), paletteIdx);
                        m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].avatarFlags[i] = paletteIdx;

                        prevIndex = paletteIdx;
                    }
                    applyAvatarColours(m_activePlayerAvatar);

                    //random hair
                    m_hairIndices[m_activePlayerAvatar] = cro::Util::Random::value(0u, m_playerAvatars[m_activePlayerAvatar].hairModels.size() - 1);
                    auto hairID = m_sharedData.hairInfo[m_hairIndices[m_activePlayerAvatar]].uid;
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].hairID = hairID;


                    //update texture
                    setPreviewModel(m_activePlayerAvatar);

                    //random ball
                    idx = cro::Util::Random::value(0u, m_sharedData.ballModels.size() - 1);
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].ballID = m_sharedData.ballModels[idx].uid;

                    m_ballIndices[m_activePlayerAvatar] = idx;
                    m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = static_cast<std::int32_t>(idx);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //done
    entity = createButton({ 147.f, 15.f }, "button_highlight");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    showPlayerConfig(false, m_activePlayerAvatar);
                    updateLocalAvatars(m_avatarCallbacks.first, m_avatarCallbacks.second);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //ball preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 206.f, 70.f, ButtonDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_ballTexture.getTexture());
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //hmm we only want to this in a resize callback really...
        auto size = glm::vec2(e.getComponent<cro::Sprite>().getTexture()->getSize());
        e.getComponent<cro::Sprite>().setTextureRect({ 0.f, 0.f, size.x, size.y });

        float scale = static_cast<float>(BallPreviewSize) / size.y;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //ball select left
    entity = createButton({ 213.f, 52.f }, "arrow_left");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                    auto idx = m_ballIndices[m_activePlayerAvatar];
                    idx = (idx + (m_sharedData.ballModels.size() - 1)) % m_sharedData.ballModels.size();
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].ballID = m_sharedData.ballModels[idx].uid;
                    m_ballIndices[m_activePlayerAvatar] = idx;

                    m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = static_cast<std::int32_t>(idx);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //ball select right
    entity = createButton({ 246.f, 52.f }, "arrow_right");
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                    auto idx = m_ballIndices[m_activePlayerAvatar];
                    idx = (idx + 1) % m_sharedData.ballModels.size();
                    m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].ballID = m_sharedData.ballModels[idx].uid;
                    m_ballIndices[m_activePlayerAvatar] = idx;

                    m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = static_cast<std::int32_t>(idx);
                }
            });
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //player preview
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 109.f, 54.f, ButtonDepth });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_avatarTexture.getTexture());
    bgNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [](cro::Entity e, float)
    {
        //hmm we only want to this in a resize callback really...
        auto size = glm::vec2(e.getComponent<cro::Sprite>().getTexture()->getSize());
        e.getComponent<cro::Sprite>().setTextureRect({ 0.f, 0.f, size.x, size.y });

        float scale = static_cast<float>(AvatarPreviewSize.y) / size.y;
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
}

void MenuState::updateLocalAvatars(std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    //these can have fixed positions as they are attached to a menuEntity[] which is UI scaled
    static constexpr glm::vec3 EditButtonOffset(-47.f, -57.f, 0.f);
    static constexpr glm::vec3 CPUTextOffset(28.f, -57.f, 0.f);
    static constexpr glm::vec3 CPUButtonOffset(16.f, -65.f, 0.f);
    static constexpr glm::vec3 CPUHighlightOffset(18.f, -63.f, 0.f);
    static constexpr glm::vec3 AvatarOffset = EditButtonOffset + glm::vec3(-68.f, -18.f, 0.f);
    static constexpr glm::vec3 BGOffset = AvatarOffset + glm::vec3(1.f, 7.f, -0.02f);
    static constexpr glm::vec3 ControlIconOffset = AvatarOffset + glm::vec3(115.f, 42.f, 0.f);
    static constexpr float LineHeight = 11.f; //-8.f

    for (auto e : m_avatarListEntities)
    {
        m_uiScene.destroyEntity(e);
    }
    m_avatarListEntities.clear();

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& fontSmall = m_sharedData.sharedResources->fonts.get(FontID::Info);

    static constexpr glm::vec3 RootPos(131.f, 174.f, 0.f);
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        auto localPos = glm::vec3(
            173.f * static_cast<float>(i % 2),
            -(LineHeight + AvatarThumbSize.y) * static_cast<float>(i / 2),
            0.1f);
        localPos += RootPos;

        //player name
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(m_sharedData.localConnectionData.playerData[i].name);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextGoldColour);
        centreText(entity);

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //add avatar background
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + BGOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ThumbBackground];

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //add avatar preview
        applyAvatarColours(i);
        updateThumb(i);
                
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + AvatarOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>(m_avatarThumbs[i].getTexture());

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);


        //add edit button
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + EditButtonOffset);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString("EDIT");
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        auto bounds = cro::Text::getLocalBounds(entity);
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_avatarEditCallbacks[i];

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        //enable CPU checkbox
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + CPUTextOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(fontSmall).setString("CPU");
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + CPUHighlightOffset);
        entity.addComponent<cro::Drawable2D>().setVertexData(
            {
                cro::Vertex2D(glm::vec2(0.f, 5.f)),
                cro::Vertex2D(glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(5.f)),
                cro::Vertex2D(glm::vec2(5.f, 0.f))
            }
        );
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float)
        {
            cro::Colour c = m_sharedData.localConnectionData.playerData[i].isCPU ? TextGoldColour : cro::Colour::Transparent;
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            for (auto& v : verts)
            {
                v.colour = c;
            }
        };
        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);


        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + CPUButtonOffset);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::CPUHighlight];
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().setUserData<const std::uint32_t>(i); //just use this to store our index for button callback
        entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::CPUHighlight].getTextureBounds();
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_cpuOptionCallbacks[0];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_cpuOptionCallbacks[1];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_cpuOptionCallbacks[2];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Enter] = m_cpuOptionCallbacks[3];
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = m_cpuOptionCallbacks[4];
        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);


        //input type icon
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(localPos + ControlIconOffset);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Keyboard];
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

        struct ControlUserData final
        {
            std::size_t prevControllerCount = 0;
            std::array<cro::Entity, 3u> arrowEntities = {}; //includes number indicator
        };

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<ControlUserData>();
        entity.getComponent<cro::Callback>().function =
            [&, i](cro::Entity e, float)
        {
            auto& controlData = e.getComponent<cro::Callback>().getUserData<ControlUserData>();
            auto controllerCount = cro::GameController::getControllerCount();
            if (controllerCount != controlData.prevControllerCount)
            {
                //we need to delete specifically the button
                //arrows should we be going to < 2 controllers
                if (controllerCount < 2u && controlData.arrowEntities[0].isValid())
                {
                    //remove from main list first
                    m_avatarListEntities.erase(std::remove_if(m_avatarListEntities.begin(), m_avatarListEntities.end(),
                        [&controlData](const cro::Entity a)
                        {
                            return (a == controlData.arrowEntities[0] || a == controlData.arrowEntities[1]);
                        }), m_avatarListEntities.end());

                    m_uiScene.destroyEntity(controlData.arrowEntities[0]);
                    m_uiScene.destroyEntity(controlData.arrowEntities[1]);
                    m_uiScene.destroyEntity(controlData.arrowEntities[2]);
                    controlData.arrowEntities = {};
                }


                if (controllerCount == 0)
                {
                    e.getComponent<cro::Sprite>() = m_sprites[SpriteID::Keyboard];
                }
                else
                {
                    e.getComponent<cro::Sprite>() = m_sprites[SpriteID::Controller];

                    //add buttons for selecting controller ID
                    if (controllerCount > 1)
                    {
                        auto ent = m_uiScene.createEntity();
                        ent.addComponent<cro::Transform>().setPosition({ -22.f, 7.f, 0.f });
                        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
                        ent.addComponent<cro::Drawable2D>();
                        ent.addComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowLeft];

                        ent.addComponent<cro::UIInput>().area = m_sprites[SpriteID::ArrowLeft].getTextureBounds();
                        ent.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
                        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_controllerCallbackIDs[ControllerCallbackID::EnterLeft];
                        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_controllerCallbackIDs[ControllerCallbackID::ExitLeft];
                        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_controllerCallbackIDs[ControllerCallbackID::Dec01 + i];

                        e.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                        m_avatarListEntities.push_back(ent);
                        controlData.arrowEntities[0] = ent;

                        ent = m_uiScene.createEntity();
                        ent.addComponent<cro::Transform>().setPosition({ 55.f, 7.f, 0.f });
                        ent.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
                        ent.addComponent<cro::Drawable2D>();
                        ent.addComponent<cro::Sprite>() = m_sprites[SpriteID::ArrowRight];

                        ent.addComponent<cro::UIInput>().area = m_sprites[SpriteID::ArrowRight].getTextureBounds();
                        ent.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
                        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_controllerCallbackIDs[ControllerCallbackID::EnterRight];
                        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_controllerCallbackIDs[ControllerCallbackID::ExitRight];
                        ent.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_controllerCallbackIDs[ControllerCallbackID::Inc01 + i];

                        e.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                        m_avatarListEntities.push_back(ent);
                        controlData.arrowEntities[1] = ent;

                        //add indicator for current controller ID
                        ent = m_uiScene.createEntity();
                        ent.addComponent<cro::Transform>().setPosition({ 25.f, 4.f, 0.f });
                        ent.addComponent<cro::Drawable2D>();
                        ent.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setCharacterSize(InfoTextSize);
                        ent.getComponent<cro::Text>().setFillColour(TextNormalColour);
                        ent.addComponent<cro::Callback>().active = true;
                        ent.getComponent<cro::Callback>().function =
                            [&, i](cro::Entity numEnt, float)
                        {
                            numEnt.getComponent<cro::Text>().setString(std::to_string(m_sharedData.controllerIDs[i] + 1));
                            centreText(numEnt);
                        };

                        e.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
                        m_avatarListEntities.push_back(ent);
                        controlData.arrowEntities[2] = ent;

                        //auto select next available controller based on this player ID
                        m_sharedData.controllerIDs[i] = std::min(i, static_cast<std::uint32_t>(controllerCount) - 1);
                    }
                }

                auto bb = e.getComponent<cro::Sprite>().getTextureBounds();
                e.getComponent<cro::Transform>().setOrigin({ std::floor(bb.width / 2.f), std::floor(bb.height / 2.f) });
            }
            controlData.prevControllerCount = controllerCount;
        };

        m_avatarMenu.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_avatarListEntities.push_back(entity);
    }
}

void MenuState::updateLobbyData(const net::NetEvent& evt)
{
    ConnectionData cd;
    if (cd.deserialise(evt.packet))
    {
        m_sharedData.connectionData[cd.connectionID] = cd;
    }

    updateLobbyAvatars();
}

void MenuState::updateLobbyAvatars()
{
    //TODO detect only the avatars which changed
    //so we don't needlessly update textures?

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::LobbyList;
    cmd.action = [&](cro::Entity e, float)
    {
        auto& children = e.getComponent<cro::Callback>().getUserData<std::vector<cro::Entity>>();
        for (auto c : children)
        {
            m_uiScene.destroyEntity(c);
        }
        children.clear();

        const auto applyTexture = [&](std::size_t idx, cro::Texture& targetTexture, const std::array<uint8_t, 4u>& flags)
        {
            m_playerAvatars[idx].setTarget(targetTexture);
            for (auto j = 0u; j < flags.size(); ++j)
            {
                m_playerAvatars[idx].setColour(pc::ColourKey::Index(j), flags[j]);
            }
            m_playerAvatars[idx].apply();
        };
        const auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
        cro::SimpleText simpleText(font);
        simpleText.setCharacterSize(UITextSize);
        simpleText.setFillColour(TextNormalColour);

        cro::Image img;
        img.create(8, 8, cro::Colour::White);
        cro::Texture texture;
        texture.loadFromImage(img);
        cro::SimpleQuad simpleQuad(texture);

        glm::vec2 textPos(0.f);
        std::int32_t h = 0;
        for (const auto& c : m_sharedData.connectionData)
        {
            if (c.connectionID < m_sharedData.nameTextures.size())
            {
                m_sharedData.nameTextures[c.connectionID].clear(cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha / 3.f));

                for (auto i = 0u; i < c.playerCount; ++i)
                {
                    simpleText.setString(c.playerData[i].name);
                    auto bounds = simpleText.getLocalBounds().width;
                    simpleText.setPosition({ std::round((LabelTextureSize.x - bounds) / 2.f), (i * (LabelTextureSize.y / 4)) + 4.f });
                    simpleText.draw();

                    simpleQuad.setPosition({ 4.f,(i * (LabelTextureSize.y / 4)) + 4.f });
                    simpleQuad.setColour(cro::Colour(pc::Palette[c.playerData[i].avatarFlags[1]].light));
                    simpleQuad.draw();

                    simpleQuad.move({ 112.f, 0.f });
                    simpleQuad.setColour(cro::Colour(pc::Palette[c.playerData[i].avatarFlags[0]].light));
                    simpleQuad.draw();
                }
                m_sharedData.nameTextures[c.connectionID].display();
            }

            //create a list of names on the connected client
            cro::String str;
            for (auto i = 0u; i < c.playerCount; ++i)
            {
                str += c.playerData[i].name + "\n";
                auto avatarIndex = indexFromAvatarID(c.playerData[i].skinID);
                applyTexture(avatarIndex, m_sharedData.avatarTextures[c.connectionID][i], c.playerData[i].avatarFlags);
            }

            if (str.empty())
            {
                str = "Empty";
            }

            textPos.x = (h % 2) * 274.f;
            textPos.y = (h / 2) * -60.f;

            textPos.x += 1.f;

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(textPos);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setString(str);
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);

            //add a ready status for that client
            static constexpr glm::vec2 ReadyOffset(-12.f, -5.f);
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(textPos + ReadyOffset);
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, h](cro::Entity e2, float)
            {
                cro::Colour colour = m_readyState[h] ? TextGreenColour : LeaderboardTextDark;

                auto& verts = e2.getComponent<cro::Drawable2D>().getVertexData();
                for (auto& v : verts)
                {
                    v.colour = colour;
                }
            };
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);

            auto& verts = entity.getComponent<cro::Drawable2D>().getVertexData();
            verts =
            {
                cro::Vertex2D(glm::vec2(0.f)),
                cro::Vertex2D(glm::vec2(5.f, 0.f)),
                cro::Vertex2D(glm::vec2(0.f, 5.f)),
                cro::Vertex2D(glm::vec2(5.f))
            };
            entity.getComponent<cro::Drawable2D>().updateLocalBounds();

            //add a network status icon
            entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(textPos + ReadyOffset);
            entity.getComponent<cro::Transform>().move({ -5.f, -50.f });
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::NetStrength];
            entity.addComponent<cro::SpriteAnimation>();

            entity.addComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Callback>().function =
                [&, h](cro::Entity ent, float)
            {
                if (m_sharedData.connectionData[h].playerCount == 0)
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                }
                else
                {
                    ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                    auto index = std::min(4u, m_sharedData.connectionData[h].pingTime / 30);
                    ent.getComponent<cro::SpriteAnimation>().play(index);
                }
            };

            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);

            h++;
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void MenuState::showPlayerConfig(bool visible, std::uint8_t playerIndex)
{
    m_activePlayerAvatar = playerIndex;

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::PlayerConfig;
    cmd.action = [&,visible](cro::Entity e, float)
    {
        float target = visible ? 1.f : 0.f;
        std::size_t menu = visible ? MenuID::PlayerConfig : MenuID::Avatar;

        e.getComponent<cro::Callback>().getUserData<std::pair<float, float>>().second = target;
        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(menu);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    cmd.targetFlags = CommandID::Menu::PlayerName;
    cmd.action = [&](cro::Entity e, float)
    {
        e.getComponent<cro::Text>().setString(m_sharedData.localConnectionData.playerData[m_activePlayerAvatar].name);
        e.getComponent<cro::Callback>().setUserData<std::uint8_t>(m_activePlayerAvatar);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //make sure the preview is set to the current player's settings
    applyAvatarColours(playerIndex);
    setPreviewModel(playerIndex);

    auto index = m_avatarIndices[m_activePlayerAvatar];

    if (visible)
    {
        m_playerAvatars[index].setColour(pc::ColourKey::Bottom, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[0]);
        m_playerAvatars[index].setColour(pc::ColourKey::Top, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[1]);
        m_playerAvatars[index].setColour(pc::ColourKey::Skin, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[2]);
        m_playerAvatars[index].setColour(pc::ColourKey::Hair, m_sharedData.localConnectionData.playerData[playerIndex].avatarFlags[3]);

        m_currentMenu = MenuID::PlayerConfig;

        m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = static_cast<std::int32_t>(m_ballIndices[m_activePlayerAvatar]);
    }
    else
    {
        //apply this to textures in the first slot just for preview
        //these will be updated in the correct positions once we join the lobby
        m_playerAvatars[index].setTarget(m_sharedData.avatarTextures[0][m_activePlayerAvatar]);
        m_playerAvatars[index].apply();

        //this is an assumption, but a fair one I feel.
        //I'm probably going to regret that.
        m_currentMenu = MenuID::Avatar;

        //this is a fudge to get around the edge case where the window
        //was resized while the avatar menu was open - the background menu,
        //being inactive, will have now been misplaced... calling this should
        //replace it again
        auto size = cro::App::getWindow().getSize();
        auto* msg = getContext().appInstance.getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
        msg->data0 = size.x;
        msg->data1 = size.y;
        msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;
    }
}

void MenuState::quitLobby()
{
    m_sharedData.clientConnection.connected = false;
    m_sharedData.clientConnection.connectionID = 4;
    m_sharedData.clientConnection.ready = false;
    m_sharedData.clientConnection.netClient.disconnect();

    if (m_sharedData.hosting)
    {
        m_sharedData.serverInstance.stop();
        m_sharedData.hosting = false;

        for (auto& cd : m_sharedData.connectionData)
        {
            cd.playerCount = 0;
        }
    }

    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;


    //delete the course selection entities as they'll be re-created as needed
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::CourseSelect;
    cmd.action =
        [&](cro::Entity b, float)
    {
        m_uiScene.destroyEntity(b);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}

void MenuState::addCourseSelectButtons()
{
    //choose scoring type
    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { -62.f, 45.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    auto bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevRules;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 63.f, 45.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextRules;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //choose gimme radius
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { -62.f, -31.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevRadius;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 63.f, -31.f };
    buttonEnt.getComponent<UIElement>().relativePosition = LobbyBackgroundPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextRadius;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //choose course
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { -88.f, -19.f };
    buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.prevCourse;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 88.f, -19.f };
    buttonEnt.getComponent<UIElement>().relativePosition = CourseDescPosition;
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_courseSelectCallbacks.nextCourse;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_menuEntities[MenuID::Lobby].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());
}