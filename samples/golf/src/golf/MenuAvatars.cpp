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

#include "MenuState.hpp"
#include "GameConsts.hpp"
#include "MenuConsts.hpp"
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>

#include <crogine/util/Random.hpp>

namespace
{
#include "RandNames.hpp"
}

void MenuState::createAvatarMenuNew(cro::Entity parent, std::uint32_t mouseEnter, std::uint32_t mouseExit)
{
    //if this is a default profile it'll be empty, so fill it with a social name
    //or a random name if social name doesn't exist
    if (m_sharedData.localConnectionData.playerData[0].name.empty())
    {
        //default player one to Social name if it exists
        if (m_matchMaking.getUserName())
        {
            auto codePoints = cro::Util::String::getCodepoints(std::string(m_matchMaking.getUserName()));
            cro::String nameStr = cro::String::fromUtf32(codePoints.begin(), codePoints.end());
            m_sharedData.localConnectionData.playerData[0].name = nameStr.substr(0, ConstVal::MaxStringChars);
        }
        else
        {
            m_sharedData.localConnectionData.playerData[0].name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
        }
        m_sharedData.localConnectionData.playerData[0].saveProfile();
        m_sharedData.playerProfiles.push_back(m_sharedData.localConnectionData.playerData[0]);
    }

    auto menuEntity = m_uiScene.createEntity();
    menuEntity.addComponent<cro::Transform>().setScale(glm::vec2(0.f));
    menuEntity.addComponent<cro::Callback>().setUserData<MenuData>();
    menuEntity.getComponent<cro::Callback>().function = MenuCallback(MainMenuContext(this));
    m_menuEntities[MenuID::Avatar] = menuEntity;
    parent.getComponent<cro::Transform>().addChild(menuEntity.getComponent<cro::Transform>());

    //sprites
    cro::SpriteSheet spriteSheet;
    spriteSheet.loadFromFile("assets/golf/sprites/player_menu.spt", m_resources.textures);

    m_sprites[SpriteID::ArrowLeft] = spriteSheet.getSprite("arrow_l");
    m_sprites[SpriteID::ArrowLeftHighlight] = spriteSheet.getSprite("arrow_l_h");
    m_sprites[SpriteID::ArrowRight] = spriteSheet.getSprite("arrow_r");
    m_sprites[SpriteID::ArrowRightHighlight] = spriteSheet.getSprite("arrow_r_h");
    m_sprites[SpriteID::Controller] = spriteSheet.getSprite("controller");
    m_sprites[SpriteID::Keyboard] = spriteSheet.getSprite("keyboard");
    m_sprites[SpriteID::ThumbBackground] = spriteSheet.getSprite("thumb_bg");
    m_sprites[SpriteID::CPUHighlight] = spriteSheet.getSprite("check_highlight");
    m_sprites[SpriteID::PlayerEdit] = spriteSheet.getSprite("edit_button");


    //this entity is the background ent with avatar information added to it
    auto avatarEnt = m_uiScene.createEntity();
    avatarEnt.addComponent<cro::Transform>();
    avatarEnt.addComponent<cro::Drawable2D>();
    avatarEnt.addComponent<cro::Sprite>(m_resources.textures.get("assets/golf/images/avatar_menu.png"));
    avatarEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    avatarEnt.getComponent<UIElement>().depth = -0.2f;
    avatarEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    auto bounds = avatarEnt.getComponent<cro::Sprite>().getTextureBounds();
    avatarEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    menuEntity.getComponent<cro::Transform>().addChild(avatarEnt.getComponent<cro::Transform>());
    m_avatarMenu = avatarEnt;

    auto& menuTransform = menuEntity.getComponent<cro::Transform>();
    menuTransform.setPosition(-m_menuPositions[MenuID::Avatar]);

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
    //team roster
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 28.f, 223.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_sharedData.localConnectionData.playerData[0].name);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto rosterEnt = entity;

    //title
    entity = m_uiScene.createEntity();
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
    entity.addComponent<cro::Transform>().setPosition({ -10000.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto cursorEnt = entity;

    //this callback is overridden for the avatar previews
    mouseEnter = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, entity, avatarEnt](cro::Entity e) mutable
        {
            auto basePos = avatarEnt.getComponent<cro::Transform>().getPosition();
            basePos -= avatarEnt.getComponent<cro::Transform>().getOrigin();

            static constexpr glm::vec3 Offset(93.f, -6.f, 0.f);
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + basePos + Offset);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(-1.f, 1.f));
            entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Back);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });


    //and this one is used for regular buttons.
    auto mouseEnterCursor = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&, entity](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            entity.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            entity.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            //entity.getComponent<cro::Callback>().setUserData<std::pair<cro::Entity, glm::vec3>>(e, CursorOffset);
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
    entity.addComponent<cro::Transform>().setPosition({ 20.f, MenuBottomBorder });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevMenu];
    entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::PrevMenu].getTextureBounds();
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
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


    auto updateRoster = [&, rosterEnt]() mutable
    {
        auto str = m_sharedData.localConnectionData.playerData[0].name;
        for (auto i = 1u; i < m_sharedData.localConnectionData.playerCount; ++i)
        {
            str += "\n" + m_sharedData.localConnectionData.playerData[i].name;
        }
        rosterEnt.getComponent<cro::Text>().setString(str);

        //TODO add profile scroll buttons for each player
        //TODO add CPU toggle for each player
    };


    //add player button
    entity = m_uiScene.createEntity();
    entity.setLabel("Add Player");
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -40.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.3334f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::AddPlayer];
    bounds = m_sprites[SpriteID::AddPlayer].getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, updateRoster](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    if (m_sharedData.localConnectionData.playerCount < ConstVal::MaxPlayers)
                    {
                        auto index = m_sharedData.localConnectionData.playerCount;

                        m_sharedData.localConnectionData.playerData[index] = m_sharedData.playerProfiles[std::min(std::size_t(index), m_sharedData.playerProfiles.size() - 1)];
                        //TODO track this slot's index into the profile array

                        m_sharedData.localConnectionData.playerCount++;
                        
                        //refresh the current roster
                        updateRoster();

                        m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

    auto labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ 18.f, 12.f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(smallFont).setString("Add Player");
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(labelEnt).width;


    createProfileLayout(avatarEnt, menuTransform, spriteSheet);


    //remove player button
    entity = m_uiScene.createEntity();
    entity.setLabel("Remove Player");
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<UIElement>().absolutePosition = { -30.f, MenuBottomBorder };
    entity.getComponent<UIElement>().relativePosition = { 0.6667f, 0.f };
    entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::RemovePlayer];
    bounds = m_sprites[SpriteID::RemovePlayer].getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, updateRoster](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();

                    if (m_sharedData.localConnectionData.playerCount > 1)
                    {
                        m_sharedData.localConnectionData.playerCount--;
                        
                        //refresh the roster view
                        updateRoster();

                        m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                    }
                }
            });
    menuTransform.addChild(entity.getComponent<cro::Transform>());

    labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ 18.f, 12.f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(smallFont).setString("Remove Player");
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
    entity.getComponent<cro::UIInput>().area.width += cro::Text::getLocalBounds(labelEnt).width;

    createMenuCallbacks();

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
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = mouseExitHighlight;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&, menuEntity](cro::Entity e, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    applyTextEdit();
                    refreshUI();

                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                    if (m_sharedData.hosting)
                    {
                        if (!m_sharedData.clientConnection.connected)
                        {
                            m_sharedData.serverInstance.launch(ConstVal::MaxClients, Server::GameMode::Golf);

                            //small delay for server to get ready
                            cro::Clock clock;
                            while (clock.elapsed().asMilliseconds() < 500) {}

                            m_matchMaking.createGame(ConstVal::MaxClients, Server::GameMode::Golf);
                        }
                    }
                    else
                    {
                        m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Dummy);
                        menuEntity.getComponent<cro::Callback>().getUserData<MenuData>().targetMenu = MenuID::Join;
                        menuEntity.getComponent<cro::Callback>().active = true;

                        m_matchMaking.refreshLobbyList(Server::GameMode::Golf);
                        updateLobbyList();
                    }

                    //kludgy way of temporarily disabling this button to prevent double clicks
                    auto defaultCallback = e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp];
                    e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = 0;

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
                            e.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = cb;
                            t.getComponent<cro::Callback>().active = false;
                            m_uiScene.destroyEntity(t);
                        }
                    };
                }
            });
    entity.getComponent<UIElement>().absolutePosition.x = -bounds.width;
    menuTransform.addChild(entity.getComponent<cro::Transform>());
}

