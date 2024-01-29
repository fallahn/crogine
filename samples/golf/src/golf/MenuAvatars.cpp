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
#include "CommandIDs.hpp"
#include "PacketIDs.hpp"
#include "MenuConsts.hpp"
#include "CallbackData.hpp"
#include "Clubs.hpp"
#include "TextAnimCallback.hpp"
#include "MenuEnum.inl"
#include "../ErrorCheck.hpp"

#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Camera.hpp>
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

    bool firstLoad = true; //one of those rare occasions we want this to be static.

    struct DButton final
    {
        enum
        {
            CPU, PrevProf, Profile, NextProf,

            Count
        };
    };
    std::array<cro::Entity, DButton::Count> DynamicButtons = {};
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
        m_profileData.playerProfiles.push_back(m_sharedData.localConnectionData.playerData[0]);
        m_profileTextures.emplace_back(m_sharedData.avatarInfo[indexFromAvatarID(m_sharedData.localConnectionData.playerData[0].skinID)].texturePath);
        updateProfileTextures(0, m_profileTextures.size());
    }
    else
    {
        //see if we have a social name and put to the font of the profile list
        if (!socialName.empty())
        {
            auto res = std::find_if(m_profileData.playerProfiles.begin(), m_profileData.playerProfiles.end(),
                [&socialName](const PlayerData& pd)
                {
                    return pd.name == socialName;
                });

            if (res != m_profileData.playerProfiles.end())
            {
                auto pos = std::distance(m_profileData.playerProfiles.begin(), res);

                std::swap(m_profileData.playerProfiles[0], m_profileData.playerProfiles[pos]);

                //remember to realign the texture indices!
                std::swap(m_profileTextures[0], m_profileTextures[pos]);
            }
        }
    }
    
    //only use the default profile if this is first loaded
    //as it may have been changed for a game and we want to maintain
    //the player's roster throughout.
    if (firstLoad)
    {
        m_sharedData.localConnectionData.playerData[0] = m_profileData.playerProfiles[0];
        firstLoad = false;
    }

    //map all the active player slots to their profile index
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerCount; ++i)
    {
        const auto& uid = m_sharedData.localConnectionData.playerData[i].profileID;
        if (auto result = std::find_if(m_profileData.playerProfiles.begin(), m_profileData.playerProfiles.end(),
            [&uid](const PlayerData& pd) {return pd.profileID == uid;}); result != m_profileData.playerProfiles.end())
        {
            m_rosterMenu.profileIndices[i] = std::distance(m_profileData.playerProfiles.begin(), result);
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
        auto bounds = q.getComponent<cro::Sprite>().getTextureBounds() * q.getComponent<cro::Transform>().getScale().x;
        auto offset = q.getComponent<cro::Transform>().getPosition();
        
        auto cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, 0.f, 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_bl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        auto cornerBounds = cornerEnt.getComponent<cro::Sprite>().getTextureBounds();

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ 0.f, (bounds.height - cornerBounds.height), 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tl");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ (bounds.width - cornerBounds.width), (bounds.height - cornerBounds.height), 0.3f });
        cornerEnt.getComponent<cro::Transform>().move(glm::vec2(offset));
        cornerEnt.addComponent<cro::Drawable2D>();
        cornerEnt.addComponent<cro::Sprite>() = spriteSheet.getSprite("corner_tr");
        p.getComponent<cro::Transform>().addChild(cornerEnt.getComponent<cro::Transform>());

        cornerEnt = m_uiScene.createEntity();
        cornerEnt.addComponent<cro::Transform>().setPosition({ (bounds.width - cornerBounds.width), 0.f, 0.3f });
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
    entity.addComponent<cro::Transform>().setPosition({ 397.f, 22.f, 0.1f });
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
        auto& profile = m_profileData.playerProfiles[profileIndex];
        auto idx = indexFromAvatarID(profile.skinID);

        //TODO if the index is the same as the model already shown
        //don't play the animation.
        for (auto& e : m_playerAvatars)
        {
            if (e.previewModel != m_playerAvatars[idx].previewModel)
            {
                e.previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 1;
                e.previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().progress = 1.f;
                e.previewModel.getComponent<cro::Callback>().active = true;
            }
        }
        
        m_playerAvatars[idx].previewModel.getComponent<cro::Transform>().setScale(glm::vec3(profile.flipped ? -0.001f : 0.001f, 0.f, 0.f)); //callback needs to know which way to face
        m_playerAvatars[idx].previewModel.getComponent<cro::Model>().setFacing(profile.flipped ? cro::Model::Facing::Back : cro::Model::Facing::Front);
        
        if (m_playerAvatars[idx].previewModel.getComponent<cro::Transform>().getScale().x != 1)
        {
            m_playerAvatars[idx].previewModel.getComponent<cro::Callback>().active = true;
            m_playerAvatars[idx].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().direction = 0;
            m_playerAvatars[idx].previewModel.getComponent<cro::Callback>().getUserData<AvatarAnimCallbackData>().progress = 0.f;
        }



        //use profile ID to set model texture
        const auto& flags = profile.avatarFlags;
        for (auto i = 0u; i < flags.size(); ++i)
        {
            m_playerAvatars[idx].setColour(pc::ColourKey::Index(i), flags[i]);
        }
        m_playerAvatars[idx].apply(m_profileTextures[profileIndex].getTexture()); //lul, what a mess.

        m_playerAvatars[idx].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", m_profileTextures[profileIndex].getTextureID());
        m_playerAvatars[idx].previewModel.getComponent<cro::Model>().setHidden(false);
        

        //reload the mugshot data - TODO how do we do this only if
        //the data changed?
        if (!profile.mugshot.empty())
        {
            m_profileTextures[profileIndex].setMugshot(profile.mugshot);

            if (auto img = cropAvatarImage(profile.mugshot); img.getPixelData())
            {
                //profile.mugshotData = std::move(img);
                m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex].mugshotData = std::move(img);
            }
        }

        if (m_profileTextures[profileIndex].getMugshot())
        {
            glm::vec2 texSize(m_profileTextures[profileIndex].getMugshot()->getSize());
            glm::vec2 scale = glm::vec2(96.f, 48.f) / texSize;

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
            if (m_playerAvatars[idx].hairAttachment->getModel().isValid())
            {
                m_playerAvatars[idx].hairAttachment->getModel().getComponent<cro::Model>().setHidden(true);
            }

            auto hID = indexFromHairID(profile.hairID);
            auto& hair = m_playerAvatars[idx].hairModels[hID];
            if (hair.model.isValid())
            {
                hair.model.getComponent<cro::Model>().setHidden(false);
                m_playerAvatars[idx].hairAttachment->setModel(hair.model);

                //apply hair colour to material
                auto hairColour = m_playerAvatars[idx].getColour(pc::ColourKey::Hair);
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
        m_rosterMenu.buttonEntities[0].getComponent<cro::UIInput>().setNextIndex(Player02, Player02);

        auto str = m_sharedData.localConnectionData.playerData[0].name;
        for (auto i = 1u; i < m_sharedData.localConnectionData.playerCount; ++i)
        {
            str += "\n" + m_sharedData.localConnectionData.playerData[i].name;

            m_rosterMenu.buttonEntities[i].getComponent<cro::UIInput>().enabled = true;
            m_rosterMenu.buttonEntities[i].getComponent<cro::UIInput>().setNextIndex(Player01 + i + 1, Player01 + i + 1);
        }
        m_rosterMenu.buttonEntities[m_sharedData.localConnectionData.playerCount - 1].getComponent<cro::UIInput>().setNextIndex(PlayerProfile, PlayerProfile);
        
        std::int32_t prevIndex = m_sharedData.localConnectionData.playerCount == 1 ? PlayerAdd : Player01 + (m_sharedData.localConnectionData.playerCount - 1);

        //we need to set the prev target of CPU, prefProf profile and nextProf to whichever button was last enabled
        DynamicButtons[DButton::CPU].getComponent<cro::UIInput>().setPrevIndex(PlayerLeaveMenu, prevIndex);
        DynamicButtons[DButton::PrevProf].getComponent<cro::UIInput>().setPrevIndex(PlayerCPU, prevIndex);
        DynamicButtons[DButton::Profile].getComponent<cro::UIInput>().setPrevIndex(PlayerPrevProf, prevIndex);
        DynamicButtons[DButton::NextProf].getComponent<cro::UIInput>().setPrevIndex(PlayerProfile, prevIndex);
        
        rosterEnt.getComponent<cro::Text>().setString(str);

        nameLabel.getComponent<cro::Text>().setString(m_profileData.playerProfiles[m_rosterMenu.profileIndices[m_rosterMenu.activeIndex]].name);
        centreText(nameLabel);

        auto idx = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
        m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = indexFromBallID(m_profileData.playerProfiles[idx].ballID);

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

                nameLabel.getComponent<cro::Text>().setString(m_profileData.playerProfiles[m_rosterMenu.profileIndices[m_rosterMenu.activeIndex]].name);
                centreText(nameLabel);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                auto idx = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                m_ballCam.getComponent<cro::Callback>().getUserData<std::int32_t>() = indexFromBallID(m_profileData.playerProfiles[idx].ballID);

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
        entity.getComponent<cro::UIInput>().setSelectionIndex(Player01 + i);
        entity.getComponent<cro::UIInput>().setNextIndex(Player01 + i + 1, Player01 + i + 1);
        entity.getComponent<cro::UIInput>().setPrevIndex(Player01 + i - 1, Player01 + i - 1);
        entity.getComponent<cro::UIInput>().enabled = false;

        bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
        entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());

        m_rosterMenu.selectionEntities[i].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
        m_rosterMenu.buttonEntities[i] = entity;
    }
    m_rosterMenu.buttonEntities[0].getComponent<cro::UIInput>().setPrevIndex(PlayerAdd, PlayerAdd);


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

    auto hideTT = uiSystem.addCallback([&](cro::Entity e, glm::vec2, const cro::MotionEvent&)
        {
            hideToolTip();
        });

    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({36.f, 104.f, 0.1f});
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::CPUHighlight];
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::UIInput>().area = m_sprites[SpriteID::CPUHighlight].getTextureBounds();
    entity.getComponent<cro::UIInput>().area.width *= 3.4f; //cover writing too
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerCPU);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerPrevProf, PlayerAdd);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerLeaveMenu, Player01);

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
                m_profileData.playerProfiles[m_rosterMenu.profileIndices[index]].isCPU = isCPU;
                m_profileData.playerProfiles[m_rosterMenu.profileIndices[m_rosterMenu.activeIndex]].saveProfile();

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
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Exit] = hideTT;
    DynamicButtons[DButton::CPU] = entity;
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
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerPrevProf);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerProfile, PlayerAdd);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerCPU, Player01);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                auto i = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                i = (i + (m_profileData.playerProfiles.size() - 1)) % m_profileData.playerProfiles.size();
                setProfileIndex(i);
            }
        });
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    DynamicButtons[DButton::PrevProf] = entity;


    //profile flyout menu
    auto& nameFont = m_sharedData.sharedResources->fonts.get(FontID::Label);
    entity = m_uiScene.createEntity(); //text
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Text>(nameFont).setCharacterSize(LabelTextSize);
    entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
    entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
    entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
    entity.getComponent<cro::Text>().setVerticalSpacing(1.f);
    m_profileFlyout.detail = entity;


    entity = m_uiScene.createEntity(); //bg
    entity.addComponent<cro::Transform>().setPosition({ 130.f, 120.f, 0.6f });
    entity.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.getComponent<cro::Transform>().addChild(m_profileFlyout.detail.getComponent<cro::Transform>());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_menuEntities[MenuID::ProfileFlyout] = entity;
    m_profileFlyout.background = entity;

    entity = m_uiScene.createEntity(); //highlight
    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    m_menuEntities[MenuID::ProfileFlyout].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    m_profileFlyout.highlight = entity;

    m_profileFlyout.selectCallback = uiSystem.addCallback([&, entity](cro::Entity e) mutable
        {
            const auto screenY = static_cast<float>(cro::App::getWindow().getSize().y);
            auto worldY = e.getComponent<cro::Transform>().getWorldPosition().y;
            const float itemSize = (ProfileItemHeight * m_viewScale.y);
            if (worldY + itemSize > screenY)
            {
                m_menuEntities[MenuID::ProfileFlyout].getComponent<cro::Transform>().move({ 0.f, -ProfileItemHeight * std::ceil(((worldY + itemSize) - screenY) / itemSize) });
            }
            else if (worldY < 0)
            {
                m_menuEntities[MenuID::ProfileFlyout].getComponent<cro::Transform>().move({ 0.f, -ProfileItemHeight * std::ceil(worldY / itemSize) });
            }

            entity.getComponent<cro::Transform>().setPosition(e.getComponent<cro::Transform>().getPosition());
            entity.getComponent<cro::Callback>().function = MenuTextCallback(); //hm is there a less horrible way to reset this?
            entity.getComponent<cro::Callback>().active = true;
            entity.getComponent<cro::AudioEmitter>().setPlayingOffset(cro::Time());
            entity.getComponent<cro::AudioEmitter>().play();
        });

    m_profileFlyout.activateCallback = uiSystem.addCallback([&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            //wheel within a wheel
            auto closePopup = [&]() mutable
            {
                m_menuEntities[MenuID::ProfileFlyout].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::Avatar);
                m_currentMenu = MenuID::Avatar;
            };

            if (activated(evt))
            {
                closePopup();
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();

                setProfileIndex(e.getComponent<cro::UIInput>().getSelectionIndex() - 1); //indices are base 1, not 0
            }

            else if (deactivated(evt))
            {
                closePopup();
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });
    
    refreshProfileFlyout();


    //profile flyout button
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 107.f, 101.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_flyout");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerProfile);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerNextProf, PlayerAdd);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerPrevProf, Player01);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_menuEntities[MenuID::ProfileFlyout].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                m_uiScene.getSystem<cro::UISystem>()->setActiveGroup(MenuID::ProfileFlyout);
                m_currentMenu = MenuID::ProfileFlyout;
            }
        });
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    DynamicButtons[DButton::Profile] = entity;

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
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerNextProf);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerReset, PlayerReset);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerProfile, Player01);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_courseSelectCallbacks.selected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = m_courseSelectCallbacks.unselected;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt) mutable
        {
            if (activated(evt))
            {
                auto i = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                i = (i + 1) % m_profileData.playerProfiles.size();
                setProfileIndex(i);
            }
        });
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
    DynamicButtons[DButton::NextProf] = entity;


    //reset stats
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 158.f, 25.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("reset_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerReset);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerEdit, PlayerEdit);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerNextProf, PlayerNextProf);
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
    entity.addComponent<cro::Transform>().setPosition({ 265.f, 53.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_edit_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerCreate);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerEdit, PlayerEdit);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerReset, PlayerRemove);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                if (m_profileData.playerProfiles.size() < ConstVal::MaxProfiles)
                {
                    auto& profile = m_profileData.playerProfiles.emplace_back();
                    profile.name = RandomNames[cro::Util::Random::value(0u, RandomNames.size() - 1)];
                    for (auto i = 0u; i < profile.avatarFlags.size(); ++i)
                    {
                        profile.avatarFlags[i] = static_cast<std::uint8_t>(cro::Util::Random::value(0u, pc::PairCounts[i] - 1));
                    }
                    profile.skinID = m_sharedData.avatarInfo[cro::Util::Random::value(0u, m_sharedData.avatarInfo.size() - 1)].uid;
                    profile.ballID = m_sharedData.ballInfo[cro::Util::Random::value(0u, m_sharedData.ballInfo.size() - 1)].uid;
                    profile.hairID = m_sharedData.hairInfo[cro::Util::Random::value(0u, m_sharedData.hairInfo.size() - 1)].uid;
                    profile.flipped = cro::Util::Random::value(0, 1) == 0 ? false : true;
                    profile.saveProfile();
                    m_profileData.activeProfileIndex = m_profileData.playerProfiles.size() - 1;

                    //create profile texture
                    auto avtIdx = indexFromAvatarID(profile.skinID);
                    m_profileTextures.emplace_back(m_sharedData.avatarInfo[avtIdx].texturePath);
                    updateProfileTextures(m_profileTextures.size() - 1, 1);

                    //set selected roster slot to this profile and refresh view
                    m_rosterMenu.profileIndices[m_rosterMenu.activeIndex] = m_profileData.activeProfileIndex;
                    m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex] = m_profileData.playerProfiles[m_profileData.activeProfileIndex];

                    updateRoster();
                    refreshProfileFlyout();

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
    entity.addComponent<cro::Transform>().setPosition({ 265.f, 36.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_edit_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerEdit);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerDelete, PlayerDelete);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerReset, PlayerCreate);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_profileData.activeProfileIndex = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                requestStackPush(StateID::Profile);
            }
        });
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    entity.getComponent<cro::Transform>().setOrigin({ bounds.width / 2.f, bounds.height / 2.f });
    entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
    avatarEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

    //delete profile
    entity = m_uiScene.createEntity();
    entity.addComponent<cro::Transform>().setPosition({ 265.f, 19.f, 0.1f });
    entity.addComponent<cro::AudioEmitter>() = m_menuSounds.getEmitter("switch");
    entity.addComponent<cro::Drawable2D>();
    entity.addComponent<cro::Sprite>() = spriteSheet.getSprite("profile_edit_select");
    entity.getComponent<cro::Sprite>().setColour(cro::Colour::Transparent);
    entity.addComponent<cro::Callback>().function = MenuTextCallback();
    bounds = entity.getComponent<cro::Sprite>().getTextureBounds();
    bounds.left += 2.f;
    bounds.bottom += 2.f;
    bounds.width -= 4.f;
    bounds.height -= 4.f;
    entity.addComponent<cro::UIInput>().area = bounds;
    entity.getComponent<cro::UIInput>().setGroup(MenuID::Avatar);
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerDelete);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerRemove, PlayerRemove);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerReset, PlayerEdit);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = selectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Unselected] = unselectionCallback;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = uiSystem.addCallback(
        [&](cro::Entity e, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                if (m_profileData.playerProfiles.size() == 1)
                {
                    m_sharedData.errorMessage = "This Profile\nCannot Be Deleted";
                }
                else
                {
                    m_sharedData.errorMessage = "delete_profile";
                    m_profileData.activeProfileIndex = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
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
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerLeaveMenu);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerAdd, Player01);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerNextMenu, PlayerCPU);
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
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerAdd);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerRemove, Player01);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerLeaveMenu, PlayerProfile);
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = mouseEnterCursor;
    entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] =
        m_uiScene.getSystem<cro::UISystem>()->addCallback(
            [&](cro::Entity, const cro::ButtonEvent& evt) mutable
            {
                if (activated(evt))
                {
                    if (m_sharedData.localConnectionData.playerCount < ConstVal::MaxPlayers)
                    {
                        auto prevProfile = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];
                        
                        m_rosterMenu.activeIndex = m_sharedData.localConnectionData.playerCount;
                        auto profileIndex = (prevProfile + 1) % m_profileData.playerProfiles.size();
                        
                        m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex] = m_profileData.playerProfiles[profileIndex];
                        m_sharedData.localConnectionData.playerCount++;
                        
                        m_rosterMenu.profileIndices[m_rosterMenu.activeIndex] = profileIndex;

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
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerRemove);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerNextMenu, PlayerCreate);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerAdd, PlayerDelete);
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
    entity.getComponent<cro::UIInput>().setSelectionIndex(PlayerNextMenu);
    entity.getComponent<cro::UIInput>().setNextIndex(PlayerLeaveMenu, PlayerLeaveMenu);
    entity.getComponent<cro::UIInput>().setPrevIndex(PlayerRemove, PlayerRemove);
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
                            m_sharedData.serverInstance.launch(ConstVal::MaxClients, Server::GameMode::Golf, m_sharedData.fastCPU);

