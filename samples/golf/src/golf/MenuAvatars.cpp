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
#include "Clubs.hpp"
#include "TextAnimCallback.hpp"
#include "../ErrorCheck.hpp"

#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/UIInput.hpp>
#include <crogine/ecs/components/SpriteAnimation.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>

#include <crogine/graphics/SpriteSheet.hpp>
#include <crogine/graphics/SimpleText.hpp>

#include <crogine/gui/Gui.hpp>
#include <crogine/util/Random.hpp>

#include <filesystem>

namespace
{
#include "RandNames.hpp"
}

void MenuState::createAvatarMenu(cro::Entity parent)
{
    //registerWindow([&]() 
    //    {
    //        if (ImGui::Begin("buns"))
    //        {
    //            for (auto& t : m_sharedData.avatarTextures[0])
    //            {
    //                ImGui::Image(t, { 128.f, 128.f }, { 0.f, 1.f }, { 1.f, 0.f });
    //                ImGui::SameLine();
    //            }
    //            ImGui::NewLine();

    //            for (auto& t : m_profileTextures)
    //            {
    //                ImGui::Image(t.getTexture(), {128.f, 128.f}, {0.f, 1.f}, {1.f, 0.f});
    //                ImGui::SameLine();
    //            }
    //        }
    //        ImGui::End();
    //    });


    //TODO this could be moved to createUI()
    createMenuCallbacks();

    cro::String socialName;
    if (m_matchMaking.getUserName())
    {
        auto codePoints = cro::Util::String::getCodepoints(std::string(m_matchMaking.getUserName()));
        cro::String nameStr = cro::String::fromUtf32(codePoints.begin(), codePoints.end());
        socialName = nameStr.substr(0, ConstVal::MaxStringChars);
    }

    //if this is a default profile it'll be empty, so fill it with a social name
    //or a random name if social name doesn't exist
    if (m_sharedData.localConnectionData.playerData[0].name.empty())
    {
        //default player one to Social name if it exists
        if (!socialName.empty())
        {
            m_sharedData.localConnectionData.playerData[0].name = socialName;
        }
        else
        {
            m_sharedData.localConnectionData.playerData[0].name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
        }
        m_sharedData.localConnectionData.playerData[0].saveProfile();
        m_sharedData.playerProfiles.push_back(m_sharedData.localConnectionData.playerData[0]);
        m_profileTextures.emplace_back(m_sharedData.avatarInfo[indexFromAvatarID(m_sharedData.localConnectionData.playerData[0].skinID)].texturePath);
        updateProfileTextures(0, m_profileTextures.size());
    }
    else
    {
        //see if we have a social name and put to the font of the profile list
        if (!socialName.empty())
        {
            auto res = std::find_if(m_sharedData.playerProfiles.begin(), m_sharedData.playerProfiles.end(),
                [&socialName](const PlayerData& pd)
                {
                    return pd.name == socialName;
                });

            if (res != m_sharedData.playerProfiles.end())
            {
                auto pos = std::distance(m_sharedData.playerProfiles.begin(), res);

                std::swap(m_sharedData.playerProfiles[0], m_sharedData.playerProfiles[pos]);

                //remember to realign the texture indices!
                std::swap(m_profileTextures[0], m_profileTextures[pos]);
            }
        }
    }
    m_sharedData.localConnectionData.playerData[0] = m_sharedData.playerProfiles[0];
    
    //map all the active player slots to their profile index
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        const auto& uid = m_sharedData.localConnectionData.playerData[i].profileID;
        if (auto result = std::find_if(m_sharedData.playerProfiles.begin(), m_sharedData.playerProfiles.end(),
            [&uid](const PlayerData& pd) {return pd.profileID == uid;}); result != m_sharedData.playerProfiles.end())
        {
            m_rosterMenu.profileIndices[i] = std::distance(m_sharedData.playerProfiles.begin(), result);
        }
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

    m_sprites[SpriteID::CPUHighlight] = spriteSheet.getSprite("check_highlight");


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
    auto bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ std::floor(bounds.width / 2.f), std::floor(bounds.height / 2.f) });
    entity.addComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function = TitleTextCallback();
    menuTransform.addChild(entity.getComponent<cro::Transform>());


    //rank icon
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

    spriteSheet.loadFromFile("assets/golf/sprites/avatar_menu.spt", m_resources.textures);


    //this entity is the background ent with avatar information added to it
    auto avatarEnt = m_uiScene.createEntity();
    avatarEnt.addComponent<cro::Transform>();
    avatarEnt.addComponent<cro::Drawable2D>();
    avatarEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("background");
    avatarEnt.addComponent<UIElement>().relativePosition = { 0.5f, 0.5f };
    avatarEnt.getComponent<UIElement>().absolutePosition = { 0.f, 10.f };
    avatarEnt.getComponent<UIElement>().depth = -0.2f;
    avatarEnt.addComponent<cro::CommandTarget>().ID = CommandID::Menu::UIElement;
    bounds = avatarEnt.getComponent<cro::Sprite>().getTextureBounds();
    avatarEnt.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    menuTransform.addChild(avatarEnt.getComponent<cro::Transform>());
    m_avatarMenu = avatarEnt;

    //cursor
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ -10000.f, 0.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::Cursor];
    entity.addComponent<cro::SpriteAnimation>().play(0);
    menuTransform.addChild(entity.getComponent<cro::Transform>());
    auto cursorEnt = entity;

    auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);

    //active profile name
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({382.f, 226.f, 0.1f});
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString("Buns");
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    centreText(entity);
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto nameLabel = entity;

    //active profile ball
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 267.f, 76.f, 0.1f });
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_ballTexture.getTexture());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    auto addCorners = [&](cro::Entity p, cro::Entity q)
    {
        auto bounds = q.getComponent<cro::Sprite>().getTextureBounds();
        auto offset = q.getComponent<cro::Transform>().getPosition();
        
        auto cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_bl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        auto cornerBounds = cornerEnt.getComponent<cro::Sprite>().getTextureBounds();

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, bounds.height - cornerBounds.height, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tr");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ bounds.width - cornerBounds.width, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_br");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());
    };

    addCorners(avatarEnt, entity);

    //active profile avatar
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 396.f, 76.f, 0.1f });
    if (!m_sharedData.pixelScale)
    {
        entity.getComponent<cro::Transform>().setScale(glm::vec2(1.f / getViewScale()));
    }
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>(m_avatarTexture.getTexture());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    addCorners(avatarEnt, entity);

    //active profile mugshot
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 396.f, 24.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>();
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto mugshot = entity;

    //team roster
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 38.f, 225.f, 0.1f });
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(largeFont).setString(m_sharedData.localConnectionData.playerData[0].name);
    entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
    entity.getComponent<cro::Text>().setVerticalSpacing(6.f);
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    auto rosterEnt = entity;

    auto showAvatar = [&, mugshot](std::size_t profileIndex) mutable
    {
        const auto& profile = m_sharedData.playerProfiles[profileIndex];

        for (auto& e : m_playerAvatars)
        {
            e.previewModel.getComponent<cro::Transform>().setScale(glm::vec3(0.f));
            if(e.hairAttachment != nullptr
                && e.hairAttachment->getModel().isValid())
            {
                e.hairAttachment->getModel().getComponent<cro::Model>().setHidden(true);
            }
        }
        auto idx = indexFromAvatarID(profile.skinID);
        m_playerAvatars[idx].previewModel.getComponent<cro::Transform>().setScale(glm::vec3(profile.flipped ? -1.f : 1.f, 1.f, 1.f));
        m_playerAvatars[idx].previewModel.getComponent<cro::Model>().setFacing(profile.flipped ? cro::Model::Facing::Back : cro::Model::Facing::Front);

        //use profile ID to set model texture
        m_playerAvatars[idx].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", m_profileTextures[profileIndex].getTexture());

        if (m_profileTextures[profileIndex].getMugshot())
        {
            glm::vec2 texSize(m_profileTextures[profileIndex].getMugshot()->getSize());
            glm::vec2 scale = glm::vec2(98.f, 42.f) / texSize;

            mugshot.getComponent<cro::Transform>().setScale(scale);
            mugshot.getComponent<cro::Sprite>().setTexture(*m_profileTextures[profileIndex].getMugshot());
        }
        else
        {
            mugshot.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }

        //update hair model
        if (m_playerAvatars[idx].hairAttachment != nullptr)
        {
            auto hID = indexFromHairID(profile.hairID);
            auto& hair = m_playerAvatars[idx].hairModels[hID];
            if (hair.model.isValid())
            {
                hair.model.getComponent<cro::Model>().setHidden(false);
                m_playerAvatars[idx].hairAttachment->setModel(hair.model);

                //apply hair colour to material
                auto hairColour = m_profileTextures[profileIndex].getColour(pc::ColourKey::Hair).first;
                hair.model.getComponent<cro::Model>().setMaterialProperty(0, "u_hairColour", hairColour);
            }
        }
    };


    updateRoster = [&, rosterEnt, nameLabel, showAvatar]() mutable
    {
        //update the CPU icon, or hide if not added
        for (auto i = 0u; i < ConstVal::MaxPlayers; ++i)
        {
            m_rosterMenu.selectionEntities[i].getComponent<cro::SpriteAnimation>().play(
                m_sharedData.localConnectionData.playerData[i].isCPU ? 5 : 0
            );

            float scale = i < m_sharedData.localConnectionData.playerCount ? 1.f : 0.f;
            m_rosterMenu.selectionEntities[i].getComponent<cro::Transform>().setScale(glm::vec2(scale));

            //reset the input - it'll be updated below if needed
            m_rosterMenu.buttonEntities[i].getComponent<cro::UIInput>().enabled = false;
        }

        //then update the display string
        m_rosterMenu.buttonEntities[0].getComponent<cro::UIInput>().enabled = m_sharedData.localConnectionData.playerCount > 1;

        auto str = m_sharedData.localConnectionData.playerData[0].name;
        for (auto i = 1u; i < m_sharedData.localConnectionData.playerCount; ++i)
        {
            str += "\n" + m_sharedData.localConnectionData.playerData[i].name;

            m_rosterMenu.buttonEntities[i].getComponent<cro::UIInput>().enabled = true;
        }
        rosterEnt.getComponent<cro::Text>().setString(str);

        nameLabel.getComponent<cro::Text>().setString(m_sharedData.playerProfiles[m_rosterMenu.profileIndices[m_rosterMenu.activeIndex]].name);
        centreText(nameLabel);

        auto idx = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
        m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = indexFromBallID(m_sharedData.playerProfiles[idx].ballID);

        showAvatar(idx);
    };



    auto& uiSystem = *m_uiScene.getSystem<cro::UISystem>();
    auto selectionCallback = uiSystem.addCallback([cursorEnt](cro::Entity e)  mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White); 
            e.getComponent<cro::AudioEmitter>().play();
            e.getComponent<cro::Callback>().active = true;
            cursorEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    auto unselectionCallback = uiSystem.addCallback([](cro::Entity e) 
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent); 
        });
    auto activateCallback = uiSystem.addCallback([&, nameLabel, showAvatar](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                m_rosterMenu.activeIndex = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();

                nameLabel.getComponent<cro::Text>().setString(m_sharedData.playerProfiles[m_rosterMenu.profileIndices[m_rosterMenu.activeIndex]].name);
                centreText(nameLabel);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                auto idx = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = indexFromBallID(m_sharedData.playerProfiles[idx].ballID);

                showAvatar(idx);
            }
        });

    //each of the selection entities for the roster
    glm::vec3 highlightPos(20.f, 212.f, 0.08f);
    static constexpr float Spacing = 14.f;
    for (auto i = 0u; i < ConstVal::MaxPlayers; ++i)
    {
        //net strength icon
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition(highlightPos);
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::NetStrength];
        entity.addComponent<cro::SpriteAnimation>();
        avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_rosterMenu.selectionEntities[i] = entity;

        highlightPos.y -= Spacing;

        //attach the selection bar 
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.f, 2.f });
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_select_dark");
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<std::uint32_t>(i);
        entity.getComponent<cro::Callback>().function =
            [&](cro::Entity e, float)
        {
            const auto idx = e.getComponent<cro::Callback>().getUserData<std::uint32_t>();
            if (m_rosterMenu.activeIndex == idx)
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            }
            else
            {
                e.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
            }
        };
        m_rosterMenu.selectionEntities[i].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        //button
        entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 14.f, 2.f });
        entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
        entity.addComponent<cro::Drawable2D>();
        entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_highlight");
        entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        entity.addComponent<cro::Callback>().function = MenuTextCallback();
        entity.getComponent<cro::Callback>().setUserData<std::uint32_t>(i);
        entity.addComponent<cro::UIInput>().area = spriteSheet.getSprite("profile_highlight").getTextureBounds();
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = activateCallback;
        entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
        entity.getComponent<cro::UIInput>().enabled = false;

        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());

        m_rosterMenu.selectionEntities[i].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_rosterMenu.buttonEntities[i] = entity;
    }


    //CPU toggle
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({38.f, 106.f, 0.1f});
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
        [&](cro::Entity e, float)
    {
        cro::Colour c = m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex].isCPU ? TextGoldColour : cro::Colour::Transparent;
        auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
        for (auto& v : verts)
        {
            v.colour = c;
        }
    };
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({36.f, 104.f, 0.1f});
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::CPUHighlight];
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::CPUHighlight].getTextureBounds();
    entity.getComponent<cro::UIInput>().area.width *= 3.4f; //cover writing too
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = 
        uiSystem.addCallback([&, cursorEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::White);
            e.getComponent<cro::AudioEmitter>().play();

            //hide the cursor
            cursorEnt.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = 
        uiSystem.addCallback([&](cro::Entity e)
        {
            e.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonDown] = 
        uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                auto index = m_rosterMenu.activeIndex;
                m_sharedData.localConnectionData.playerData[index].isCPU =
                    !m_sharedData.localConnectionData.playerData[index].isCPU;

                bool isCPU = m_sharedData.localConnectionData.playerData[index].isCPU;

                //write profile with updated settings
                m_sharedData.playerProfiles[m_rosterMenu.profileIndices[index]].isCPU = isCPU;
                m_sharedData.playerProfiles[m_rosterMenu.profileIndices[m_rosterMenu.activeIndex]].saveProfile();

                //update roster
                m_rosterMenu.selectionEntities[index].getComponent<cro::SpriteAnimation>().play(isCPU ? 5 : 0);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Enter] = 
        uiSystem.addCallback([&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            showToolTip("Make this a computer controlled player");
        });
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = 
        uiSystem.addCallback([&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            hideToolTip();
        });
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());





    //prev profile
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 86.f, 104.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::PrevCourse];
    entity.addComponent<cro::SpriteAnimation>();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                auto i = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                i = (i + (m_sharedData.playerProfiles.size() - 1)) % m_sharedData.playerProfiles.size();
                m_rosterMenu.profileIndices[m_rosterMenu.activeIndex] = i;

                m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex] = m_sharedData.playerProfiles[i];

                updateRoster();

                auto avtIdx = indexFromAvatarID(m_sharedData.playerProfiles[i].skinID);
                auto soundSize = m_playerAvatars[avtIdx].previewSounds.size();
                if (soundSize != 0)
                {
                    auto idx = soundSize > 1 ? cro::Util::Random::value(0u, soundSize - 1) : 0;
                    m_playerAvatars[avtIdx].previewSounds[idx].getComponent<cro::AudioEmitter>().play();
                }
                else
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            }
        });
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //next profile
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 148.f, 104.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::NextCourse];
    entity.addComponent<cro::SpriteAnimation>();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                auto i = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                i = (i + 1) % m_sharedData.playerProfiles.size();
                m_rosterMenu.profileIndices[m_rosterMenu.activeIndex] = i;

                m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex] = m_sharedData.playerProfiles[i];

                updateRoster();

                auto avtIdx = indexFromAvatarID(m_sharedData.playerProfiles[i].skinID);
                auto soundSize = m_playerAvatars[avtIdx].previewSounds.size();
                if (soundSize != 0)
                {
                    auto idx = soundSize > 1 ? cro::Util::Random::value(0u, soundSize - 1) : 0;
                    m_playerAvatars[avtIdx].previewSounds[idx].getComponent<cro::AudioEmitter>().play();
                }
                else
                {
                    m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
                }
            }
        });
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //reset stats
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 151.f, 23.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("reset_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.errorMessage = "reset_profile";
                requestStackPush(StateID::MessageOverlay);
            }
        });
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


    //create profile
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 265.f, 52.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_edit_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                if (m_sharedData.playerProfiles.size() < ConstVal::MaxProfiles)
                {
                    m_sharedData.playerProfiles.emplace_back();
                    m_sharedData.playerProfiles.back().name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
                    m_sharedData.playerProfiles.back().saveProfile();
                    m_sharedData.activeProfileIndex = m_sharedData.playerProfiles.size() - 1;

                    //create profile texture
                    auto avtIdx = indexFromAvatarID(m_sharedData.playerProfiles.back().skinID);
                    m_profileTextures.emplace_back(m_sharedData.avatarInfo[avtIdx].texturePath);
                    updateProfileTextures(m_profileTextures.size() - 1, 1);

                    //set selected roster slot to this profile and refresh view
                    m_rosterMenu.profileIndices[m_rosterMenu.activeIndex] = m_sharedData.activeProfileIndex;
                    m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex] = m_sharedData.playerProfiles[m_sharedData.activeProfileIndex];

                    updateRoster();

                    requestStackPush(StateID::Profile);
                }
                else
                {
                    m_sharedData.errorMessage = "Maximum Profiles Reached.";
                    requestStackPush(StateID::MessageOverlay);
                }
            }
        });
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //edit profile
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 265.f, 37.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_edit_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.activeProfileIndex = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                requestStackPush(StateID::Profile);
            }
        });
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //delete profile
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 265.f, 22.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_edit_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                if (m_sharedData.playerProfiles.size() == 1)
                {
                    m_sharedData.errorMessage = "This Profile\nCannot Be Deleted";
                }
                else
                {
                    m_sharedData.errorMessage = "delete_profile";
                    m_sharedData.activeProfileIndex = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                }
                requestStackPush(StateID::MessageOverlay);
            }
        });
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());


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


    //and this one is used for regular buttons.
    auto mouseEnterCursor = uiSystem.addCallback(
        [cursorEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            cursorEnt.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition() + CursorOffset);
            cursorEnt.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
            cursorEnt.getComponent<cro::Drawable2D>().setFacing(cro::Drawable2D::Facing::Front);
            cursorEnt.getComponent<cro::Sprite>().setColour(cro::Colour::White);
        });

    auto mouseEnterHighlight = uiSystem.addCallback(
        [cursorEnt](cro::Entity e) mutable
        {
            e.getComponent<cro::AudioEmitter>().play();
            auto bounds = e.getComponent<cro::Sprite>().getTextureRect();
            bounds.left += bounds.width;
            e.getComponent<cro::Sprite>().setTextureRect(bounds);
            cursorEnt.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        });
    auto mouseExitHighlight = uiSystem.addCallback(
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
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.localConnectionData.playerCount < ConstVal::MaxPlayers)
                    {
                        auto index = m_sharedData.localConnectionData.playerCount;
                        auto profileIndex = index % m_sharedData.playerProfiles.size();
                        
                        m_sharedData.localConnectionData.playerData[index] = m_sharedData.playerProfiles[profileIndex];
                        m_sharedData.localConnectionData.playerCount++;
                        
                        m_rosterMenu.profileIndices[index] = profileIndex;

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
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.localConnectionData.playerCount > 1)
                    {
                        m_sharedData.localConnectionData.playerCount--;
                        
                        m_rosterMenu.activeIndex = std::min(m_rosterMenu.activeIndex, std::size_t(m_sharedData.localConnectionData.playerCount - 1));

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


    //make sure any active players are included in the list
    updateRoster();
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

    
    //club status
    if (m_clubTexture.create(210, 36, false))
    {
        auto clubEnt = m_uiScene.createEntity();
        clubEnt.addComponent<cro::Transform>().setPosition({ 23.f, 57.f, 0.1f });
        clubEnt.addComponent<cro::Drawable2D>();
        clubEnt.addComponent<cro::Sprite>(m_clubTexture.getTexture());
        parent.getComponent<cro::Transform>().addChild(clubEnt.getComponent<cro::Transform>());

        cro::SimpleText text(font);
        text.setFillColour(TextNormalColour);
        text.setShadowColour(LeaderboardTextDark);
        text.setShadowOffset({ 1.f, -1.f });
        text.setCharacterSize(InfoTextSize);

        glm::vec2 position(2.f, 28.f);

        m_clubTexture.clear(cro::Colour::Transparent);

        static constexpr std::int32_t Col = 4;
        static constexpr std::int32_t Row = 3;

        const auto flags = Social::getUnlockStatus(Social::UnlockType::Club);

        for (auto i = 0; i < Row; ++i)
        {
            for (auto j = 0; j < Col; ++j)
            {
                auto idx = i + (j * Row);
                text.setString(Clubs[idx].getLabel() + " ");
                text.setPosition(position);
                
                if (flags & (1 << idx))
                {
                    if (ClubID::DefaultSet & (1 << idx))
                    {
                        text.setFillColour(TextNormalColour);
                    }
                    else
                    {
                        text.setFillColour(TextGoldColour);
                    }
                }
                else
                {
                    text.setFillColour(TextHighlightColour);
                }
                
                text.draw();
                position.x += 49.f;
            }
            position.x = 2.f;
            position.y -= 12.f;
        }

        m_clubTexture.display();
    }


    //streak info
    labelEnt = m_uiScene.createEntity();
    labelEnt.addComponent<cro::Transform>().setPosition({ 22.f, 48.f, 0.1f });
    labelEnt.addComponent<cro::Drawable2D>();
    labelEnt.addComponent<cro::Text>(font).setString("Current Streak: " + std::to_string(Social::getCurrentStreak()) 
                                            + " Days\nLongest Streak: " + std::to_string(Social::getLongestStreak()) + " Days");
    labelEnt.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
    labelEnt.getComponent<cro::Text>().setFillColour(TextNormalColour);
    labelEnt.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    labelEnt.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);

    //*sigh* leaderboards are not necessarily up to date because of the time it takes to fetch
    //(we should really be using a stat) so we have to fudge this in here