void MenuState::createMenuCallbacks()
{
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

    m_courseSelectCallbacks.prevHoleCount = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                prevHoleCount();
            }
        });
    m_courseSelectCallbacks.nextHoleCount = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                nextHoleCount();
            }
        });

    m_courseSelectCallbacks.prevCourse = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                prevCourse();
            }
        });
    m_courseSelectCallbacks.nextCourse = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                nextCourse();
            }
        });

    m_courseSelectCallbacks.prevHoleType = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                do
                {
                    m_currentRange = (m_currentRange + (Range::Count - 1)) % Range::Count;
                    m_sharedData.courseIndex = m_courseIndices[m_currentRange].start;
                } while (m_courseIndices[m_currentRange].count == 0);
                nextCourse(); //silly way of refreshing the display
                prevCourse();

                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::CourseType;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(CourseTypes[m_currentRange]);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
        });
    m_courseSelectCallbacks.nextHoleType = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                do
                {
                    m_currentRange = (m_currentRange + 1) % Range::Count;
                    m_sharedData.courseIndex = m_courseIndices[m_currentRange].start;
                } while (m_courseIndices[m_currentRange].count == 0);
                prevCourse();
                nextCourse();

                cro::Command cmd;
                cmd.targetFlags = CommandID::Menu::CourseType;
                cmd.action = [&](cro::Entity e, float)
                {
                    e.getComponent<cro::Text>().setString(CourseTypes[m_currentRange]);
                };
                m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
            }
        });

    m_courseSelectCallbacks.toggleReverseCourse = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.reverseCourse = m_sharedData.reverseCourse == 0 ? 1 : 0;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ReverseCourse, m_sharedData.reverseCourse, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_courseSelectCallbacks.toggleFriendsOnly = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_matchMaking.setFriendsOnly(!m_matchMaking.getFriendsOnly());

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_courseSelectCallbacks.toggleGameRules = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto rect = e.getComponent<cro::Sprite>().getTextureRect();
                auto area = e.getComponent<cro::UIInput>().area;

                float scale = m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y;
                if (scale == 0)
                {
                    scale = 1.f;
                    rect.bottom -= 16.f;
                    area.left += area.width;
                    m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
                }
                else
                {
                    scale = 0.f;
                    rect.bottom += 16.f;
                    area.left -= area.width;
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }

                m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().setScale({ 1.f, scale });
                e.getComponent<cro::Sprite>().setTextureRect(rect);
                e.getComponent<cro::UIInput>().area = area;
            }
        });

    m_courseSelectCallbacks.inviteFriends = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                Social::inviteFriends(m_sharedData.lobbyID);
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

    m_courseSelectCallbacks.selectHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();
        });
    m_courseSelectCallbacks.unselectHighlight = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });

    m_courseSelectCallbacks.selectText = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextGoldColour);
            e.getComponent<cro::AudioEmitter>().play();
        });
    m_courseSelectCallbacks.unselectText = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            e.getComponent<cro::Text>().setFillColour(TextNormalColour);
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
}

