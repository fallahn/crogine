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

#include "MenuState.hpp"
#include "MenuCallbacks.hpp"
#include "SharedStateData.hpp"
#include "PacketIDs.hpp"
#include "MessageIDs.hpp"
#include "MenuConsts.hpp"
#include "GameConsts.hpp"
#include "Utility.hpp"
#include "NameScrollSystem.hpp"
#include "CommandIDs.hpp"
#include "spooky2.hpp"
#include "Clubs.hpp"
#include "UnlockItems.hpp"
#include "MenuEnum.inl"
#include "TextAnimCallback.hpp"
#include "../ErrorCheck.hpp"
#include "server/ServerPacketData.hpp"
#include "../../buildnumber.h"
#include "../version/VersionNumber.hpp"

#include <AchievementStrings.hpp>
#include <Social.hpp>
#include <Input.hpp>

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
#include <crogine/gui/Gui.hpp>

#include <cstring>
#include <iomanip>

namespace
{
#include "RandNames.hpp"
#include "shaders/PostProcess.inl"

    const char VersionNumber[] =
    {
        ' ',
        BUILD_YEAR_CH0, BUILD_YEAR_CH1, BUILD_YEAR_CH2, BUILD_YEAR_CH3,
        BUILD_MONTH_CH0, BUILD_MONTH_CH1,
        BUILD_DAY_CH0, BUILD_DAY_CH1,
        '.',
        BUILD_HOUR_CH0, BUILD_HOUR_CH1,
        BUILD_MIN_CH0, BUILD_MIN_CH1,
        BUILD_SEC_CH0, BUILD_SEC_CH1,
        '.',
        '\0'
    };

    struct ScorecardCallbackData final
    {
        std::int32_t direction = 0;
        std::size_t targetMenuID = MenuState::MenuID::Dummy;
    };

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

    //updates start/quit buttons with correct navigation indices
    std::function<void(std::int32_t)> navigationUpdate;
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

void MenuState::parseCourseDirectory(const std::string& rootDir, bool isUser)
{
    auto root = rootDir;
    if (!isUser)
    {
        //macOS shenanigans.
        auto rpath = cro::FileSystem::getResourcePath();
        if (rpath.find(root) == std::string::npos)
        {
            root = rpath + root;
        }
    }

    if (!cro::FileSystem::directoryExists(root))
    {
        cro::FileSystem::createDirectory(root);
    }

    auto directories = cro::FileSystem::listDirectories(root);

    //at least be consistent across platforms
    std::sort(directories.begin(), directories.end(), [](const  std::string& a, const std::string& b) {return a < b; });

    m_courseIndices[m_currentRange].start = m_courseData.size();

    std::int32_t courseNumber = 1;
    for (const auto& dir : directories)
    {
        if (dir == "tutorial")
        {
            continue;
        }

        auto courseFile = rootDir + dir + "/course.data";

        //because macs are special, obvs
        auto testPath = courseFile;
        if (!isUser)
        {
            testPath = cro::FileSystem::getResourcePath() + testPath;
        }

        if (cro::FileSystem::fileExists(testPath))
        {
            cro::String title;
            std::string description;
            std::int32_t holeCount = 0;
            std::vector<std::int32_t> parVals;

            cro::ConfigFile cfg;
            cfg.loadFromFile(courseFile, !isUser);

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
                    cro::ConfigFile hole;
                    if (hole.loadFromFile(prop.getValue<std::string>()))
                    {
                        if (auto parProp = hole.findProperty("par"); parProp != nullptr)
                        {
                            parVals.push_back(parProp->getValue<std::int32_t>());
                        }

                        holeCount++;
                    }
                }
                else if (propName == "title")
                {
                    title = prop.getValue<cro::String>();
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
                if (!title.empty())
                {
                    data.title = title;
                }
                if (!description.empty())
                {
                    data.description = description;
                }
                data.directory = dir;
                data.courseNumber = courseNumber;
                data.isUser = isUser;
                data.holeCount[0] = "All " + std::to_string(std::min(holeCount, 18)) + " holes";
                data.holeCount[1] = "Front " + std::to_string(std::max(holeCount / 2, 1));
                data.holeCount[2] = "Back " + std::to_string(std::min(holeCount - (holeCount / 2), 9));
                data.parVals.swap(parVals);

                courseNumber++;
                m_courseIndices[m_currentRange].count++;
            }
        }


        //check for thumbnail
        courseFile = rootDir + dir + "/preview.png";
        testPath = courseFile;
        if (!isUser)
        {
            testPath = cro::FileSystem::getResourcePath() + testPath;
        }

        std::unique_ptr<cro::Texture> t = std::make_unique<cro::Texture>();
        if (cro::FileSystem::fileExists(testPath) &&
            t->loadFromFile(courseFile))
        {
            m_courseThumbs.insert(std::make_pair(dir, std::move(t)));
        }

        //and video thumbnail
        courseFile = rootDir + dir + "/preview.mpg";
        testPath = courseFile;
        if (!isUser)
        {
            testPath = cro::FileSystem::getResourcePath() + testPath;
        }

        if (cro::FileSystem::fileExists(testPath))
        {
            m_videoPaths.insert(std::make_pair(dir, courseFile));
        }

        //TODO remove me - this prevents parsing the incomplete course for now
        /*if (courseNumber == 10)
        {
            break;
        }*/
    }

    //moved to createUI() because this func gets called multiple times
    /*if (!m_courseData.empty())
    {
        m_sharedData.courseIndex = std::min(m_sharedData.courseIndex, m_courseData.size() - 1);
    }*/
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
    m_currentRange = Range::Official;
    parseCourseDirectory(ConstVal::MapPath, false);

    m_currentRange = Range::Custom;
    parseCourseDirectory(cro::App::getPreferencePath() + ConstVal::UserMapPath, true);

    m_currentRange = Range::Official; //make this default
    if (m_sharedData.courseIndex == m_courseIndices[Range::Custom].start)
    {
        m_currentRange = Range::Custom;
    }

    if (!m_courseData.empty())
    {
        m_sharedData.courseIndex = std::min(m_sharedData.courseIndex, m_courseData.size() - 1);
    }

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
        [&](cro::Entity e, float)
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

    createMainMenu(rootNode, mouseEnterCallback, mouseExitCallback);
    createAvatarMenu(rootNode);
#ifdef USE_GNS
    createBrowserMenu(rootNode, mouseEnterCallback, mouseExitCallback);
#else
    createJoinMenu(rootNode, mouseEnterCallback, mouseExitCallback);
#endif
    createLobbyMenu(rootNode, mouseEnterCallback, mouseExitCallback);

    //diplays version number
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 2.f, 10.f, 0.05f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Version: " + StringVer + VersionNumber + BUILDNUMBER_STR);
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    auto versionEnt = entity;

    //ui viewport is set 1:1 with window, then the scene
    //is scaled to best-fit to maintain pixel accuracy of text.
    auto updateView = [&, rootNode, courseEnt, versionEnt](cro::Camera& cam) mutable
    {
        auto windowSize = GolfGame::getActiveTarget()->getSize();
        glm::vec2 size(windowSize);

        cam.setOrthographic(0.f, size.x, 0.f, size.y, -2.f, 10.f);
        cam.viewport = { 0.f, 0.f, 1.f, 1.f };

        m_viewScale = glm::vec2(getViewScale());
        rootNode.getComponent<cro::Transform>().setScale(m_viewScale);
        rootNode.getComponent<cro::Transform>().setPosition(m_menuPositions[m_currentMenu] * m_viewScale);

        glm::vec2 courseScale(m_sharedData.pixelScale ? m_viewScale.x : 1.f);
        courseEnt.getComponent<cro::Transform>().setScale(courseScale);
        courseEnt.getComponent<cro::Callback>().active = true; //makes sure to delay so updating the texture size is complete first
        courseEnt.getComponent<cro::Transform>().setPosition(glm::vec3(size / 2.f, -1.f));

        static constexpr glm::vec2 versionPos(2.f, 10.f);
        versionEnt.getComponent<cro::Transform>().setScale(m_viewScale);
        versionEnt.getComponent<cro::Transform>().setPosition(versionPos * m_viewScale);

        refreshUI();
    };

    entity = m_uiScene.getActiveCamera();
    entity.getComponent<cro::Transform>();
    entity.getComponent<cro::Camera>().resizeCallback = updateView;
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
    //auto cartEnt = entity;

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

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -100.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("cursor");
    entity.addComponent<cro::SpriteAnimation>().play(0);
    bannerEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

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