#ifndef USE_GNS
                            //small delay for server to get ready
                            cro::Clock clock;
                            while (clock.elapsed().asMilliseconds() < 500) {}
#endif
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
                m_sharedData.gimmeRadius = (m_sharedData.gimmeRadius + (GimmeSize::Count - 1)) % GimmeSize::Count;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::GimmeRadius, m_sharedData.gimmeRadius, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
        });
    m_courseSelectCallbacks.nextRadius = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.gimmeRadius = (m_sharedData.gimmeRadius + 1) % GimmeSize::Count;
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
                    m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //refresh view
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
                    m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true; //refresh view
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

    m_courseSelectCallbacks.toggleClubLimit = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.clubLimit = m_sharedData.clubLimit == 0 ? 1 : 0;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::ClubLimit, m_sharedData.clubLimit, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_courseSelectCallbacks.toggleNightTime = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                m_sharedData.nightTime = m_sharedData.nightTime == 0 ? 1 : 0;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::NightTime, m_sharedData.nightTime, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
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

    m_courseSelectCallbacks.setWeather = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                std::uint8_t weatherType = (m_sharedData.weatherType + 1) % WeatherType::Count;
                m_sharedData.clientConnection.netClient.sendPacket(PacketID::WeatherType, weatherType, net::NetFlag::Reliable, ConstVal::NetChannelReliable);
                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
            }
        });

    m_courseSelectCallbacks.toggleFastCPU = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt))
            {
                //we rely on the server reply to actually set shared data value
                m_sharedData.clientConnection.netClient.sendPacket<std::uint8_t>(PacketID::FastCPU, m_sharedData.fastCPU ? 0 : 1, net::NetFlag::Reliable, ConstVal::NetChannelReliable);

                m_audioEnts[AudioID::Back].getComponent<cro::AudioEmitter>().play();
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

    m_courseSelectCallbacks.selectPM = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity e)
        {
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour = TextGoldColour;
            }
            e.getComponent<cro::AudioEmitter>().play();
            //e.getComponent<cro::Callback>().active = true;

            //set prev/next indices based on which sub-menu is active
            const auto holeScale = m_lobbyWindowEntities[LobbyEntityID::HoleSelection].getComponent<cro::Transform>().getScale().y;
            const auto infoScale = m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().getScale().y;

            if (holeScale + infoScale == 0)
            {
                //we're on the rules tab
                e.getComponent<cro::UIInput>().setNextIndex(LobbyCourseA, LobbyQuit);
                e.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoA, LobbyQuit);
            }
            else
            {
                if (holeScale == 0)
                {
                    //we're on info
                    e.getComponent<cro::UIInput>().setNextIndex(LobbyCourseB, LobbyQuit);
                    e.getComponent<cro::UIInput>().setPrevIndex(LobbyRulesB, LobbyQuit);
                }
                else
                {
                    //we're on course tab
                    e.getComponent<cro::UIInput>().setNextIndex(LobbyRulesA, LobbyQuit);
                    e.getComponent<cro::UIInput>().setPrevIndex(LobbyInfoB, LobbyQuit);
                }
            }
        });
    m_courseSelectCallbacks.unselectPM = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [](cro::Entity e)
        {
            for (auto& v : e.getComponent<cro::Drawable2D>().getVertexData())
            {
                v.colour = cro::Colour::Transparent;
            }
        });
    m_courseSelectCallbacks.activatePM = m_uiScene.getSystem<cro::UISystem>()->addCallback(
        [&](cro::Entity, const cro::ButtonEvent& evt)
        {
            if (activated(evt)
                && m_sharedData.hosting)
            {
                requestStackPush(StateID::PlayerManagement);
                m_audioEnts[AudioID::Accept].getComponent<cro::AudioEmitter>().play();
            }
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

    m_sharedData.preferredClubSet = std::min(m_sharedData.preferredClubSet, Social::getClubLevel());


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
            auto current = Social::getCurrentStreak();
            auto longest = std::max(current, Social::getLongestStreak());

            e.getComponent<cro::Text>().setString("Current Streak: " + std::to_string(current)
                                        + " Days\nLongest Streak: " + std::to_string(longest) + " Days");
        }
    };
