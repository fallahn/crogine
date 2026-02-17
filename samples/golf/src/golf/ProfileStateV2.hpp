/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
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

#pragma once

#include "../StateIDs.hpp"
#include "ui/MenuLayout.hpp"
#include "SharedProfileData.hpp"
#include "CommonConsts.hpp"

#include <crogine/core/Clock.hpp>
#include <crogine/core/State.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Sprite.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

#include <vector>

struct SharedStateData;

class ProfileStateV2 final : public cro::State, public cro::GuiClient
{
public:
    ProfileStateV2(cro::StateStack&, cro::State::Context, SharedStateData&, SharedProfileData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Profile; }

private:

    cro::Scene m_scene;
    cro::Scene m_previewScene;
    cro::Scene m_statScene;
    SharedStateData& m_sharedData;
    SharedProfileData& m_profileData;
    SharedProfileData::LocalProfile m_activeProfile;

    float m_exitHoldTimer;
    std::uint8_t m_exitFlags;
    static constexpr std::uint8_t ExitFlagSave = 0x1;
    static constexpr std::uint8_t ExitFlagQuit = 0x2;
    static constexpr std::uint8_t ExitFlagRandomise = 0x4;

    cro::Shader m_progressShader;
    std::int32_t m_progressUniform;

    cro::Entity m_rootNode;

    cro::RenderTexture m_previewTexture;
    cro::ResourceCollection m_resources;
    std::int32_t m_avatarIndex;
    std::int32_t m_lockedAvatarCount;
    std::vector<AvatarPreview> m_avatarModels;
    std::vector<ProfileTexture> m_profileTextures;
    std::vector<cro::Entity> m_avatarHairModels;
    std::vector<BallPreview> m_ballModels;
    std::int32_t m_ballIndex;
    std::int32_t m_lockedBallCount;
    std::array<cro::Entity, 3u> m_ballParticles = {};
    std::size_t m_particleIndex;

    std::vector<ClubData> m_clubData;
    std::int32_t m_lockedClubCount;
    cro::RenderTexture m_clubTexture;
    std::int32_t m_clubIndex;

    std::string m_nameBuffer;
    bool m_showNameInput;

    struct VoiceData final
    {
        cro::AudioScape audioScape;
        bool isWorkshop = false;
    };
    std::vector<VoiceData> m_voices;
    std::int32_t m_voiceIndex;

    bool m_saveMugshotOnExit;
    cro::RenderTexture m_mugshotTexture;
    cro::Shader m_mugshotShader;

    void loadAssets();
    void buildScene();

    struct PreviewCamera final
    {
        enum
        {
            Avatar, Ball, //Club, //clubs have thumbnails on disk
            MugShot, Biog,
            Count
        };
    };
    std::array<cro::Entity, PreviewCamera::Count> m_previewCameras = {};
    void buildPreviewScene();
    void buildStatScene();

    void createBodyItems();
    void createHeadwearItems();
    void createEquipmentItems();
    void createLoadoutItems();
    void createDetailItems();

    void onCachedPush() override;
    void onCachedPop() override;

    std::array<cro::Clock, 4u> m_inputRepeatClocks = {};
    std::array<cro::Time, 4u> m_repeatTimes = {};
    std::array<std::uint8_t, 4u> m_controllerMasks = {};
    std::array<std::uint8_t, 4u> m_controllerPrevMasks = {};
    void resetRepeatTimer(std::int32_t, cro::Time);

    UILayout m_uiLayout;

    void updateTabBar();
    void nextTab();
    void prevTab();
    void activateTab(std::int32_t idx);

    cro::Entity m_infoString;
    cro::Entity m_infoSprite;
    std::array<cro::FloatRect, 2u> m_infoRects = {};
    cro::Texture m_colourPreview; //TODO this is 1x1px so we could just atlas into another texture...

    struct ItemIcon final
    {
        enum
        {
            Locked,
            UnlockedItem,
            WorkshopItem,
            WorkshopButton,
            EquipButton,

            Gallawent, Dong,
            Fellowcraft, Akrun,
            Dannis, Clix,
            Beytree, Tunnelrock,
            Flaxen, Hardings,
            Woodgear, BnS,

            Count
        };
    };
    std::array<cro::Sprite, ItemIcon::Count> m_itemIcons = {};


    struct TabID final
    {
        enum
        {
            Body, Headwear,
            Equipment, Loadout,
            Details,

            Count
        };
    };

    void resizeItemGraphics();

    void nextItem();
    void prevItem();
    void activateLeft();
    void activateRight();
    void activate();

    void checkMouseOver(glm::vec2);
    void doMouseClick(glm::vec2);

    void quitState();

    struct StatLayout final
    {
        struct StatBar final
        {
            cro::Entity bgEnt;
            cro::Entity pointer;
            cro::Entity text;
        };
        std::vector<StatBar> statBars;
        cro::Entity statTitle;
        cro::Entity manufacturerName;
        cro::Entity manufacturerInfo;
        cro::Entity manufacturerIcon;
    }m_statLayout;
    void refreshStat(std::uint32_t catID, std::int32_t invID, bool setPointer);

    void loadAvatarPreviews();
    void loadAvatarTextures();
    void loadHairModels();
    void loadBallModels();
    void loadClubData();
    void loadVoiceData();

    std::int32_t indexFromAvatarID(std::uint32_t skinID) const;
    std::int32_t indexFromHairID(std::uint32_t hairID) const;
    std::int32_t indexFromBallID(std::uint32_t) const;
    std::int32_t indexFromClubID(std::uint32_t) const;

    //note this sets the m_avatarIndex member value too!!
    void setAvatarIndex(std::int32_t idx);
    void setHairIndex(std::int32_t idx);
    void setHatIndex(std::int32_t idx);
    void setBallIndex(std::int32_t);
    void setClubIndex(std::int32_t);

    void applyHeadwearTransform(std::size_t idx, std::size_t indexOffset);
    void nameInputWindow();
    void applyNameString();
    void playPreviewAudio();
    void updateMugshot();
    void clearMugshot();
    std::string generateRandomBio() const;
    void refreshBio();
    void setBioString(const std::string&);
    void randomise();
};