    auto validData = Social::isValid();
    if (validData
        &&!m_courseData.empty()
        && !m_sharedData.ballInfo.empty()
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
                        m_sharedData.clubSet = m_sharedData.preferredClubSet;

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
                        m_sharedData.clubSet = m_sharedData.preferredClubSet;

                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                        menuEntity.getComponent<cro::Callback>().active = true;

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });

        //facilities menu
        entity = createButton("19th Hole");
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt)
                {
                    if (activated(evt))
                    {
                        requestStackPush(StateID::Practice);
                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });
    }
    else
    {
        std::string str = "Error:\n";
        if (!validData)
        {
            str += "Invalid Course Data\n";
        }
        if (m_courseData.empty())
        {
            str += "No course data found\n";
        }
        if (m_sharedData.ballInfo.empty())
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

#ifdef USE_GNS
    //join chat button
    //if (!Social::isSteamdeck())
    //{
    //    entity = m_uiScene.createEntity();
    //    entity.addComponent<cro::Transform>();
    //    entity.addComponent<UIElement>().absolutePosition = { 0.f, 0.f };
    //    entity.getComponent<UIElement>().relativePosition = { 0.12f, 0.89f };
    //    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;

    //    entity.getComponent<cro::Transform>().setScale({ 0.f, 0.f });
    //    entity.addComponent<cro::Callback>().active = true;
    //    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    //    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    //    menuTransform.addChild(entity.getComponent<cro::Transform>());

    //    auto buttonNode = entity;

    //    entity = m_uiScene.createEntity();
    //    entity.addComponent<cro::Transform>();
    //    entity.addComponent<cro::Drawable2D>();
    //    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("chat_button");
    //    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    //    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    //    entity.addComponent<cro::Callback>().active = true;
    //    entity.getComponent<cro::Callback>().function =
    //        [buttonNode](cro::Entity e, float dt)
    //        {
    //            if (buttonNode.getComponent<cro::Callback>().active)
    //            {
    //                //rotate with scale
    //                const float scale = -buttonNode.getComponent<cro::Transform>().getScale().x;
    //                e.getComponent<cro::Transform>().setRotation(cro::Util::Const::PI * scale);
    //            }
    //            else
    //            {
    //                //gently rotate
    //                e.getComponent<cro::Transform>().rotate(-dt * 0.1f);
    //            }
    //        };
    //    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //    entity = createButton("Join\nChatroom");
    //    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    //    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    //    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    //    bounds = cro::Text::getLocalBounds(entity);
    //    entity.getComponent<cro::Transform>().setRotation(0.09f);
    //    entity.getComponent<cro::Transform>().setPosition(glm::vec3(0.f, 0.f, 0.1f));
    //    entity.getComponent<cro::Transform>().setOrigin({ 0.f, std::floor(-bounds.height / 2.f) - 2.f});
    //    entity.getComponent<cro::UIInput>().area = bounds;
    //    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
    //        m_uiScene.getSystem<cro::UISystem>()->addCallback([](cro::Entity, const cro::ButtonEvent& evt)
    //            {
    //                if (activated(evt))
    //                {
    //                    Social::joinChatroom();
    //                }
    //            });
    //    buttonNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    //}
#endif

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
                            auto* msg = postMessage<SystemEvent>(cl::MessageID::SystemMessage);
                            msg->type = SystemEvent::RequestOSK;
                            msg->data = 0;
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
            entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto mouseEnterHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left -= bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
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
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
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
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit(); //finish any pending changes
                    refreshUI();

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    if (!m_sharedData.targetIP.empty() &&
                        !m_sharedData.clientConnection.connected)
                    {
                        m_matchMaking.joinGame(0);
                    }

                    auto defaultCallback = e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown];
                    e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = 0;

                    auto tempEnt = m_uiScene.createEntity();
                    tempEnt.addComponent<cro::Callback>().active = true;
                    tempEnt.getComponent<cro::Callback>().setUserData<std::pair<std::uint32_t, float>>(defaultCallback, 0.f);
                    tempEnt.getComponent<cro::Callback>().function =
                        [&, e](cro::Entity t, float dt) mutable
                    {
                        auto& [cb, currTime] = t.getComponent<cro::Callback>().getUserData<std::pair<std::uint32_t, float>>();
                        currTime += dt;
                        if (currTime > 1)
                        {
                            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = cb;
                            t.getComponent<cro::Callback>().active = false;
                            m_uiScene.destroyEntity(t);
                        }
                    };
                }
            });
    
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createBrowserMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
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
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_browser.spt", m_resources.textures);


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

    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.55f };
    entity.getComponent<UIElement>().depth = -0.01f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.rootNode = entity;


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
            entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f });
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto mouseEnterHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left -= bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
        });

    auto arrowSelected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&,entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::Callback>().active = true;
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto arrowUnselected = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto lobbyActivated = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                std::size_t idx = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
                idx += (LobbyPager::ItemsPerPage * m_lobbyPager.currentPage);

                if (idx < m_lobbyPager.lobbyIDs.size())
                {
                    //this will be reset next time the page is scrolled, and prevents double presses
                    e.getComponent<cro::UIInput>().enabled = false;

                    m_audioEnts[AudioID::Start].getComponent<cro::AudioEmitter>().play();

                    m_matchMaking.joinGame(m_lobbyPager.lobbyIDs[idx]);
                    m_sharedData.lobbyID = m_lobbyPager.lobbyIDs[idx];
                }

                refreshUI();
            }
        });

    //entry highlights
    glm::vec3 highlightPos(6.f, 161.f, 0.2f);
    for (auto i = 0u; i < LobbyPager::ItemsPerPage; ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(highlightPos);
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("lobby_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());

        entity.addComponent<cro::Callback>().setUserData<std::uint32_t>(i); //used by button activated callback
        entity.getComponent<cro::Callback>().function = HighlightAnimationCallback();

        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = lobbyActivated;

        m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_lobbyPager.slots.push_back(entity);
        highlightPos.y -= entity.getComponent<cro::Sprite>().getTextureBounds().height;
    }

    auto updateActiveSlots = [&]()
    {
        auto start = m_lobbyPager.currentPage * LobbyPager::ItemsPerPage;
        auto end = start + LobbyPager::ItemsPerPage;

        for (auto i = start; i < end; ++i)
        {
            m_lobbyPager.slots[i % LobbyPager::ItemsPerPage].getComponent<cro::UIInput>().enabled = (i < m_lobbyPager.lobbyIDs.size());
        }
    };

    struct ButtonIndex final
    {

    };


    //button left
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 13.f, 5.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_left");
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonLeft[0] = entity;

    //highlight left
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 11.f, 3.f, 0.2f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight_left");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, updateActiveSlots](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    if (m_lobbyPager.pages.size() > 1)
                    {
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        m_lobbyPager.currentPage = (m_lobbyPager.currentPage + (m_lobbyPager.pages.size() - 1)) % m_lobbyPager.pages.size();
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                        //only enable item slots as available on the new page
                        updateActiveSlots();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonLeft[1] = entity;

    auto* uiSystem = m_uiScene.getSystem<cro::UISystem>();

    //friends overlay
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 200.f, 10.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("friends_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f)});
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    Social::findFriends();
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            });
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    const auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    //opens to steam group
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 300.f, 15.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Meet New Players");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    bounds = cro::Text::getLocalBounds(entity);
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] =
        uiSystem->addCallback([](cro::Entity e) 
            {
                e.getComponent<cro::Text>().setFillColour(TextGoldColour);
                e.getComponent<cro::AudioEmitter>().play();
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] =
        uiSystem->addCallback([](cro::Entity e) 
            {
                e.getComponent<cro::Text>().setFillColour(TextNormalColour);
            });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        uiSystem->addCallback([](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    Social::showWebPage("https://steamcommunity.com/groups/supervideoclubhouse");
                }
            });
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //button right
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 382.f, 5.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("button_right");
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonRight[0] = entity;

    //highlight right
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 380.f, 3.f, 0.2f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("highlight_right");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = arrowSelected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = arrowUnselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, updateActiveSlots](cro::Entity e, const cro::ButtonEvent& evt)
            {
                if (activated(evt))
                {
                    if (m_lobbyPager.pages.size() > 1)
                    {
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                        m_lobbyPager.currentPage = (m_lobbyPager.currentPage + 1) % m_lobbyPager.pages.size();
                        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(1.f));

                        updateActiveSlots();

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyPager.buttonRight[1] = entity;

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
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&, menuEntity](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                    menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Avatar;
                    menuEntity.getComponent<cro::Callback>().active = true;

                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //refresh
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setOrigin({ 0.f, -2.f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -20.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.98f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("refresh");
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Join);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    m_matchMaking.refreshLobbyList(Server::GameMode::Golf);
                    updateLobbyList(); //clears existing display until message comes in
                }
            });

    menuTransform.addChild(entity.getComponent<cro::Transform>());


#ifdef USE_GNS
    //struct ListData final
    //{
    //    float refreshTime = 5.f;
    //    std::vector<cro::Entity> listItems;
    //};

    ////list of friends currently in a lobby
    //entity = m_uiScene.createEntity();
    //entity.addComponent<cro::Transform>();
    //entity.addComponent<cro::Callback>().active = true;
    //entity.getComponent<cro::Callback>().setUserData<ListData>();
    //entity.getComponent<cro::Callback>().function =
    //    [&](cro::Entity e, float dt)
    //{
    //    if (m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Join)
    //    {
    //        auto& [currTime, listItems] = e.getComponent<cro::Callback>().getUserData<ListData>();
    //        currTime -= dt;
    //        if (currTime < 0)
    //        {
    //            for (auto e : listItems)
    //            {
    //                m_uiScene.destroyEntity(e);
    //            }
    //            listItems.clear();

    //            const auto newList = m_matchMaking.getFriendGames();
    //            for (const auto& game : newList)
    //            {
    //                //TODO create a button ent to join the lobby
    //                //TODO rather than create a background drawable for
    //                //each button use a single repeated texture on the
    //                //parent entity
    //                auto ent = m_uiScene.createEntity();
    //                ent.addComponent<cro::Transform>();



    //                listItems.push_back(ent);
    //                e.getComponent<cro::Transform>().addChild(ent.getComponent<cro::Transform>());
    //            }

    //            currTime += 5.f;
    //        }
    //    }
    //};

    //menuTransform.addChild(entity.getComponent<cro::Transform>());
#endif

    updateLobbyList();
}