#ifdef USE_GNS
    labelEnt.addComponent<cro::Callback>().active = true;
    labelEnt.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float)
    {
        if (m_currentMenu == MenuID::Avatar)
        {
            e.getComponent<cro::Text>().setString("Current Streak: " + std::to_string(Social::getCurrentStreak())
                                        + " Days\nLongest Streak: " + std::to_string(Social::getLongestStreak()) + " Days");
        }
    };
#endif

    parent.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
}

void MenuState::eraseCurrentProfile()
{
    auto profileID = m_sharedData.playerProfiles[m_sharedData.activeProfileIndex].profileID;

    //erase profile first so we know we have valid remaining
    m_sharedData.playerProfiles.erase(m_sharedData.playerProfiles.begin() + m_sharedData.activeProfileIndex);
    
    //set the active profile slot to the first valid profile
    //remember the erased profile could be assigned to multiple slots..
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerData.size(); ++i)
    {
        if (m_sharedData.localConnectionData.playerData[i].profileID == profileID)
        {
            m_rosterMenu.profileIndices[i] = 0;
            m_sharedData.localConnectionData.playerData[i] = m_sharedData.playerProfiles[0];
        }
    }

    m_profileTextures.erase(m_profileTextures.begin() + m_sharedData.activeProfileIndex);
    m_sharedData.activeProfileIndex = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];

    //refresh the preview / roster list
    updateRoster();


    //remove the data from disk
    auto path = Social::getUserContentPath(Social::UserContent::Profile);
    path += profileID;
    if (cro::FileSystem::directoryExists(path))
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}