#endif

    parent.getComponent<cro::Transform>().addChild(labelEnt.getComponent<cro::Transform>());
}

void MenuState::eraseCurrentProfile()
{
    auto profileID = m_profileData.playerProfiles[m_profileData.activeProfileIndex].profileID;

    //assign a valid texture to the preview model
    auto idx = indexFromAvatarID(m_profileData.playerProfiles[m_profileData.activeProfileIndex].skinID);
    m_playerAvatars[idx].previewModel.getComponent<cro::Model>().setMaterialProperty(0, "u_diffuseMap", m_playerAvatars[idx].getTextureID());

    //before we delete the old one...
    m_profileTextures.erase(m_profileTextures.begin() + m_profileData.activeProfileIndex);


    //erase profile first so we know we have valid remaining
    m_profileData.playerProfiles.erase(m_profileData.playerProfiles.begin() + m_profileData.activeProfileIndex);
    
    //set the active profile slot to the first valid profile
    //remember the erased profile could be assigned to multiple slots..
    for (auto i = 0u; i < m_sharedData.localConnectionData.playerData.size(); ++i)
    {
        if (m_sharedData.localConnectionData.playerData[i].profileID == profileID)
        {
            m_rosterMenu.profileIndices[i] = 0;
            m_sharedData.localConnectionData.playerData[i] = m_profileData.playerProfiles[0];
        }
    }

    m_profileData.activeProfileIndex = m_rosterMenu.profileIndices[m_rosterMenu.activeIndex];

    //refresh the preview / roster list
    updateRoster();
    refreshProfileFlyout();

    //remove the data from disk
    auto path = Social::getUserContentPath(Social::UserContent::Profile);
    path += profileID;
    if (cro::FileSystem::directoryExists(path))
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
}