void MenuState::createLobbyMenu(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Lobby] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());
    m_textChat.setRootNode(menuEntity);

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Lobby]);

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_resources.textures);

    cro::SpriteSheet spriteSheetV2;
    spriteSheetV2.loadFromFile("assets/golf/sprites/lobby_menu_v2.spt", m_resources.textures);
    m_sprites[SpriteID::ReadyStatus] = spriteSheetV2.getSprite("ready_indicator");
    m_sprites[SpriteID::LobbyLeft] = spriteSheetV2.getSprite("prev_course");
    m_sprites[SpriteID::LobbyRight] = spriteSheetV2.getSprite("next_course");
    m_sprites[SpriteID::WeatherHighlight] = spriteSheetV2.getSprite("weather_highlight");

    //title
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.9f };
    entity.getComponent<UIElement>().depth = 1.6f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::TitleText;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("title");
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //chat hint
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().relativePosition = { 0.f, 1.f };
    entity.getComponent<UIElement>().depth = 1.6f;
    entity.getComponent<UIElement>().absolutePosition = { 2.f, -4.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ChatHint;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("     to Chat");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    cro::SpriteSheet buttonSheet;
    buttonSheet.loadFromFile("assets/golf/sprites/controller_buttons.spt", m_resources.textures);

    auto hintEnt = entity;
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 0.f, -11.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = buttonSheet.getSprite("button_y");
    entity.addComponent<cro::SpriteAnimation>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ChatHint;
    hintEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //background
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("background");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    entity.getComponent<UIElement>().absolutePosition = { 0.f, 10.f };
    entity.getComponent<UIElement>().depth = -0.2f;
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    m_lobbyWindowEntities[LobbyEntityID::Background] = entity;
    auto bgEnt = entity;
    auto bgBounds = bounds;

#ifdef USE_GNS
    //scrolls info about the selected course
    auto& labelFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 100.f, 0.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(labelFont);
    entity.getComponent<cro::Text>().setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float dt)
    {
        if (m_currentMenu == MenuID::Lobby)
        {
            static constexpr float BasePosY = 23.f;
            static constexpr float LineHeight = 13.f;

            auto scrollBounds = cro::Text::getLocalBounds(e);

            auto pos = e.getComponent<cro::Transform>().getPosition();

            pos.x -= 20.f * dt;
            pos.y = BasePosY + std::floor(scrollBounds.height - LineHeight) ;
            pos.z = 0.3f;

            static constexpr float Offset = 232.f;
            const auto bgWidth = bounds.width;
            if (pos.x < -scrollBounds.width + Offset)
            {
                pos.x = bgWidth;
            }

            e.getComponent<cro::Transform>().setPosition(pos);

            cro::FloatRect cropping = { -pos.x + Offset, -16.f + (BasePosY - pos.y), (bgWidth)-(Offset + 10.f), 18.f};
            e.getComponent<cro::Drawable2D>().setCroppingArea(cropping);
        }
    };
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::CourseTicker] = entity;
#endif

    //display the score type
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 364.f, 210.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(ScoreTypes[m_sharedData.scoreType]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ScoreType | CommandID::Menu::UIElement;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //and score description
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 364.f, 194.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString(RuleDescriptions[m_sharedData.scoreType]);
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::ScoreDesc | CommandID::Menu::UIElement;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //gimme radius
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString(GimmeString[m_sharedData.gimmeRadius]);
    entity.addComponent<UIElement>().absolutePosition = {364.f, 104.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::GimmeDesc | CommandID::Menu::UIElement;
    centreText(entity);
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 364.f, 88.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.getComponent<cro::Text>().setString("For faster game play the ball is considered holed\nwhen it's within this radius");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    const auto ruleButtonEnable = [&](cro::Entity e, float)
    {
        //hack to disable the button when course selection is visible
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y +
            m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().getScale().y == 0;
    };


    //clubset selection
    if (Social::getClubLevel())
    {
        m_sharedData.preferredClubSet %= (Social::getClubLevel() + 1);

        //button icon
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<UIElement>().absolutePosition = { 306.f, 48.f };
        entity.getComponent<UIElement>().depth = 0.1f;
        entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("clubset_button");
        entity.addComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
        entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        auto buttonEnt = entity;

        //button actual
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<UIElement>().absolutePosition = { 304.f, 46.f };
        entity.getComponent<UIElement>().depth = 0.1f;
        entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("clubset_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.addComponent<cro::UIInput>().area = bounds;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        entity.getComponent<cro::UIInput>().setSelectionIndex(RulesClubset);

        entity.getComponent<cro::UIInput>().setNextIndex(LobbyInfoA, LobbyInfoA);
        entity.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseA, LobbyCourseA);
        m_lobbyButtonContext.rulesClubset = entity;

        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, buttonEnt](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    m_sharedData.preferredClubSet = (m_sharedData.preferredClubSet + 1) % (Social::getClubLevel() + 1);
                    buttonEnt.getComponent<cro::SpriteAnimation>().play(m_sharedData.preferredClubSet);
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    //make sure the server knows what we request so it can be considered
                    //when choosing a club set limit in MP games
                    std::uint16_t data = (m_sharedData.clientConnection.connectionID << 8) | std::uint8_t(m_sharedData.preferredClubSet);
                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClubLevel, data, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                    m_sharedData.clubSet = m_sharedData.clubLimit ? m_sharedData.clubSet : m_sharedData.preferredClubSet;
                }
            });

        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().function = ruleButtonEnable;
        bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }

    navigationUpdate = nullptr; //we're using nasty static hack so make sure to reset this

    const auto selectNext = [&](std::int32_t idx)
    {
        auto e = m_uiScene.createEntity();
        e.addComponent<cro::Callback>().active = true;
        e.getComponent<cro::Callback>().function =
            [&, idx](cro::Entity f, float)
        {
            m_uiScene.getSystem<cro::UISystem>()->selectByIndex(idx);
            f.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(f);
        };
    };

    //button for course selection
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("course_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 230.f, 31.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyCourseA);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyInfoA, LobbyQuit);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoA, Social::getClubLevel() ? RulesClubset : LobbyQuit);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, selectNext](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 0.f, 0.f });

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces a visibility refresh

                navigationUpdate(LobbyCourseA);

                selectNext(LobbyRulesA);
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = ruleButtonEnable;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.lobbyCourseA = entity;

    //button for lobby info
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("course_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 408.f, 31.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyInfoA);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyCourseA, LobbyStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseA, Social::getClubLevel() ? RulesClubset : LobbyStart);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, selectNext](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 0.f, 0.f });
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 1.f, 1.f });

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces a visibility refresh

                navigationUpdate(LobbyInfoA);

                selectNext(LobbyCourseB);
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = ruleButtonEnable;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.lobbyInfoA = entity;


    //display lobby members - updateLobbyAvatars() adds the text ents to this.
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 21.f, bgBounds.height - 28.f };
    entity.getComponent<UIElement>().depth = 0.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::LobbyList;
    entity.addComponent<cro::Callback>().setUserData<std::vector<cro::Entity>>(); //abuse this component to store handles to the text children.
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //displays a message if current rule type requires more players
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 118.f, 20.f, 0.4f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setString("NEED MORE PLAYERS");
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextHighlightColour);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    entity.addComponent<cro::Callback>().function = HighlightAnimationCallback();
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::MinPlayerCount] = entity;

    //displays the thumbnails for the selected course
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("course_background");
    entity.addComponent<UIElement>().absolutePosition = { 230.f, 31.f };
    entity.getComponent<UIElement>().depth = 1.2f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto thumbBgEnt = entity;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection] = thumbBgEnt;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 16.f, 53.f, 0.2f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::HoleThumb] = entity;

    /*entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 85.f, 140.f, 0.4f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("Course of the Month!");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::MonthlyCourse] = entity;*/

    //hole count
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 86.f, 45.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseHoles;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //checkbox to show reverse status
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 176.f, 156.f };
    entity.getComponent<UIElement>().depth = 0.01f;

    bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float)
    {
        auto b = bounds;
        if (m_sharedData.reverseCourse)
        {
            b.bottom -= bounds.height;
        }
        e.getComponent<cro::Sprite>().setTextureRect(b);
    };
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //checkbox to show fast CPU status
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 176.f, 130.f };
    entity.getComponent<UIElement>().depth = 0.01f;

    bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float)
    {
        auto b = bounds;
        if (m_sharedData.fastCPU)
        {
            b.bottom -= bounds.height;
        }
        e.getComponent<cro::Sprite>().setTextureRect(b);
    };
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //checkbox to show clubset limit status
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 176.f, 117.f };
    entity.getComponent<UIElement>().depth = 0.01f;

    bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float)
        {
            auto b = bounds;
            if (m_sharedData.clubLimit)
            {
                b.bottom -= bounds.height;
            }
            e.getComponent<cro::Sprite>().setTextureRect(b);
        };
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //checkbox to show night time status
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 176.f, 104.f };
    entity.getComponent<UIElement>().depth = 0.01f;

    bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, bounds](cro::Entity e, float)
        {
            auto b = bounds;
            if (m_sharedData.nightTime)
            {
                b.bottom -= bounds.height;
            }
            e.getComponent<cro::Sprite>().setTextureRect(b);
        };
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //weather icon
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("weather_icon");
    auto iconEnt = entity;
    bounds = entity.getComponent<cro::Sprite>().getTextureRect();

    //text for current weather type
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<UIElement>().absolutePosition = { 208.f, 82.f };
    entity.getComponent<UIElement>().depth = 0.01f;

    entity.addComponent<cro::Text>(smallFont);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setString(WeatherStrings[m_sharedData.weatherType]);
    centreText(entity);
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<std::uint32_t>(WeatherType::Count);
    entity.getComponent<cro::Callback>().function =
        [&, iconEnt, bounds](cro::Entity e, float) mutable
        {
            auto& lastType = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
            if (lastType != m_sharedData.weatherType)
            {
                e.getComponent<cro::Text>().setString(WeatherStrings[m_sharedData.weatherType]);
                centreText(e);

                auto x = std::round(cro::Text::getLocalBounds(e).width) + 2.f;
                iconEnt.getComponent<cro::Transform>().setPosition(glm::vec3(x, -9.f, 0.1f));

                auto rect = bounds;
                rect.left += bounds.width * m_sharedData.weatherType;
                iconEnt.getComponent<cro::Sprite>().setTextureRect(rect);
            }
            lastType = m_sharedData.weatherType;
        };

    entity.getComponent<cro::Transform>().addChild(iconEnt.getComponent<cro::Transform>());
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    


    const auto courseButtonEnable =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y != 0;
    };

    //button for rule selection
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("rules_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 90.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyRulesA);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyInfoB, LobbyQuit);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoB, LobbyQuit); //TODO update this if hosting
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, selectNext](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 0.f, 0.f });
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 0.f, 0.f });

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces a visibility refresh

                navigationUpdate(LobbyRulesA);

                selectNext(LobbyInfoA);
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = courseButtonEnable;
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.lobbyRulesA = entity;

    //button for score page
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("course_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 178.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyInfoB);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyRulesA, LobbyStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyRulesA, LobbyStart);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, selectNext](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 0.f, 0.f });
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 1.f, 1.f });

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces a visibility refresh

                navigationUpdate(LobbyInfoB);

                selectNext(LobbyCourseB);
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = courseButtonEnable;
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.lobbyInfoB = entity;


    //course title
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 134.f, 192.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseTitle;
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //course description
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 134.f, 180.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseDesc;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //current rules
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 213.f, 67.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseRules;
    entity.addComponent<cro::Text>(smallFont).setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setString("Please Wait...");
    entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Centre);
    thumbBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //displays the player info
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setScale({ 0.f, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("info_background");
    entity.addComponent<UIElement>().absolutePosition = { 230.f, 31.f };
    entity.getComponent<UIElement>().depth = 1.25f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto infoBgEnt = entity;
    m_lobbyWindowEntities[LobbyEntityID::Info] = infoBgEnt;

    const auto infoButtonEnable =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().getScale().y != 0.f;
    };

    //button for rule selection
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("rules_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 90.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyRulesB);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyCourseB, LobbyStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseB, InfoLeague);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, selectNext](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 0.f, 0.f });
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 0.f, 0.f });

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces a visibility refresh

                navigationUpdate(LobbyRulesB);

                selectNext(LobbyCourseA);
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = infoButtonEnable;
    infoBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.lobbyRulesB = entity;

    //button for course selection
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("course_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 0.f, 0.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyCourseB);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyRulesB, LobbyQuit);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyRulesB, InfoLeaderboards);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, selectNext](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 0.f, 0.f });

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //forces a visibility refresh

                navigationUpdate(LobbyCourseB);

                selectNext(LobbyRulesA);
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = infoButtonEnable;
    infoBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.lobbyCourseB = entity;

    //button to show leaderboards
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("leaderboard_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 3.f, 17.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(InfoLeaderboards);
    entity.getComponent<cro::UIInput>().setNextIndex(InfoLeague, LobbyCourseB);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseB, LobbyCourseB);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
#ifdef USE_GNS
                requestStackPush(StateID::Leaderboard);
#else
                cro::Util::String::parseURL("https://gamejolt.com/games/super-video-golf/809390/scores/872059/best");
#endif
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = infoButtonEnable;
    infoBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    
    
    
    
    //button to show league
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheetV2.getSprite("league_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 92.f, 17.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(InfoLeague);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyRulesB, LobbyRulesB);
    entity.getComponent<cro::UIInput>().setPrevIndex(InfoLeaderboards, LobbyCourseB);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                requestStackPush(StateID::League);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = infoButtonEnable;
    infoBgEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyButtonContext.infoLeague = entity;

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
        [&, entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            entity.getComponent<cro::Transform>().setScale({ 1.f, 1.f });

            m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
        });

    mouseExit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity) mutable
        {
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    auto mouseEnterHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left -= bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
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

        bool startGame = false;
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
                //TODO this needs to be updated so the resize handler correctly
                //accounts for this and the scorecard menu
                m_currentMenu = MenuID::Lobby;// MenuID::ConfirmQuit;

                if (data.startGame)
                {
                    m_uiScene.getSystem<cro::UISystem>()->selectAt(1);
                }
            }
        }
        else
        {
            data.progress = std::max(0.f, data.progress - (dt * 4.f));
            scale = cro::Util::Easing::easeOutQuint(data.progress);
            if (data.progress == 0)
            {
                e.getComponent<cro::Callback>().active = false;
                m_currentMenu = MenuID::Lobby;

                if (data.quitWhenDone)
                {
                    quitLobby();
                }
                else
                {
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Lobby);
                }
                refreshUI();
            }
        }

        
        e.getComponent<cro::Transform>().setScale(glm::vec2(scale));
    };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto confirmEnt = entity;


    //quad to darken the screen
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, bounds.height / 2.f, -0.1f });
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
        m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = confirmEnt.getComponent<cro::Callback>().active;
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
    auto messageTitleEnt = entity;

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ bounds.width / 2.f, 44.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(smallFont).setString("This will kick all players.");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::Magenta);
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto messageEnt = entity;



    //stash this so we can access it from the event handler (escape to ignore etc)
    quitConfirmCallback = [&, confirmEnt, shadeEnt]() mutable
    {
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::Out;
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;
        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) - 20.f, 26.f, 0.1f });
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
            [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    quitConfirmCallback();
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ (bounds.width / 2.f) + 20.f, 26.f, 0.1f });
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
                    auto& data = confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>();

                    if (!data.startGame)
                    {
                        data.dir = ConfirmationData::Out;
                        data.quitWhenDone = true;
                        confirmEnt.getComponent<cro::Callback>().active = true;
                        shadeEnt.getComponent<cro::Callback>().active = true;
                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();

                        //restore the rules tab if necessary
                        float scale = m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y;
                        if (scale == 0)
                        {
                            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
                            m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                        }
                    }
                    else
                    {
                        m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                    }
                }
            });
    centreText(entity);
    confirmEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //back
    enterConfirmCallback = [&, confirmEnt, shadeEnt, messageEnt, messageTitleEnt](bool quit) mutable
    {
        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().dir = ConfirmationData::In;
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().quitWhenDone = false;
        confirmEnt.getComponent<cro::Callback>().getUserData<ConfirmationData>().startGame = !quit;
        confirmEnt.getComponent<cro::Callback>().active = true;
        shadeEnt.getComponent<cro::Callback>().active = true;

        if (quit)
        {
            messageTitleEnt.getComponent<cro::Text>().setString("Are You Sure?");
            centreText(messageTitleEnt);
            messageEnt.getComponent<cro::Text>().setFillColour(m_sharedData.hosting ? TextNormalColour : cro::Colour::Transparent);
        }
        else
        {
            //continue message
            messageEnt.getComponent<cro::Text>().setFillColour(cro::Colour::Transparent);

            messageTitleEnt.getComponent<cro::Text>().setString("Start Game?");
            centreText(messageTitleEnt);
        }
        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
    };

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { 20.f, MenuBottomBorder };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyQuit);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyStart, LobbyStart);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyStart, LobbyRulesA); //TODO dynamically update these with active menu
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    enterConfirmCallback(true);
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto lobbyQuit = entity; //for capture, below

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
    entity.getComponent<cro::UIInput>().setSelectionIndex(LobbyStart);
    entity.getComponent<cro::UIInput>().setNextIndex(LobbyQuit, LobbyQuit);
    entity.getComponent<cro::UIInput>().setPrevIndex(LobbyQuit, LobbyInfoB); //TODO dynamically update these with active menu
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnter;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExit;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.hosting)
                    {
                        if (m_connectedPlayerCount < ScoreType::PlayerCount[m_sharedData.scoreType])
                        {
                            m_lobbyWindowEntities[LobbyEntityID::MinPlayerCount].getComponent<cro::Callback>().active = true;
                            m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().play();
                            m_audioEnts[AudioID::Nope].getComponent<cro::AudioEmitter>().setPlayingOffset(cro::seconds(0.f));
                        }
                        else
                        {
                            //check all members ready
                            bool ready = true;
                            std::int32_t clientCount = 0;
                            for (auto i = 0u; i < ConstVal::MaxClients; ++i)
                            {
                                if (m_sharedData.connectionData[i].playerCount != 0)
                                {
                                    clientCount++;
                                    if (!m_readyState[i])
                                    {
                                        ready = false;
                                        break;
                                    }
                                }
                            }

                            if (ready && m_sharedData.clientConnection.connected
                                && m_sharedData.serverInstance.running()) //not running if we're not hosting :)
                            {
                                if (clientCount > 1)
                                {
                                    m_sharedData.clientConnection.netClient.sendPacket(PacketID::RequestGameStart, std::uint8_t(sv::StateID::Golf), net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                                }
                                else
                                {
                                    enterConfirmCallback(false);
                                }
                            }

                            LogI << "Shared Data set to Hosting" << std::endl;
                        }
                    }
                    else
                    {
                        //toggle readyness but only if the selected course is locally available
                        if (!m_sharedData.mapDirectory.empty())
                        {
                            std::uint8_t ready = m_readyState[m_sharedData.clientConnection.connectionID] ? 0 : 1;
                            m_sharedData.clientConnection.netClient.sendPacket(PacketID::LobbyReady,
                                std::uint16_t(m_sharedData.clientConnection.connectionID << 8 | ready),
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
                        else
                        {
                            LogI << "Shared Data Map Directory Is Empty" << std::endl;
                        }
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    auto lobbyStart = entity;

    navigationUpdate = [&, lobbyQuit, lobbyStart](std::int32_t callerID) mutable
    {
        switch (callerID)
        {
        default: break;
        //we can pair these up because they're switching to the same menu
        case LobbyCourseA:
        case LobbyCourseB:
            lobbyQuit.getComponent<cro::UIInput>().setNextIndex(LobbyStart, LobbyStart);
            lobbyQuit.getComponent<cro::UIInput>().setPrevIndex(LobbyStart, LobbyRulesA);

            lobbyStart.getComponent<cro::UIInput>().setNextIndex(LobbyQuit, LobbyQuit);
            lobbyStart.getComponent<cro::UIInput>().setPrevIndex(LobbyQuit, LobbyInfoB);
            break;
        case LobbyRulesA:
        case LobbyRulesB:
            lobbyQuit.getComponent<cro::UIInput>().setNextIndex(LobbyStart, m_sharedData.hosting ? RulesPrevious : LobbyStart);
            lobbyQuit.getComponent<cro::UIInput>().setPrevIndex(LobbyStart, LobbyCourseA);

            lobbyStart.getComponent<cro::UIInput>().setNextIndex(LobbyQuit, m_sharedData.hosting ? RulesNext : LobbyQuit);
            lobbyStart.getComponent<cro::UIInput>().setPrevIndex(LobbyQuit, LobbyInfoA);
            break;
        case LobbyInfoA:
        case LobbyInfoB:
            lobbyQuit.getComponent<cro::UIInput>().setNextIndex(LobbyStart, LobbyStart);
            lobbyQuit.getComponent<cro::UIInput>().setPrevIndex(LobbyStart, LobbyCourseB);

            lobbyStart.getComponent<cro::UIInput>().setNextIndex(LobbyQuit, m_lobbyButtonContext.hasScoreCard ? InfoScorecard : LobbyQuit);
            lobbyStart.getComponent<cro::UIInput>().setPrevIndex(LobbyQuit, LobbyRulesB);
            break;
        }
    };

    //server info message
#ifndef USE_GNS
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<UIElement>().absolutePosition = { -160.f, -4.f };
    entity.getComponent<UIElement>().relativePosition = { 1.f, 1.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::ServerInfo;
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(m_sharedData.sharedResources->fonts.get(FontID::Info)).setString("Connected to");
    entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    entity.getComponent<cro::Text>().setFillColour(cro::Colour::White);
    entity.getComponent<cro::Text>().setShadowColour(cro::Colour(std::uint8_t(110), 179, 157));
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    menuTransform.addChild(entity.getComponent<cro::Transform>());
#endif


    //running scores
    struct ScoreInfo final
    {
        std::uint8_t clientID = 0;
        std::uint8_t playerID = 0;
        std::int8_t score = 0;
    };

    std::vector<ScoreInfo> scoreInfo;
    const auto& courseData = m_courseData[m_sharedData.courseIndex];
    cro::String str = "Welcome To Super Video Golf!";

    //only tally scores if we returned from a previous game
    //rather than quitting one, or completing the tutorial
    if (!m_sharedData.tutorial) //at this point (when the menu is built) this will be set if we're returning from a tutorial or quit menu
    {
        for (auto i = 0u; i < m_sharedData.connectionData.size(); ++i)
        {
            for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
            {

                if (!m_sharedData.connectionData[i].playerData[j].name.empty())
                {
                    auto& info = scoreInfo.emplace_back();
                    info.clientID = i;
                    info.playerID = j;
                    switch (m_sharedData.scoreType)
                    {
                    default:
                    case ScoreType::BattleRoyale:
                    case ScoreType::Stroke:
                    case ScoreType::ShortRound:
                    case ScoreType::MultiTarget:
                        info.score = m_sharedData.connectionData[i].playerData[j].parScore;
                        break;
                    case ScoreType::Match:
                        info.score = m_sharedData.connectionData[i].playerData[j].matchScore;
                        break;
                    case ScoreType::Skins:
                        info.score = m_sharedData.connectionData[i].playerData[j].skinScore;
                        break;
                    case ScoreType::Stableford:
                    case ScoreType::StablefordPro:
                        for (auto k = 0u; k < m_sharedData.connectionData[i].playerData[j].holeScores.size(); ++k)
                        {
                            auto diff = static_cast<std::int32_t>(m_sharedData.connectionData[i].playerData[j].holeScores[k]) - courseData.parVals[k];
                            auto stableScore = 2 - diff;

                            if (m_sharedData.scoreType == ScoreType::Stableford)
                            {
                                stableScore = std::max(0, stableScore);
                            }
                            else if (stableScore < 2)
                            {
                                stableScore -= 2;
                            }
                            info.score += stableScore;
                        }
                        break;
                    }
                }
            }
        }

        std::sort(scoreInfo.begin(), scoreInfo.end(),
            [&](const ScoreInfo& a, const ScoreInfo& b)
            {
                switch (m_sharedData.scoreType)
                {
                default:
                case ScoreType::Stroke:
                case ScoreType::BattleRoyale:
                case ScoreType::ShortRound:
                case ScoreType::MultiTarget:
                    return a.score < b.score;
                case ScoreType::Stableford:
                case ScoreType::StablefordPro:
                    return a.score > b.score;
                }
            });


        std::vector<cro::String> names;
        for (const auto& score : scoreInfo)
        {
            names.push_back(m_sharedData.connectionData[score.clientID].playerData[score.playerID].name);
            names.back() += ": (" + std::to_string(score.score) + ")";
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::BattleRoyale:
            case ScoreType::MultiTarget:
            case ScoreType::ShortRound:
            case ScoreType::Stroke:
                if (score.score < 0)
                {
                    names.back() += " Under Par";
                }
                else if (score.score > 0)
                {
                    names.back() += " Over Par";
                }
                break;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                names.back() += " Points";
                break;
            case ScoreType::Skins:
                names.back() += " Skins";
                break;
            case ScoreType::Match:
                names.back() += " Match Points";
                break;
            }
        }

        if (!names.empty())
        {
            str = "Last Round's Top Scorers: < " + names[0];
            for (auto i = 1u; i < names.size() && i < 4u; ++i)
            {
                str += " >< " + names[i];
            }
            str += " >";
        }
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

    refreshLobbyButtons();
}

void MenuState::updateLobbyData(const net::NetEvent& evt)
{
    ConnectionData cd;
    if (cd.deserialise(evt.packet))
    {
        //hum. this overwrite level values as they're maintained independently
        //would it be better to resend our level data when rx'ing this?
        auto lvl = m_sharedData.connectionData[cd.connectionID].level;
        m_sharedData.connectionData[cd.connectionID] = cd;
        m_sharedData.connectionData[cd.connectionID].level = lvl;

#ifdef USE_GNS
        //check the new player data for UGC
        for (auto i = 0u; i < cd.playerCount; ++i)
        {
            //hmmm using 0 as the return value seems suspicious
            //as it's not strictly an invalid index - however
            //we will fall back to this *anyway* if the remote content
            //is unavailable.
            if (indexFromBallID(cd.playerData[i].ballID) == 0)
            {
                //no local ball for this player
                Social::fetchRemoteContent(cd.peerID, cd.playerData[i].ballID, Social::UserContent::Ball);
            }

            if (indexFromHairID(cd.playerData[i].hairID) == 0)
            {
                //no local hair model
                Social::fetchRemoteContent(cd.peerID, cd.playerData[i].hairID, Social::UserContent::Hair);
            }

            if (indexFromAvatarID(cd.playerData[i].skinID) == 0)
            {
                //no local avatar model
                Social::fetchRemoteContent(cd.peerID, cd.playerData[i].skinID, Social::UserContent::Avatar);
            }
        }
#endif
    }

    if (m_sharedData.hosting)
    {
        std::int32_t playerCount = 0;
        for (const auto& c : m_sharedData.connectionData)
        {
            playerCount += c.playerCount;
        }

        m_matchMaking.setGamePlayerCount(playerCount);
    }

    //new players won't have other levels
    std::uint16_t xp = (Social::getLevel() << 8) | m_sharedData.clientConnection.connectionID;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::PlayerXP, xp, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

    updateLobbyAvatars();
}

void MenuState::updateLobbyList()
{
#ifdef USE_GNS
    for (auto& e : m_lobbyPager.pages)
    {
        m_uiScene.destroyEntity(e);
    }

    m_lobbyPager.pages.clear();
    m_lobbyPager.lobbyIDs.clear();

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);
    const auto& lobbyData = m_matchMaking.getLobbies();
    if (lobbyData.empty())
    {
        //no lobbies found :(
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(glm::vec3(LobbyTextRootPosition, 0.1f));
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setString(" No Games Found.");
        entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setCharacterSize(UITextSize);

        m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_lobbyPager.pages.push_back(entity);

        for (auto e : m_lobbyPager.slots)
        {
            e.getComponent<cro::UIInput>().enabled = false;
        }
    }
    else
    {
        auto pageCount = (lobbyData.size() / LobbyPager::ItemsPerPage) + 1;
        for (auto i = 0u; i < pageCount; ++i)
        {
            cro::String pageString;

            const auto startIndex = i * LobbyPager::ItemsPerPage;
            const auto endIndex = std::min(lobbyData.size(), startIndex + LobbyPager::ItemsPerPage);
            for (auto j = startIndex; j < endIndex; ++j)
            {
                std::stringstream ss;
                ss << " " << lobbyData[j].clientCount << "  " << std::setw(2) << std::setfill('0') << lobbyData[j].playerCount << " - ";
                pageString += ss.str();
                pageString += lobbyData[j].title + "\n";

                m_lobbyPager.lobbyIDs.push_back(lobbyData[j].ID);
            }

            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(LobbyTextRootPosition, 0.1f));
            entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(font).setString(pageString);
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);

            m_lobbyPager.rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            m_lobbyPager.pages.push_back(entity);
        }
        m_lobbyPager.pages[0].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        m_lobbyPager.currentPage = std::min(m_lobbyPager.currentPage, m_lobbyPager.pages.size() - 1);

        //enable slot highlights for current page
        auto start = m_lobbyPager.currentPage * LobbyPager::ItemsPerPage;
        auto end = start + LobbyPager::ItemsPerPage;

        for (auto i = start; i < end; ++i)
        {
            m_lobbyPager.slots[i % LobbyPager::ItemsPerPage].getComponent<cro::UIInput>().enabled = (i < m_lobbyPager.lobbyIDs.size());
        }
    }

    //hide or show buttons
    if (m_lobbyPager.pages.size() > 1)
    {
        m_lobbyPager.buttonLeft[0].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::UIInput>().enabled = true;

        m_lobbyPager.buttonRight[0].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::UIInput>().enabled = true;

        for (auto page : m_lobbyPager.pages)
        {
            page.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        m_lobbyPager.pages[m_lobbyPager.currentPage].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
    }
    else
    {
        m_lobbyPager.buttonLeft[0].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonLeft[1].getComponent<cro::UIInput>().enabled = false;

        m_lobbyPager.buttonRight[0].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::Transform>().setScale({ 0.f, 1.f });
        m_lobbyPager.buttonRight[1].getComponent<cro::UIInput>().enabled = false;
    }
#endif
}

void MenuState::quitLobby()
{
    m_sharedData.clientConnection.connected = false;
    m_sharedData.clientConnection.connectionID = ConstVal::NullValue;
    m_sharedData.clientConnection.ready = false;
    m_sharedData.clientConnection.netClient.disconnect();

    m_matchMaking.leaveGame();

    if (m_sharedData.hosting)
    {
        m_sharedData.serverInstance.stop();
        m_sharedData.hosting = false;
    }

    for (auto& cd : m_sharedData.connectionData)
    {
        cd.playerCount = 0;
    }
    

    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Main;
    m_menuEntities[m_currentMenu].getComponent<cro::Callback>().active = true;


    //reset the lobby tabs
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, 1.f });
    m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().setScale({ 0.f, 0.f });
    navigationUpdate(LobbyCourseA);
    m_uiScene.getSystem<cro::UISystem>()->selectByIndex(LobbyCourseA);

    //delete the course selection entities as they'll be re-created as needed
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::CourseSelect;
    cmd.action =
        [&](cro::Entity b, float)
    {
        m_uiScene.destroyEntity(b);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    Social::setStatus(Social::InfoID::Menu, { "Main Menu" });
    Social::setGroup(0);
}

void MenuState::addCourseSelectButtons()
{
    //selection box to open player management
    static constexpr float Width = 216.f;
    static constexpr float Height = 228.f;
    static constexpr float Thickness = 1.f;

    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 5.f, 7.f, 0.5f });
    entity.getComponent<cro::Transform>().setOrigin({ Width / 2.f, Height / 2.f });
    entity.getComponent<cro::Transform>().move({ Width / 2.f, Height / 2.f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::CourseSelect;
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            //left
            cro::Vertex2D(glm::vec2(0.f,Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(0.f, Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Thickness), TextHighlightColour),

            cro::Vertex2D(glm::vec2(Thickness,Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(0.f, Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Thickness), TextHighlightColour),

            //right
            cro::Vertex2D(glm::vec2(Width - Thickness,Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width - Thickness, Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width, Thickness), TextHighlightColour),

            cro::Vertex2D(glm::vec2(Width,Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width - Thickness, Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width, Thickness), TextHighlightColour),


            //bottom
            cro::Vertex2D(glm::vec2(Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Thickness, 0.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width - Thickness, Thickness), TextHighlightColour),

            cro::Vertex2D(glm::vec2(Width - Thickness, Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Thickness, 0.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width - Thickness, 0.f), TextHighlightColour),

            //top
            cro::Vertex2D(glm::vec2(Thickness, Height), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Thickness, Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width - Thickness, Height), TextHighlightColour),

            cro::Vertex2D(glm::vec2(Width - Thickness, Height), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Thickness, Height - Thickness), TextHighlightColour),
            cro::Vertex2D(glm::vec2(Width - Thickness, Height - Thickness), TextHighlightColour),
        });
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    for (auto& v : entity.getComponent<cro::Drawable2D>().getVertexData())
    {
        v.colour = cro::Colour::Transparent;
    }

    entity.addComponent<cro::UIInput>().area = { 0.f, 0.f, Width, Height };
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerManagement);
    entity.getComponent<cro::UIInput>().setNextIndex(CoursePrev, LobbyQuit);
    entity.getComponent<cro::UIInput>().setPrevIndex(CourseNightTime, LobbyQuit);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectPM;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectPM;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.activatePM;

    //entity.addComponent<cro::Callback>().function = MenuTextCallback();

    m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());







    const auto gameRuleEnable = [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y +
            m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().getScale().y == 0;
    };

    //choose scoring type
    auto buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 364.f-91.f, 206.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    auto bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(RulesPrevious);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(RulesNext, RulesGimmePrev);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(RulesNext, LobbyQuit);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.prevRules;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = gameRuleEnable;

    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 364.f+92.f, 206.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(RulesNext);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(RulesPrevious, RulesGimmeNext);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(RulesPrevious, LobbyStart);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.nextRules;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = gameRuleEnable;
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //choose gimme radius
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 364.f - 91.f, 99.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(RulesGimmePrev);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(RulesGimmeNext, Social::getClubLevel() ? RulesClubset : LobbyCourseA);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(RulesGimmeNext, RulesPrevious);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.prevRadius;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = gameRuleEnable;
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 364.f + 92.f, 99.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(RulesGimmeNext);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(RulesGimmePrev, Social::getClubLevel() ? RulesClubset : LobbyInfoA);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(RulesGimmePrev, RulesNext);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.nextRadius;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = gameRuleEnable;
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });

    m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    const auto courseButtonEnable =
        [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y != 0;
    };

    //hole count buttons.
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 46.f, 41.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseHolePrev);
#ifdef USE_GNS
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseHoleNext, CourseInviteFriend);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CourseFriendsOnly, CoursePrev);
#else
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CourseHoleNext, CourseNext);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseHoleNext, LobbyRulesA);
#endif
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.prevHoleCount;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = courseButtonEnable;
        
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    buttonEnt.addComponent<cro::SpriteAnimation>();
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 126.f, 41.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseHoleNext);
#ifdef USE_GNS
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseFriendsOnly, CourseInviteFriend);
#else
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseHolePrev, LobbyRulesA);
#endif
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CourseHolePrev, CourseNext);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.nextHoleCount;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = courseButtonEnable;
    buttonEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());

    const bool hasUserCourses = m_courseData.size() > m_courseIndices[Range::Official].count;

    //toggle course reverse
    auto checkboxEnt = m_uiScene.createEntity();
    checkboxEnt.addComponent<cro::Transform>();
    checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    checkboxEnt.addComponent<cro::Drawable2D>();
    checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
    checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    checkboxEnt.addComponent<UIElement>().absolutePosition = { 175.f, 155.f };
    checkboxEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 78.f; //wild stab at the width of the text (it's not here to measure...)
    checkboxEnt.addComponent<cro::UIInput>().area = bounds;
    checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    checkboxEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseReverse);
    checkboxEnt.getComponent<cro::UIInput>().setNextIndex(CoursePrev, hasUserCourses ? CourseUser : CourseCPUSkip);
    checkboxEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, LobbyStart);
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.toggleReverseCourse;
    checkboxEnt.addComponent<cro::Callback>().active = true;
    checkboxEnt.getComponent<cro::Callback>().function = courseButtonEnable;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


    //toggle CPU skip
    checkboxEnt = m_uiScene.createEntity();
    checkboxEnt.addComponent<cro::Transform>();
    checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    checkboxEnt.addComponent<cro::Drawable2D>();
    checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
    checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    checkboxEnt.addComponent<UIElement>().absolutePosition = { 175.f, 129.f };
    checkboxEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 78.f;
    checkboxEnt.addComponent<cro::UIInput>().area = bounds;
    checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    checkboxEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseCPUSkip);
    checkboxEnt.getComponent<cro::UIInput>().setNextIndex(CoursePrev, CourseClubLimit);
    checkboxEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, hasUserCourses ? CourseUser : CourseReverse);
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.toggleFastCPU;
    checkboxEnt.addComponent<cro::Callback>().active = true;
    checkboxEnt.getComponent<cro::Callback>().function = courseButtonEnable;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


    //toggle club set limit
    checkboxEnt = m_uiScene.createEntity();
    checkboxEnt.addComponent<cro::Transform>();
    checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    checkboxEnt.addComponent<cro::Drawable2D>();
    checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
    checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    checkboxEnt.addComponent<UIElement>().absolutePosition = { 175.f, 116.f };
    checkboxEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 78.f;
    checkboxEnt.addComponent<cro::UIInput>().area = bounds;
    checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    checkboxEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseClubLimit);
    checkboxEnt.getComponent<cro::UIInput>().setNextIndex(CoursePrev, CourseNightTime);
    checkboxEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, CourseCPUSkip);
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.toggleClubLimit;
    checkboxEnt.addComponent<cro::Callback>().active = true;
    checkboxEnt.getComponent<cro::Callback>().function = courseButtonEnable;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


    //toggle night time
    checkboxEnt = m_uiScene.createEntity();
    checkboxEnt.addComponent<cro::Transform>();
    checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    checkboxEnt.addComponent<cro::Drawable2D>();
    checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
    checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    checkboxEnt.addComponent<UIElement>().absolutePosition = { 175.f, 103.f };
    checkboxEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.width += 60.f;
    checkboxEnt.addComponent<cro::UIInput>().area = bounds;
    checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    checkboxEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseNightTime);
    checkboxEnt.getComponent<cro::UIInput>().setNextIndex(PlayerManagement, Social::isAvailable() ? CourseFriendsOnly : CourseWeather);
    checkboxEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, CourseClubLimit);
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.toggleNightTime;
    checkboxEnt.addComponent<cro::Callback>().active = true;
    checkboxEnt.getComponent<cro::Callback>().function = courseButtonEnable;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    if (Social::isAvailable())
    {
        //friends only lobby
        auto checkboxEnt = m_uiScene.createEntity();
        checkboxEnt.addComponent<cro::Transform>();
        checkboxEnt.addComponent<cro::Drawable2D>();
        checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
        checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        checkboxEnt.addComponent<UIElement>().absolutePosition = { 176.f, 91.f };
        checkboxEnt.getComponent<UIElement>().depth = 0.01f;

        bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
        checkboxEnt.addComponent<cro::Callback>().active = true;
        checkboxEnt.getComponent<cro::Callback>().function =
            [&, bounds](cro::Entity e, float)
        {
            auto b = bounds;
            if (m_matchMaking.getFriendsOnly())
            {
                b.bottom -= bounds.height;
            }
            e.getComponent<cro::Sprite>().setTextureRect(b);
        };

        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


        checkboxEnt = m_uiScene.createEntity();
        checkboxEnt.addComponent<cro::Transform>();
        checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        checkboxEnt.addComponent<cro::Drawable2D>();
        checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
        checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        checkboxEnt.addComponent<UIElement>().absolutePosition = { 175.f, 90.f };
        checkboxEnt.getComponent<UIElement>().depth = 0.01f;
        bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
        checkboxEnt.addComponent<cro::UIInput>().area = bounds;
        checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        checkboxEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseFriendsOnly);
        checkboxEnt.getComponent<cro::UIInput>().setNextIndex(CoursePrev, CourseWeather);
        checkboxEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, CourseNightTime);
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.toggleFriendsOnly;
        checkboxEnt.addComponent<cro::Callback>().active = true;
        checkboxEnt.getComponent<cro::Callback>().function = courseButtonEnable;
        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());

        auto labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString("Friends Only");
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { 187.f, 98.f };
        labelEnt.getComponent<UIElement>().depth = 0.01f;
        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
        checkboxEnt.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(labelEnt).width;

        labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::InviteHighlight];
        labelEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { 39.f, 15.f };
        labelEnt.getComponent<UIElement>().depth = 0.02f;
        bounds = m_sprites[SpriteID::InviteHighlight].getTextureBounds();
        labelEnt.addComponent<cro::UIInput>().area = bounds;
        labelEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        labelEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseInviteFriend);
        labelEnt.getComponent<cro::UIInput>().setNextIndex(LobbyRulesA, LobbyRulesA);
        labelEnt.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoB, CourseHolePrev);
        labelEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
        labelEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
        labelEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.inviteFriends;
        labelEnt.addComponent<cro::Callback>().active = true;
        labelEnt.getComponent<cro::Callback>().function = courseButtonEnable;
        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());


        labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::Envelope];
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { 41.f, 17.f };
        labelEnt.getComponent<UIElement>().depth = 0.01f;
        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    }


    //change weather
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::WeatherHighlight];
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 185.f, 72.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = m_sprites[SpriteID::WeatherHighlight].getTextureBounds();
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseWeather);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CoursePrev, LobbyInfoB);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, Social::isAvailable() ? CourseFriendsOnly : CourseNightTime);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.setWeather;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = courseButtonEnable;

    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //choose course
    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyLeft];
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 6.f, 101.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.bottom -= 6.f;
    bounds.height += 12.f;
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CoursePrev);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseNext, CourseHolePrev);
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(PlayerManagement, LobbyQuit);
//#ifdef USE_GNS
//#else
//#endif
    //buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CourseCPUSkip, LobbyQuit);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.prevCourse;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = courseButtonEnable;
    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());



    buttonEnt = m_uiScene.createEntity();
    buttonEnt.addComponent<cro::Transform>();
    buttonEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    buttonEnt.addComponent<cro::Drawable2D>();
    buttonEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyRight];
    buttonEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    buttonEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
    buttonEnt.addComponent<UIElement>().absolutePosition = { 159.f, 101.f };
    buttonEnt.getComponent<UIElement>().depth = 0.01f;
    bounds = buttonEnt.getComponent<cro::Sprite>().getTextureBounds();
    bounds.bottom -= 6.f;
    bounds.height += 12.f;
    buttonEnt.addComponent<cro::UIInput>().area = bounds;
    buttonEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    buttonEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseNext);
    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseNightTime, CourseHoleNext);