void MenuState::createProfileLayout(cro::Entity parent, cro::Transform& menuTransform, const cro::SpriteSheet& spriteSheet)
{
    //XP info
    const float xPos = spriteSheet.getSprite("background").getTextureRect().width / 2.f;
    auto entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ xPos, 10.f, 0.1f });

    const auto progress = Social::getLevelProgress();
    constexpr float BarWidth = 80.f;
    constexpr float BarHeight = 10.f;
    const auto CornerColour = cro::Colour(std::uint8_t(101), 67, 47);
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, BarHeight / 2.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), TextHighlightColour),

            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), TextHighlightColour),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), TextHighlightColour),

            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), BarHeight / 2.f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + (BarWidth * progress.progress), -BarHeight / 2.f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), LeaderboardTextDark),

            //corners
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, BarHeight / 2.f), CornerColour),

            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (BarHeight / 2.f) - 1.f), CornerColour),

            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),

            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),
            cro::Vertex2D(glm::vec2(-BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2((-BarWidth / 2.f) + 1.f, -BarHeight / 2.f), CornerColour),


            cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (BarHeight / 2.f) - 1.f), CornerColour),
            cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), CornerColour),

            cro::Vertex2D(glm::vec2(BarWidth / 2.f, BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (BarHeight / 2.f) - 1.f), CornerColour),
            cro::Vertex2D(glm::vec2(BarWidth / 2.f, (BarHeight / 2.f) - 1.f), CornerColour),

            cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, (-BarHeight / 2.f) + 1.f), CornerColour),
            cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, -BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2(BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),

            cro::Vertex2D(glm::vec2(BarWidth / 2.f, (-BarHeight / 2.f) + 1.f), CornerColour),
            cro::Vertex2D(glm::vec2((BarWidth / 2.f) - 1.f, -BarHeight / 2.f), CornerColour),
            cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), CornerColour)
        });
    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    parent.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    if (Social::getLevel() > 0)
    {
        auto iconEnt = m_uiScene.createEntity();
        iconEnt.addComponent<cro::Transform>();
        iconEnt.addComponent<cro::Drawable2D>();
        iconEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("rank_icon");
        iconEnt.addComponent<cro::SpriteAnimation>().play(std::min(5, Social::getLevel() / 10));
        auto bounds = iconEnt.getComponent<cro::Sprite>().getTextureBounds();
        iconEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        iconEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.f };
        iconEnt.getComponent<UIElement>().absolutePosition = { 0.f, 24.f };
        iconEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
        menuTransform.addChild(iconEnt.getComponent<cro::Transform>());
    }

    auto& font = m_sharedData.sharedResources->fonts.get(FontID::Info);
    auto labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ (-BarWidth / 2.f) + 2.f, 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Level " + std::to_string(Social::getLevel()));
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(-xPos * 0.6f), 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString(std::to_string(progress.currentXP) + "/" + std::to_string(progress.levelXP) + " XP");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    centreText(labelEnt);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());

    labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ std::floor(xPos * 0.6f), 4.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Total: " + std::to_string(Social::getXP()) + " XP");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    centreText(labelEnt);
    entity.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());


    //TODO club status
    //TODO current streak
    //TODO add longest streak
}