void MenuState::setProfileIndex(std::size_t i)
{
    m_rosterMenu.profileIndices[m_rosterMenu.activeIndex] = i;

    m_sharedData.localConnectionData.playerData[m_rosterMenu.activeIndex] = m_profileData.playerProfiles[i];

    updateRoster();

    auto avtIdx = indexFromAvatarID(m_profileData.playerProfiles[i].skinID);
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

void MenuState::refreshProfileFlyout()
{
    //name list
    cro::String nameList;
    for (const auto& profile : m_profileData.playerProfiles)
    {
        nameList += profile.name + "\n";
    }

    m_profileFlyout.detail.getComponent<cro::Text>().setString(nameList);
    auto bounds = cro::Text::getLocalBounds(m_profileFlyout.detail);
    bounds.height = ProfileItemHeight * m_profileData.playerProfiles.size();
    m_profileFlyout.detail.getComponent<cro::Transform>().setPosition({ 0.f, bounds.height - 1.f, 0.1f });


    //resize background
    const float menuWidth = std::max(bounds.width, 120.f);
    m_profileFlyout.background.getComponent<cro::Drawable2D>().setVertexData(createMenuBackground({ menuWidth, bounds.height + 2.f }));


    //resize highlight
    m_profileFlyout.highlight.getComponent<cro::Transform>().setOrigin({ menuWidth / 2.f, ProfileItemHeight / 2.f });
    m_profileFlyout.highlight.getComponent<cro::Drawable2D>().setVertexData(createMenuHighlight({ menuWidth, ProfileItemHeight }, TextGoldColour));

    //clear old items and add new
    for (auto e : m_profileFlyout.items)
    {
        m_uiScene.destroyEntity(e);
    }
    m_profileFlyout.items.clear();


    const std::size_t nameCount = m_profileData.playerProfiles.size();
    for (auto i = 0u; i < nameCount; ++i)
    {
        auto entity = m_uiScene.createEntity();
        entity.addComponent<cro::Transform>().setPosition({ 0.f, (i * ProfileItemHeight), 0.2f });
        entity.addComponent<cro::UIInput>().area = { 0.f, 0.f, menuWidth, ProfileItemHeight };
        entity.getComponent<cro::UIInput>().setGroup(MenuID::ProfileFlyout);
        entity.getComponent<cro::UIInput>().setSelectionIndex(((nameCount - 1) - i) + 1); //hm, indices of 0 are automatically reassigned by UISystem...
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::Selected] = m_profileFlyout.selectCallback;
        entity.getComponent<cro::UIInput>().callbacks[cro::UIInput::ButtonUp] = m_profileFlyout.activateCallback;

        entity.getComponent<cro::Transform>().setOrigin({ menuWidth / 2.f, ProfileItemHeight / 2.f });
        entity.getComponent<cro::Transform>().move(entity.getComponent<cro::Transform>().getOrigin());
        m_profileFlyout.background.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());

        m_profileFlyout.items.push_back(entity);
    }
}