//#ifdef USE_GNS
//#else
//    buttonEnt.getComponent<cro::UIInput>().setNextIndex(CourseCPUSkip, CourseHoleNext);
//#endif
    buttonEnt.getComponent<cro::UIInput>().setPrevIndex(CoursePrev, LobbyRulesA);
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    buttonEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.nextCourse;
    buttonEnt.addComponent<cro::Callback>().active = true;
    buttonEnt.getComponent<cro::Callback>().function = courseButtonEnable;

    m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(buttonEnt.getComponent<cro::Transform>());


    //checkbox to show user created courses
    if (hasUserCourses)
    {
        checkboxEnt = m_uiScene.createEntity();
        checkboxEnt.addComponent<cro::Transform>();
        checkboxEnt.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        checkboxEnt.addComponent<cro::Drawable2D>();
        checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckboxHighlight];
        checkboxEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        checkboxEnt.addComponent<UIElement>().absolutePosition = { 175.f, 142.f };
        checkboxEnt.getComponent<UIElement>().depth = 0.01f;
        bounds = checkboxEnt.getComponent<cro::Sprite>().getTextureBounds();
        bounds.width += 78.f; //wild stab at the width of the text (it's not here to measure...)
        checkboxEnt.addComponent<cro::UIInput>().area = bounds;
        checkboxEnt.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
        checkboxEnt.getComponent<cro::UIInput>().setSelectionIndex(CourseUser);
        checkboxEnt.getComponent<cro::UIInput>().setNextIndex(CoursePrev, CourseCPUSkip);
        checkboxEnt.getComponent<cro::UIInput>().setPrevIndex(CourseNext, CourseReverse);
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
        checkboxEnt.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = m_courseSelectCallbacks.prevHoleType;
        checkboxEnt.addComponent<cro::Callback>().active = true;
        checkboxEnt.getComponent<cro::Callback>().function = courseButtonEnable;
        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());
        
        
        checkboxEnt = m_uiScene.createEntity();
        checkboxEnt.addComponent<cro::Transform>();
        checkboxEnt.addComponent<cro::Drawable2D>();
        checkboxEnt.addComponent<cro::Sprite>() = m_sprites[SpriteID::LobbyCheckbox];
        checkboxEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        checkboxEnt.addComponent<UIElement>().absolutePosition = { 176.f, 143.f };
        checkboxEnt.getComponent<UIElement>().depth = 0.01f;

        bounds = m_sprites[SpriteID::LobbyCheckbox].getTextureRect();
        checkboxEnt.addComponent<cro::Callback>().active = true;
        checkboxEnt.getComponent<cro::Callback>().function =
            [&, bounds](cro::Entity e, float)
        {
            auto b = bounds;
            if (m_currentRange != Range::Official)
            {
                b.bottom -= bounds.height;
            }
            e.getComponent<cro::Sprite>().setTextureRect(b);
        };

        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(checkboxEnt.getComponent<cro::Transform>());


        auto labelEnt = m_uiScene.createEntity();
        labelEnt.addComponent<cro::Transform>();
        labelEnt.addComponent<cro::Drawable2D>();
        labelEnt.addComponent<cro::Text>(font).setString("User Courses");
        labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
        labelEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement | CommandID::Menu::CourseSelect;
        labelEnt.addComponent<UIElement>().absolutePosition = { 187.f, 150.f };
        labelEnt.getComponent<UIElement>().depth = 0.1f;
        m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    }
}