void MenuState::updateLobbyAvatars()
{
    //TODO we need to refine this so only textures of the most
    //recently updated client are updated

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

        const auto applyTexture = [&](std::size_t idx, cro::Texture& targetTexture, const std::array<uint8_t, pc::ColourKey::Count>& flags)
        {
            for (auto j = 0u; j < flags.size(); ++j)
            {
                m_playerAvatars[idx].setColour(pc::ColourKey::Index(j), flags[j]);
            }
            m_playerAvatars[idx].apply(&targetTexture);
        };
        const auto& font = m_sharedData.sharedResources->fonts.get(FontID::Label);
        cro::SimpleText simpleText(font);
        simpleText.setCharacterSize(LabelTextSize);
        simpleText.setFillColour(TextNormalColour);
        simpleText.setShadowOffset({ 1.f, -1.f });
        simpleText.setShadowColour(LeaderboardTextDark);
        //simpleText.setBold(true);

        cro::Image img;
        img.create(1, 1, cro::Colour::White);
        cro::Texture quadTexture;
        quadTexture.loadFromImage(img);
        cro::SimpleQuad simpleQuad(quadTexture);
        simpleQuad.setBlendMode(cro::Material::BlendMode::None);
        simpleQuad.setColour(cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha / 2.f));

        cro::Texture iconTexture;
        cro::Image iconImage;

        std::int32_t h = 0;
        std::int32_t clientCount = 0;
        std::int32_t playerCount = 0;
        glm::vec2 textureSize(LabelTextureSize);
        textureSize.y -= LabelIconSize.y * 4.f;

        auto& largeFont = m_sharedData.sharedResources->fonts.get(FontID::UI);
        auto& smallFont = m_sharedData.sharedResources->fonts.get(FontID::Info);

        cro::String nameString;

        constexpr float RowSpacing = -14.f;
        for (const auto& c : m_sharedData.connectionData)
        {
            //update the name label texture
            if (c.connectionID < m_sharedData.nameTextures.size())
            {
                m_sharedData.nameTextures[c.connectionID].clear(cro::Colour::Transparent);

                simpleQuad.setTexture(quadTexture);
                simpleQuad.setColour(cro::Colour(0.f, 0.f, 0.f, BackgroundAlpha / 2.f));

                for (auto i = 0u; i < c.playerCount; ++i)
                {
                    simpleText.setString(c.playerData[i].name);
                    auto bounds = simpleText.getLocalBounds();
                    simpleText.setPosition({ std::round((textureSize.x - bounds.width) / 2.f), (i * (textureSize.y / ConstVal::MaxPlayers)) + 4.f });

                    simpleQuad.setPosition({ simpleText.getPosition().x - 2.f,(i * (textureSize.y / ConstVal::MaxPlayers)) });
                    simpleQuad.setScale({ (bounds.width + 5.f), 14.f });
                    simpleQuad.draw();
                    simpleText.draw();
                }

                //check profile for headshot first
                iconImage = Social::getUserIcon(c.peerID);
                for (auto i = 0u; i < c.playerCount; ++i)
                {
                    bool hasIcon = false;
                    glm::vec2 iconScale(1.f);

                    if (c.connectionID == m_sharedData.clientConnection.connectionID
                        && !m_sharedData.localConnectionData.playerData[i].mugshotData.empty())
                    {
                        auto dims = m_sharedData.localConnectionData.playerData[i].mugshotData.getDimensions();
                        iconScale = glm::vec2(Social::IconSize) / glm::vec2(dims);
                        iconTexture.create(dims.x, dims.y);
                        iconTexture.update(m_sharedData.localConnectionData.playerData[i].mugshotData.data(), false);
                        iconTexture.setSmooth(true);

                        hasIcon = true;
                    }
                    else if (iconImage.getPixelData())
                    {
                        iconTexture.loadFromImage(iconImage);
                        iconTexture.setSmooth(true);
                        auto dims = iconImage.getSize();
                        iconScale = glm::vec2(Social::IconSize) / glm::vec2(dims);
                        hasIcon = true;
                    }

                    if (hasIcon)
                    {
                        simpleQuad.setTexture(iconTexture);
                        simpleQuad.setScale(iconScale);
                        simpleQuad.setPosition({ (i % 2) * Social::IconSize, textureSize.y + ((i/2) * Social::IconSize) });
                        simpleQuad.setColour(cro::Colour::White);
                        simpleQuad.draw();
                    }
                }

                m_sharedData.nameTextures[c.connectionID].display();
            }

            glm::vec2 iconPos(1.f, 0.f);
            const std::int32_t row = playerCount;
            iconPos.y = row * RowSpacing;

            //add list of names on the connected client
            for (auto i = 0u; i < c.playerCount; ++i)
            {
                //stringCols[playerCount / ConstVal::MaxPlayers] += "buns\n";

                auto avatarIndex = indexFromAvatarID(c.playerData[i].skinID);
                applyTexture(avatarIndex, m_sharedData.avatarTextures[c.connectionID][i], c.playerData[i].avatarFlags);
                nameString += c.playerData[i].name.substr(0, ConstVal::MaxStringChars) + "\n";

                playerCount++;
            }

            if (c.playerCount != 0)
            {
                //add a ready status for that client
                auto entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ -11.f, iconPos.y - 7.f });
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::ReadyStatus];
                entity.addComponent<cro::SpriteAnimation>();
                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().function =
                    [&, h](cro::Entity e2, float)
                {
                    auto index = m_readyState[h] ? 1 : 0;
                    e2.getComponent<cro::SpriteAnimation>().play(index);
                };
                e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);


                //rank text
                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ 100.f, (RowSpacing * clientCount) + 180.f, 0.3f });
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Text>(smallFont).setString("Level");
                entity.getComponent<cro::Text>().setCharacterSize(InfoTextSize);
                entity.getComponent<cro::Text>().setFillColour(TextNormalColour);
                entity.getComponent<cro::Text>().setShadowColour(LeaderboardTextDark);
                entity.getComponent<cro::Text>().setShadowOffset({ 1.f, -1.f });
                entity.getComponent<cro::Text>().setString("Level " + std::to_string(m_sharedData.connectionData[h].level));

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
                        std::string str = "Level " + std::to_string(m_sharedData.connectionData[h].level);
                        str += "               " + std::to_string(m_sharedData.connectionData[h].playerCount) + " player(s)";
                        ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        ent.getComponent<cro::Text>().setString(str);

                        //float offset = ent.getComponent<cro::Callback>().getUserData<float>();
                        //ent.getComponent<cro::Transform>().setPosition({ std::floor((56.f + (m_lobbyExpansion / 2.f)) - offset), -160.f, 0.1f });
                    }
                };
                m_lobbyWindowEntities[LobbyEntityID::Info].getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);
                auto rankEnt = entity;

                //add a rank badge
                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ -18.f, -12.f, 0.15f });
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Sprite>() = m_sprites[SpriteID::LevelBadge];
                entity.addComponent<cro::SpriteAnimation>();

                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().function =
                    [&, h](cro::Entity ent, float)
                {
                    if (m_sharedData.connectionData[h].playerCount == 0
                        || m_sharedData.connectionData[h].level == 0)
                    {
                        ent.getComponent<cro::Transform>().setScale(glm::vec2(0.f));
                    }
                    else
                    {
                        ent.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                        auto index = std::min(5, m_sharedData.connectionData[h].level / 10);
                        ent.getComponent<cro::SpriteAnimation>().play(index);
                    }
                };
                rankEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);

                //add an avatar icon
                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition({ -98.f, -11.f, 0.15f });
                entity.getComponent<cro::Transform>().setScale({ 0.2f, 0.2f });
                entity.addComponent<cro::Drawable2D>();
                entity.addComponent<cro::Sprite>(m_sharedData.nameTextures[h].getTexture());
                cro::FloatRect bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.y * 4.f), LabelIconSize.x, LabelIconSize.y };
                entity.getComponent<cro::Sprite>().setTextureRect(bounds);
                entity.addComponent<cro::SpriteAnimation>();
                rankEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);

                //add a network icon
                entity = m_uiScene.createEntity();
                entity.addComponent<cro::Transform>().setPosition(glm::vec3(-82.f, -12.f, 0.1f));
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
                        auto index = std::min(4u, m_sharedData.connectionData[h].pingTime / 60);
                        ent.getComponent<cro::SpriteAnimation>().play(index);
                    }
                };
                entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::LobbyText;
                rankEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                children.push_back(entity);


                //if this is our local client then add the current xp level
                if (h == m_sharedData.clientConnection.connectionID)
                {
                    //level progress
                    constexpr float BarWidth = 80.f;
                    constexpr float BarHeight = 10.f;

                    entity = m_uiScene.createEntity();
                    entity.addComponent<cro::Transform>().setPosition({ (BarWidth / 2.f) - 2.f, -4.f, -0.15f });

                    const auto CornerColour = cro::Colour(std::uint8_t(58), 57, 65); //grey
                    //const auto CornerColour = cro::Colour(std::uint8_t(152), 122, 104); //beige

                    const auto progress = Social::getLevelProgress();
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
                            cro::Vertex2D(glm::vec2(BarWidth / 2.f, -BarHeight / 2.f), CornerColour),
                        });
                    entity.getComponent<cro::Drawable2D>().setPrimitiveType(GL_TRIANGLES);
                    rankEnt.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
                    children.push_back(entity);

                    //rankEnt.getComponent<cro::Callback>().setUserData<float>((BarWidth / 2.f) - 7.f);
                }
                //no need to do this now as we don't dynamically re-align
                //else
                //{
                //    //text width
                //    //auto bounds = cro::Text::getLocalBounds(rankEnt);
                //    //rankEnt.getComponent<cro::Callback>().setUserData<float>((bounds.width / 2.f) + 8.f);
                //}
                clientCount++;
            }
            h++;
        }

        //create text for name list
        if (!nameString.empty())
        {
            auto entity = m_uiScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(glm::vec3(1.f, 0.f, 0.2f));
            entity.addComponent<cro::Drawable2D>();
            entity.addComponent<cro::Text>(largeFont).setString(nameString);
            entity.getComponent<cro::Text>().setFillColour(LeaderboardTextDark);
            entity.getComponent<cro::Text>().setCharacterSize(UITextSize);
            entity.getComponent<cro::Text>().setVerticalSpacing(6.f);

            //used to update spacing by resize callback from lobby background ent.
            //TODO is this still used?
            entity.addComponent<cro::CommandTarget>().ID = CommandID::Menu::LobbyText;
            e.getComponent<cro::Transform>().addChild(entity.getComponent<cro::Transform>());
            children.push_back(entity);
        }


        auto strClientCount = std::to_string(clientCount);
        auto strGameType = std::to_string(ConstVal::MaxClients) + " - " + ScoreTypes[m_sharedData.scoreType];

        Social::setStatus(Social::InfoID::Lobby, { "Golf", strClientCount.c_str(), strGameType.c_str() });
        Social::setGroup(/*m_sharedData.lobbyID*/m_sharedData.clientConnection.hostID, playerCount);
        //LogI << "Set group data to " << m_sharedData.clientConnection.hostID << ", " << playerCount << std::endl;

        m_connectedClientCount = clientCount;
        m_connectedPlayerCount = playerCount;
        if (m_connectedPlayerCount < ScoreType::PlayerCount[m_sharedData.scoreType])
        {
            m_lobbyWindowEntities[LobbyEntityID::MinPlayerCount].getComponent<cro::Transform>().setScale(glm::vec2(1.f));
        }
        else
        {
            m_lobbyWindowEntities[LobbyEntityID::MinPlayerCount].getComponent<cro::Transform>().setScale(glm::vec2(0.f));
        }
        m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;


        //delayed entities to refresh/redraw the UI
        auto temp = m_uiScene.createEntity();
        temp.addComponent<cro::Callback>().active = true;
        temp.getComponent<cro::Callback>().function = [&](cro::Entity e, float)
        {
            refreshUI();
            e.getComponent<cro::Callback>().active = false;
            m_uiScene.destroyEntity(e);

            auto f = m_uiScene.createEntity();
            f.addComponent<cro::Callback>().active = true;
            f.getComponent<cro::Callback>().function =
                [&](cro::Entity g, float)
            {
                m_uiScene.getActiveCamera().getComponent<cro::Camera>().active = true;
                g.getComponent<cro::Callback>().active = false;
                m_uiScene.destroyEntity(g);
            };
        };
    };
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);


    //show or hide the chat hint depending on number of clients
    cmd.targetFlags = CommandID::Menu::ChatHint;
    if (m_sharedData.hosting)
    {
        //hide the chat hint if we're the only connection
        //this assumes the above command has already updated the client count
        cmd.action = [&](cro::Entity e, float)
            {
                if (e.hasComponent<cro::Text>())
                {
                    glm::vec2 scale(m_connectedClientCount > 1 ? 1.f : 0.f);
                    e.getComponent<cro::Transform>().setScale(scale);
                }
            };
    }
    else
    {
        //assume there are multiple players, else how would we be here?
        cmd.action = [](cro::Entity e, float)
            {
                if (e.hasComponent<cro::Text>())
                {
                    e.getComponent<cro::Transform>().setScale(glm::vec2(1.f));
                }
            };
    }
    m_uiScene.getSystem<cro::CommandSystem>()->sendCommand(cmd);
}