void MenuState::prevHoleCount()
{
    m_sharedData.holeCount = (m_sharedData.holeCount + 2) % 3;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::HoleCount, m_sharedData.holeCount, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void MenuState::nextHoleCount()
{
    m_sharedData.holeCount = (m_sharedData.holeCount + 1) % 3;
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::HoleCount, m_sharedData.holeCount, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;

    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void MenuState::prevCourse()
{
    auto idx = m_sharedData.courseIndex - m_courseIndices[m_currentRange].start;
    idx = (idx + (m_courseIndices[m_currentRange].count - 1)) % m_courseIndices[m_currentRange].count;

    m_sharedData.courseIndex = m_courseIndices[m_currentRange].start + idx;

    m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
    auto data = serialiseString(m_sharedData.mapDirectory);
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
}

void MenuState::nextCourse()
{
    auto idx = m_sharedData.courseIndex - m_courseIndices[m_currentRange].start;
    idx = (idx + 1) % m_courseIndices[m_currentRange].count;

    m_sharedData.courseIndex = m_courseIndices[m_currentRange].start + idx;

    m_sharedData.mapDirectory = m_courseData[m_sharedData.courseIndex].directory;
    auto data = serialiseString(m_sharedData.mapDirectory);
    m_sharedData.clientConnection.netClient.sendPacket(PacketID::MapInfo, data.data(), data.size(), net::NetFlag::Reliable, ConstVal::NetChannelStrings);

    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
}

void MenuState::refreshUI()
{
    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::UIElement;
    cmd.action =
        [&](cro::Entity e, float)
    {
        glm::vec2 size(GolfGame::getActiveTarget()->getSize());

        const auto& element = e.getComponent<UIElement>();
        auto pos = element.absolutePosition;
        pos += element.relativePosition * size / m_viewScale;

        pos.x = std::floor(pos.x);
        pos.y = std::floor(pos.y);

        e.getComponent<cro::Transform>().setPosition(glm::vec3(pos, element.depth));

        if (element.resizeCallback)
        {
            element.resizeCallback(e);
        }
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //and resizes banners horizontally
    cmd.targetFlags = CommandID::Menu::UIBanner;
    cmd.action =
        [](cro::Entity e, float)
    {
        e.getComponent<cro::Callback>().active = true;
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

    //makes sure to refresh for at least one frame if cam currently static
    m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
}

void MenuState::updateCourseRuleString(bool updateScoreboard)
{
    const auto data = std::find_if(m_courseData.cbegin(), m_courseData.cend(),
        [&](const CourseData& cd)
        {
            return cd.directory == m_sharedData.mapDirectory;
        });

    cro::Command cmd;
    cmd.targetFlags = CommandID::Menu::CourseRules;
    cmd.action = [&, data](cro::Entity e, float)
    {
        std::string str;
        if (data != m_courseData.end()
            && !data->isUser)
        {
            str = "Course " + std::to_string(data->courseNumber) + "\n";
        }
        else
        {
            str = "User Course\n";
        }

        str += ScoreTypes[m_sharedData.scoreType];
        str += "\n" + GimmeString[m_sharedData.gimmeRadius];

        if (data != m_courseData.end())
        {
            str += "\n" + data->holeCount[m_sharedData.holeCount];
        }
       
        e.getComponent<cro::Text>().setString(str);
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);

//#ifdef USE_GNS
    //update ticker
    if (!m_sharedData.tutorial && //data will be courseData.cend()
        m_lobbyWindowEntities[LobbyEntityID::CourseTicker].isValid())
    {
        if (!data->isUser)
        {
            if (updateScoreboard)
            {
                auto scoreStr = Social::getTopFive(m_sharedData.mapDirectory, m_sharedData.holeCount);
                m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Text>().setString(scoreStr);
                m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Transform>().setPosition(glm::vec2(200.f, 0.f));
            }
        }
        else
        {
            m_lobbyWindowEntities[LobbyEntityID::CourseTicker].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
    }
//#endif
}

void MenuState::updateUnlockedItems()
{
    //current day streak
    auto streak = Social::updateStreak();
    CRO_ASSERT(streak < 8, "");
    switch (streak)
    {
    default:
        m_sharedData.unlockedItems.push_back(ul::UnlockID::Streak01 + (streak - 1));
        break;
    case 0: //do nothing
        break;
    }

    if (streak != 0)
    {
        Social::awardXP(StreakXP[streak-1]);
    }



    //clubs
    auto clubFlags = Social::getUnlockStatus(Social::UnlockType::Club);
    if (clubFlags == 0)
    {
        clubFlags = ClubID::DefaultSet;
    }
    auto level = Social::getLevel();
    auto clubCount = std::min(ClubID::LockedSet.size(), static_cast<std::size_t>(level / 5));
    for (auto i = 0u; i < clubCount; ++i)
    {
        auto clubID = ClubID::LockedSet[i];
        if ((clubFlags & ClubID::Flags[clubID]) == 0)
        {
            clubFlags |= ClubID::Flags[clubID];
            m_sharedData.unlockedItems.push_back(ul::UnlockID::FiveWood + i);
        }
    }

    m_sharedData.inputBinding.clubset = clubFlags;
    Social::setUnlockStatus(Social::UnlockType::Club, clubFlags);

    if (m_sharedData.inputBinding.clubset == ClubID::FullSet)
    {
        Achievements::awardAchievement(AchievementStrings[AchievementID::FullHouse]);
    }


    //ball flags (balls are unlocked every 10 levels)
    auto ballFlags = Social::getUnlockStatus(Social::UnlockType::Ball);
    auto ballCount = std::min(5, level / 10);
    for (auto i = 0; i < ballCount; ++i)
    {
        auto flag = (1 << i);
        if ((ballFlags & flag) == 0)
        {
            ballFlags |= flag;
            m_sharedData.unlockedItems.push_back(ul::UnlockID::BronzeBall + i);
        }
    }

    //plus a super special one a level 100
    if ((level / 100) == 1)
    {
        auto flag = (1 << 6);
        if ((ballFlags & flag) == 0)
        {
            ballFlags |= flag;
            m_sharedData.unlockedItems.push_back(ul::UnlockID::AmbassadorBall);
        }
    }

    Social::setUnlockStatus(Social::UnlockType::Ball, ballFlags);


    //level up
    if (level > 0)
    {
        //levels are same interval as balls + 1st level
        auto levelCount = ballCount + 1; //(note this skips level 100 as it's not sequential)
        auto levelFlags = Social::getUnlockStatus(Social::UnlockType::Level);

        for (auto i = 0; i < levelCount; ++i)
        {
            auto flag = (1 << i);
            if ((levelFlags & flag) == 0)
            {
                levelFlags |= flag;
                m_sharedData.unlockedItems.push_back(ul::UnlockID::Level1 + i);
            }
        }
        //centenery is handled separately
        if ((level / 100) == 1)
        {
            auto flag = (1 << levelCount);
            if ((levelFlags & flag) == 0)
            {
                levelFlags |= flag;
                m_sharedData.unlockedItems.push_back(ul::UnlockID::Level100);
            }
        }
        Social::setUnlockStatus(Social::UnlockType::Level, levelFlags);
    }


    //generic unlocks from achievements etc
    auto genericFlags = Social::getUnlockStatus(Social::UnlockType::Generic);
    constexpr std::int32_t genericBase = ul::UnlockID::RangeExtend01;

    if (level > 14)
    {
        //club range is extended at level 15 and 30
        auto flag = (1 << 0);
        if ((genericFlags & flag) == 0)
        {
            genericFlags |= flag;
            m_sharedData.unlockedItems.push_back(ul::UnlockID::RangeExtend01);
        }
        else if (level > 29)
        {
            flag = (1 << (ul::UnlockID::RangeExtend02 - genericBase));
            if ((genericFlags & flag) == 0)
            {
                genericFlags |= flag;
                m_sharedData.unlockedItems.push_back(ul::UnlockID::RangeExtend02);
            }
        }
    }

    auto flag = (1 << (ul::UnlockID::Clubhouse - genericBase));
    if ((genericFlags & flag) == 0 &&
        Achievements::getAchievement(AchievementStrings[AchievementID::JoinTheClub])->achieved)
    {
        genericFlags |= flag;
        m_sharedData.unlockedItems.push_back(ul::UnlockID::Clubhouse);
    }

    flag = (1 << (ul::UnlockID::CourseEditor - genericBase));
    if ((genericFlags & flag) == 0 &&
        Achievements::getAchievement(AchievementStrings[AchievementID::GrandTour])->achieved)
    {
        genericFlags |= flag;
        m_sharedData.unlockedItems.push_back(ul::UnlockID::CourseEditor);
    }

    Social::setUnlockStatus(Social::UnlockType::Generic, genericFlags);
}

void MenuState::createPreviousScoreCard()
{
    static constexpr float OffscreenPos = -300.f;

    //background image
    auto& tex = m_resources.textures.get("assets/golf/images/lobby_scoreboard.png");
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(tex);
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    bounds = m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setPosition({ bounds.width / 2.f, OffscreenPos, 1.95f });


    const float targetPos = bounds.height / 2.f;
    entity.addComponent<cro::Callback>().setUserData<ScorecardCallbackData>();
    entity.getComponent<cro::Callback>().function =
        [&, targetPos](cro::Entity e, float dt)
    {
        const float Speed = dt * 6.f;

        auto pos = e.getComponent<cro::Transform>().getPosition();
        auto& [dir, dest] = e.getComponent<cro::Callback>().getUserData<ScorecardCallbackData>();
        if (dir == 0)
        {
            //moving out
            pos.y = std::max(OffscreenPos, pos.y + ((OffscreenPos - pos.y) * Speed) - 0.1f);

            if (pos.y == OffscreenPos)
            {
                //only set the active menu if the dest is Lobby
                if (dest == MenuID::Lobby)
                {
                    m_currentMenu = MenuID::Lobby;
                    m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Lobby);
                    dest = MenuID::Dummy;
                }
                e.getComponent<cro::Callback>().active = false;
            }
        }
        else
        {
            //move in
            pos.y = std::min(targetPos, pos.y + ((targetPos - pos.y) * Speed) + 0.1f);
            
            if (pos.y == targetPos)
            {
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Scorecard);
                dest = MenuID::Lobby;
                e.getComponent<cro::Callback>().active = false;
                m_currentMenu = MenuID::Lobby;// needs to be set to this to correctly resize the window TODO find out where resize is handled and include correct menu IDs in the condition...
            }
        }
        m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
        e.getComponent<cro::Transform>().setPosition(pos);


        //send command to lobby title to hide out the way
        float scale = 1.f - std::clamp((pos.y - OffscreenPos) / (targetPos - OffscreenPos), 0.f, 1.f);
        cro::Command cmd;
        cmd.targetFlags = CommandID::Menu::TitleText;
        cmd.action = [scale](cro::Entity f, float)
        {
            f.getComponent<cro::Transform>().setScale(glm::vec2(scale));
        };
        m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
    };

    m_lobbyWindowEntities[LobbyEntityID::Background].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_lobbyWindowEntities[LobbyEntityID::Scorecard] = entity;


    //background fade
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition(m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().getOrigin());
    entity.getComponent<cro::Transform>().move({ 0.f, 0.f, -0.06f });
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-0.5f, 0.5f), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(-0.5f), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(0.5f), cro::Colour::Black),
            cro::Vertex2D(glm::vec2(0.5f, -0.5f), cro::Colour::Black),
        }
    );
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function =
        [&, targetPos](cro::Entity e, float)
    {
        auto pos = m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().getPosition().y;
        pos -= OffscreenPos;
        pos /= (targetPos - OffscreenPos);

        e.getComponent<cro::Transform>().setScale(glm::vec2(cro::App::getWindow().getSize()) * pos * 1.1f);
        pos = cro::Util::Easing::easeInExpo(pos);
        for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
        {
            v.colour.setAlpha(BackgroundAlpha * pos);
        }
    };
    m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());



    //fills with dummy data for testing  - must remove this!
    //for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    //{
    //    for (auto j = 0u; j < ConstVal::MaxPlayers; ++j)
    //    {
    //        m_sharedData.connectionData[i].playerData[j].name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
    //        m_sharedData.connectionData[i].playerData[j].holeScores.resize(18);
    //        for (auto& score : m_sharedData.connectionData[i].playerData[j].holeScores)
    //        {
    //            score = cro::Util::Random::value(1, 5);
    //        }
    //    }
    //}





    //this should still be set from the last round (it'll be modified if the
    //host changes settings but we'll already have built this by now)
    auto courseData = m_courseData[m_sharedData.courseIndex];
    if (m_sharedData.reverseCourse)
    {
        std::reverse(courseData.parVals.begin(), courseData.parVals.end());
    }

    auto holeCount = static_cast<std::int32_t>(courseData.parVals.size());
    auto frontCount = std::max(holeCount / 2, 1);
    auto backCount = std::min(holeCount - (holeCount / 2), 9);    

    if (m_sharedData.scoreType == ScoreType::ShortRound
        && !m_sharedData.showCustomCourses)
    {
        switch (m_sharedData.holeCount)
        {
        default:
        case 0:
            backCount = 3;
            break;
        case 1:
            frontCount = 6;
            break;
        case 2:
            backCount = 6;
            break;
        }
    }

    if (m_sharedData.scoreType == ScoreType::BattleRoyale)
    {
        //use the number of holes actually played
        holeCount = m_sharedData.holesPlayed + 1;
        frontCount = std::min(9, holeCount);
        backCount = std::max(0, holeCount - frontCount);
    }

    struct Entry final
    {
        std::uint32_t client = 0; //use this to index directly into name data, save making a copy
        std::uint32_t player = 0;
        std::array<std::int32_t, 18> holeScores = {};
        std::int32_t total = 0;
        std::int32_t totalFront = 0;
        std::int32_t totalBack = 0;
        std::int32_t roundScore = 0; //rule type, ie par diff or match points
    };

    std::vector<Entry> scoreEntries;
    for (auto i = 0u; i < ConstVal::MaxClients; ++i)
    {
        if (m_sharedData.connectionData[i].playerCount != 0)
        {
            for (auto j = 0u; j < m_sharedData.connectionData[i].playerCount; ++j)
            {
                auto& entry = scoreEntries.emplace_back();
                entry.client = i;
                entry.player = j;

                switch (m_sharedData.scoreType)
                {
                default:
                case ScoreType::BattleRoyale:
                case ScoreType::MultiTarget:
                case ScoreType::Stroke:
                case ScoreType::ShortRound:
                    entry.roundScore = m_sharedData.connectionData[i].playerData[j].parScore;
                    break;
                case ScoreType::Match:
                    entry.roundScore = m_sharedData.connectionData[i].playerData[j].matchScore;
                    break;
                case ScoreType::Skins:
                    entry.roundScore = m_sharedData.connectionData[i].playerData[j].skinScore;
                    break;
                case ScoreType::StablefordPro:
                case ScoreType::Stableford:
                    //do nothing, we re-calc below
                    break;
                }

                //These should be fine from the last round as they aren't
                //cleared and resized until the beginning of the next one
                //also means there are only as many scores as there are holes.
                auto k = 0;
                for (auto score : m_sharedData.connectionData[i].playerData[j].holeScores)
                {
                    if (m_sharedData.scoreType == ScoreType::Stableford
                        || m_sharedData.scoreType == ScoreType::StablefordPro)
                    {
                        auto diff = static_cast<std::int32_t>(score) - courseData.parVals[k];
                        auto stableScore = 2 - diff;

                        if (m_sharedData.scoreType == ScoreType::Stableford)
                        {
                            stableScore = std::max(0, stableScore);
                        }
                        else if (stableScore < 2)
                        {
                            stableScore -= 2;
                        }

                        entry.holeScores[k] = stableScore;
                        entry.total += stableScore;
                        if (k < 9)
                        {
                            entry.totalFront += stableScore;
                        }
                        else
                        {
                            entry.totalBack += stableScore;
                        }
                    }
                    else
                    {
                        entry.holeScores[k] = score;
                        entry.total += score;
                        if (k < 9)
                        {
                            entry.totalFront += score;
                        }
                        else
                        {
                            entry.totalBack += score;
                        }
                    }
                    k++;
                }
            }
        }
    }

    std::sort(scoreEntries.begin(), scoreEntries.end(), [&](const Entry& a, const Entry& b)
        {
            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::BattleRoyale:
            case ScoreType::Stroke:
            case ScoreType::ShortRound:
            case ScoreType::MultiTarget:
                return a.total < b.total;
            case ScoreType::Match:
            case ScoreType::Skins:
                return a.roundScore > b.roundScore;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                return a.total > b.total;
            }
        });


    static constexpr glm::vec3 RootPosition(10.f, 269.f, 0.1f);
    auto rootNode = m_uiScene.createEntity(); //use this for paging
    rootNode.addComponent<cro::Transform>().setPosition(RootPosition);
    m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().addChild(rootNode.getComponent<cro::Transform>());

    std::vector<cro::Entity> textEntities;

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //name column
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    textEntities.push_back(entity);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //TODO we could do all the updates in one iter over the Entries
    //but this is all in the loading screen so... meh

    bool page2 = m_sharedData.scoreType == ScoreType::BattleRoyale ?
        backCount != 0
        : m_sharedData.holeCount == 0;

    cro::String str("HOLE\n");
    switch (m_sharedData.scoreType)
    {
    default:
        str += "PAR";
        break;
    case ScoreType::Stableford:
    case ScoreType::StablefordPro:
        str += " ";
        break;
    }

    for (const auto& entry : scoreEntries)
    {
        str += "\n " + m_sharedData.connectionData[entry.client].playerData[entry.player].name.substr(0, ConstVal::MaxStringChars);
    }

    if (page2)
    {
        //pad remaining rows
        for (auto i = 0u; i < 16u - scoreEntries.size(); ++i)
        {
            str += "\n";
        }
        
        str += "\n\nHOLE\n";
        switch (m_sharedData.scoreType)
        {
        default:
            str += "PAR";
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
            str += " ";
            break;
        }

        for (const auto& entry : scoreEntries)
        {
            str += "\n " + m_sharedData.connectionData[entry.client].playerData[entry.player].name.substr(0, ConstVal::MaxStringChars);
        }
    }
    entity.getComponent<cro::Text>().setString(str);

    if (page2)
    {
        bounds = cro::Text::getLocalBounds(entity);
        bounds.height /= 2.f;
        bounds.bottom += bounds.height;
        entity.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
    }

    //hole columns
    cro::String redStr;

    std::int32_t parOffset = 0;
    std::int32_t parTotal = 0;
    std::int32_t parTotalFront = 0;
    std::int32_t parTotalBack = 0;

    switch (m_sharedData.holeCount)
    {
    default: break;
    case 1:
        holeCount = frontCount;
        
        if (m_sharedData.scoreType == ScoreType::BattleRoyale
            && m_sharedData.reverseCourse)
        {
            parOffset = 9;
        }

        break;
    case 2:
        //back 9
        if (m_sharedData.scoreType == ScoreType::BattleRoyale)
        {
            parOffset = m_sharedData.reverseCourse ? 0 : 9;
        }
        else
        {
            parOffset = frontCount;
            holeCount = backCount;
        }
        break;
    }

    float colStart = 210.f;
    const float colWidth = 20.f;
    for (auto i = 0; i < std::min(9, holeCount); ++i)
    {
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ colStart, 0.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
        entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
        rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        textEntities.push_back(entity);

        auto redEnt = m_uiScene.createEntity();
        redEnt.addComponent<cro::Transform>().setPosition({ colStart, -7.f }); //not sure where the 7 comes from :S
        redEnt.addComponent<cro::Drawable2D>();
        redEnt.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
        redEnt.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
        redEnt.getComponent<cro::Text>().setFillColour(TextHighlightColour);
        redEnt.getComponent<cro::Text>().setAlignment(cro::Text::Alignment::Right);
        rootNode.getComponent<cro::Transform>().addChild(redEnt.getComponent<cro::Transform>());
        textEntities.push_back(redEnt);

        str = std::to_string(i + 1);
        str += "\n";

        switch (m_sharedData.scoreType)
        {
        default:
            str += std::to_string(courseData.parVals[parOffset + i]);
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
            str += " ";
            break;
        }

        parTotal += courseData.parVals[parOffset + i];
        parTotalFront += courseData.parVals[parOffset + i];

        redStr = "\n";

        for (const auto& entry : scoreEntries)
        {
            auto score = entry.holeScores[i];

            switch (m_sharedData.scoreType)
            {
            default:
                if (score > courseData.parVals[parOffset + i])
                {
                    str += "\n";
                    redStr += "\n" + std::to_string(score);
                }
                else
                {
                    str += "\n" + std::to_string(score);
                    redStr += "\n";
                }
                break;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                if (score > 0)
                {
                    str += "\n" + std::to_string(score);
                    redStr += "\n";
                }
                else
                {
                    str += "\n";
                    redStr += "\n" + std::to_string(score);
                }
                break;
            }
        }

        if (page2)
        {
            //pad remaining rows
            for (auto j = 0u; j < 16u - scoreEntries.size(); ++j)
            {
                str += "\n";
                redStr += "\n";
            }

            auto holeTotal = m_sharedData.scoreType == ScoreType::ShortRound ? 12 : holeCount;

            if (i < (holeTotal - 9))
            {
                parTotal += courseData.parVals[parOffset + i + 9];
                parTotalBack += courseData.parVals[parOffset + i + 9];

                str += "\n\n" + std::to_string(i + 10) + "\n";
                redStr += "\n\n\n";

                switch (m_sharedData.scoreType)
                {
                default:
                    str += std::to_string(courseData.parVals[parOffset + i + 9]);
                    break;
                case ScoreType::Stableford:
                case ScoreType::StablefordPro:
                    str += " ";
                    break;
                }


                for (const auto& entry : scoreEntries)
                {
                    auto score = entry.holeScores[i + 9];

                    switch (m_sharedData.scoreType)
                    {
                    default:
                        if (score > courseData.parVals[parOffset + i + 9])
                        {
                            str += "\n";
                            redStr += "\n" + std::to_string(score);
                        }
                        else
                        {
                            str += "\n" + std::to_string(score);
                            redStr += "\n";
                        }
                        break;
                    case ScoreType::Stableford:
                    case ScoreType::StablefordPro:
                        if (score > 0)
                        {
                            str += "\n" + std::to_string(score);
                            redStr += "\n";
                        }
                        else
                        {
                            str += "\n";
                            redStr += "\n" + std::to_string(score);
                        }
                        break;
                    }
                }
            }
        }
        entity.getComponent<cro::Text>().setString(str);
        redEnt.getComponent<cro::Text>().setString(redStr);

        if (page2)
        {
            bounds = cro::Text::getLocalBounds(entity);
            bounds.height /= 2.f;
            bounds.bottom += bounds.height;
            entity.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
            bounds.bottom -= 7.f; //again this random offset creeps in...
            redEnt.getComponent<cro::Drawable2D>().setCroppingArea(bounds); //this *ought* to be the same...
        }

        colStart += colWidth;
    }

    //make sure to pad the column count if less than 9 holes
    for (auto i = holeCount; i < 9; ++i)
    {
        colStart += colWidth;
    }

    colStart -= 14.f; //magic number because final column isn't right-aligned

    //total column
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ colStart, 0.f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(font).setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(LeaderboardTextSpacing);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    rootNode.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    textEntities.push_back(entity);

    str = "TOTAL\n";
    switch (m_sharedData.scoreType)
    {
    default:
        str += std::to_string(parTotalFront);
        break;
    case ScoreType::Stableford:
    case ScoreType::StablefordPro:
        if (page2)
        {
            str += "F9";
        }
        else
        {
            str += " ";
        }
        break;
    }

    for (const auto& entry : scoreEntries)
    {
        str += "\n" + std::to_string(entry.totalFront);

        switch (m_sharedData.scoreType)
        {
        default:
        case ScoreType::BattleRoyale:
        case ScoreType::MultiTarget:
        case ScoreType::Stroke:
        case ScoreType::ShortRound:
        {
            auto pTotal = entry.totalFront - parTotalFront;
            str += " (";
            if (pTotal > 0)
            {
                str += "+";
            }
            str += std::to_string(pTotal) + ")";
        }
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
            str += " - " + std::to_string(entry.total) + " POINTS";
            break;
        case ScoreType::Match:
            str += " - " + std::to_string(entry.roundScore) + " POINTS";
            break;
        case ScoreType::Skins:
            str += " - " + std::to_string(entry.roundScore) + " SKINS";
            break;
        }
    }

    if (page2)
    {
        //pad remaining rows
        for (auto i = 0u; i < 16u - scoreEntries.size(); ++i)
        {
            str += "\n";
        }

        str += "\n\nTOTAL\n";

        switch (m_sharedData.scoreType)
        {
        default:
            str += std::to_string(parTotalBack) + " (" + std::to_string(parTotal) + ")";
            break;
        case ScoreType::Stableford:
        case ScoreType::StablefordPro:
            str += "B9 - FINAL";
            break;
        }

        for (const auto& entry : scoreEntries)
        {
            str += "\n" + std::to_string(entry.totalBack);

            switch (m_sharedData.scoreType)
            {
            default:
            case ScoreType::BattleRoyale:
            case ScoreType::MultiTarget:
            case ScoreType::ShortRound:
            case ScoreType::Stroke:
            {
                auto pTotal = entry.total - parTotal;
                str += " (";
                if (pTotal > 0)
                {
                    str += "+";
                }
                str += std::to_string(pTotal) + ")";
            }
                break;
            case ScoreType::Stableford:
            case ScoreType::StablefordPro:
                if (entry.totalBack < 10 && entry.totalBack > -1)
                {
                    str += " ";
                }
                str += " - " + std::to_string(entry.total) + " POINTS";
                break;
            case ScoreType::Match:
                str += " - " + std::to_string(entry.roundScore) + " POINTS";
                break;
            case ScoreType::Skins:
                str += " - " + std::to_string(entry.roundScore) + " SKINS";
                break;
            }
        }
    }
    entity.getComponent<cro::Text>().setString(str);

    if (page2)
    {
        bounds = cro::Text::getLocalBounds(entity);
        bounds.height /= 2.f;
        bounds.bottom += bounds.height;
        entity.getComponent<cro::Drawable2D>().setCroppingArea(bounds);

        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 253.f, 14.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Text>(smallFont).setString("Front 9");
        entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
        entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
        entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
        entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
        centreText(entity);
        m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        auto labelEnt = entity;

        //add buttons if more than one page
        cro::SpriteSheet spriteSheet;
        spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu.spt", m_resources.textures);
        
        auto selected = m_uiScene.getSystem<cro::UISystem>()->addCallback([&](cro::Entity e)
            {
                e.getComponent<cro::SpriteAnimation>().play(1); 
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            });
        auto unselected = m_uiScene.getSystem<cro::UISystem>()->addCallback([](cro::Entity e) {e.getComponent<cro::SpriteAnimation>().play(0); });

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({190.f, 6.f, 0.1f});
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_left");
        entity.addComponent<cro::SpriteAnimation>();
        entity.addComponent<cro::UIInput>().setGroup(MenuID::Scorecard);
        entity.getComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, labelEnt, rootNode, textEntities](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt)
                        && rootNode.getComponent<cro::Transform>().getPosition().y > RootPosition.y)
                    {
                        labelEnt.getComponent<cro::Text>().setString("Front 9");
                        centreText(labelEnt);

                        rootNode.getComponent<cro::Transform>().setPosition(RootPosition);
                        for (auto e : textEntities)
                        {
                            auto bounds = e.getComponent<cro::Drawable2D>().getCroppingArea();
                            bounds.bottom += bounds.height;
                            e.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
                        }

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                });
        m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 292.f, 6.f, 0.1f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("arrow_right");
        entity.addComponent<cro::SpriteAnimation>();
        entity.addComponent<cro::UIInput>().setGroup(MenuID::Scorecard);
        entity.getComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselected;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] =
            m_uiScene.getSystem<cro::UISystem>()->addCallback(
                [&, labelEnt, rootNode, textEntities](cro::Entity, const cro::ButtonEvent& evt) mutable
                {
                    if (activated(evt)
                        && rootNode.getComponent<cro::Transform>().getPosition().y < (RootPosition.y + 1.f))
                    {
                        labelEnt.getComponent<cro::Text>().setString("Back 9");
                        centreText(labelEnt);

                        rootNode.getComponent<cro::Transform>().setPosition(RootPosition + glm::vec3(0.f, 266.f, 0.f));
                        for (auto e : textEntities)
                        {
                            auto bounds = e.getComponent<cro::Drawable2D>().getCroppingArea();
                            bounds.bottom -= bounds.height;
                            e.getComponent<cro::Drawable2D>().setCroppingArea(bounds);
                        }

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                });
        m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    }
    else
    {
        //add a dummy button to allow the menu group to switch
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>();
        entity.addComponent<cro::UIInput>().setGroup(MenuID::Scorecard);
    }

    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/lobby_menu_v2.spt", m_resources.textures);

    //add a button to the lobby page to display the score card
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("score_button");
    entity.addComponent<UIElement>().absolutePosition = { 147.f, 19.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("score_button_highlight");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<UIElement>().absolutePosition = { 145.f, 17.f };
    entity.getComponent<UIElement>().depth = 0.1f;
    entity.addComponent<cro::UIInput>().area = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Lobby);
    entity.getComponent<cro::UIInput>().setSelectionIndex(InfoScorecard);
    entity.getComponent<cro::UIInput>().setNextIndex(InfoLeague, LobbyRulesB);
    entity.getComponent<cro::UIInput>().setPrevIndex(InfoLeague, LobbyStart);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselectHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                togglePreviousScoreCard();
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        }
    );
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().function = [&](cro::Entity e, float)
    {
        e.getComponent<cro::UIInput>().enabled =
            m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().getScale().y != 0.f;
    };
    m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    m_lobbyButtonContext.hasScoreCard = true;
}

void MenuState::togglePreviousScoreCard()
{
    if (m_lobbyWindowEntities[LobbyEntityID::Scorecard].isValid()
        && !m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Callback>().active)
    {
        if (m_currentMenu == MenuID::Lobby)
        {
            if (m_uiScene.getSystem<cro::UISystem>()->getActiveGroup() == MenuID::Lobby)
            {
                m_currentMenu = MenuID::Dummy;
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);

                m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Callback>().getUserData<ScorecardCallbackData>().direction = 1;
            }
            else
            {
                //we're already on a different menu so hide the score card
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Callback>().getUserData<ScorecardCallbackData>().direction = 0;
            }
            m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Callback>().active = true;
        }
        else
        {
            //doesn't matter where we are, just hide the menu
            //remember to only switch back to lobby if we did anything though...
            m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Callback>().getUserData<ScorecardCallbackData>().direction = 0;
            m_lobbyWindowEntities[LobbyEntityID::Scorecard].getComponent<cro::Callback>().active = true;
        }
    }
}

void MenuState::refreshLobbyButtons()
{
    if (m_sharedData.hosting)
    {
        m_lobbyButtonContext.lobbyCourseB.getComponent<cro::UIInput>().setPrevIndex(PlayerManagement, InfoLeaderboards);

        m_lobbyButtonContext.lobbyCourseA.getComponent<cro::UIInput>().setNextIndex(LobbyInfoA, LobbyQuit);
        m_lobbyButtonContext.lobbyCourseA.getComponent<cro::UIInput>().setPrevIndex(PlayerManagement, Social::getClubLevel() ? RulesClubset : RulesGimmePrev);

        m_lobbyButtonContext.lobbyInfoA.getComponent<cro::UIInput>().setNextIndex(PlayerManagement, LobbyStart);
        m_lobbyButtonContext.lobbyInfoA.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseA, Social::getClubLevel() ? RulesClubset : RulesGimmeNext);

        m_lobbyButtonContext.lobbyInfoB.getComponent<cro::UIInput>().setNextIndex(PlayerManagement, LobbyStart);
        m_lobbyButtonContext.lobbyInfoB.getComponent<cro::UIInput>().setPrevIndex(LobbyRulesA, CourseWeather);

        m_lobbyButtonContext.lobbyRulesA.getComponent<cro::UIInput>().setNextIndex(LobbyInfoB, LobbyQuit);
        m_lobbyButtonContext.lobbyRulesA.getComponent<cro::UIInput>().setPrevIndex(PlayerManagement, CourseNext);

        if (m_lobbyButtonContext.rulesClubset.isValid())
        {
            m_lobbyButtonContext.rulesClubset.getComponent<cro::UIInput>().setNextIndex(LobbyInfoA, LobbyInfoA);
            m_lobbyButtonContext.rulesClubset.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseA, RulesGimmePrev);
        }
    }
    else
    {
        m_lobbyButtonContext.lobbyCourseB.getComponent<cro::UIInput>().setPrevIndex(LobbyRulesB, InfoLeaderboards);

        m_lobbyButtonContext.lobbyCourseA.getComponent<cro::UIInput>().setNextIndex(LobbyInfoA, LobbyQuit);
        m_lobbyButtonContext.lobbyCourseA.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoA, Social::getClubLevel() ? RulesClubset : LobbyStart);

        m_lobbyButtonContext.lobbyInfoA.getComponent<cro::UIInput>().setNextIndex(LobbyCourseA, LobbyStart);
        m_lobbyButtonContext.lobbyInfoA.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseA, Social::getClubLevel() ? RulesClubset : LobbyStart);

        m_lobbyButtonContext.lobbyInfoB.getComponent<cro::UIInput>().setNextIndex(LobbyRulesA, LobbyStart);
        m_lobbyButtonContext.lobbyInfoB.getComponent<cro::UIInput>().setPrevIndex(LobbyRulesA, LobbyStart);

        m_lobbyButtonContext.lobbyRulesA.getComponent<cro::UIInput>().setNextIndex(LobbyInfoB, LobbyQuit);
        m_lobbyButtonContext.lobbyRulesA.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoB, LobbyQuit);

        if (m_lobbyButtonContext.rulesClubset.isValid())
        {
            m_lobbyButtonContext.rulesClubset.getComponent<cro::UIInput>().setNextIndex(LobbyInfoA, LobbyInfoA);
            m_lobbyButtonContext.rulesClubset.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseA, LobbyCourseA);
        }
    }

    if (m_lobbyButtonContext.hasScoreCard)
    {
        m_lobbyButtonContext.lobbyRulesB.getComponent<cro::UIInput>().setNextIndex(PlayerManagement, LobbyStart);
        m_lobbyButtonContext.lobbyRulesB.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseB, InfoLeague);

        m_lobbyButtonContext.infoLeague.getComponent<cro::UIInput>().setNextIndex(InfoScorecard, LobbyRulesB);
        m_lobbyButtonContext.infoLeague.getComponent<cro::UIInput>().setPrevIndex(InfoLeaderboards, LobbyQuit);
    }
    else
    {
        m_lobbyButtonContext.lobbyRulesB.getComponent<cro::UIInput>().setNextIndex(PlayerManagement, LobbyStart);
        m_lobbyButtonContext.lobbyRulesB.getComponent<cro::UIInput>().setPrevIndex(LobbyCourseB, InfoLeague);

        m_lobbyButtonContext.infoLeague.getComponent<cro::UIInput>().setNextIndex(LobbyRulesB, LobbyRulesB);
        m_lobbyButtonContext.infoLeague.getComponent<cro::UIInput>().setPrevIndex(InfoLeaderboards, LobbyQuit);
    }
}