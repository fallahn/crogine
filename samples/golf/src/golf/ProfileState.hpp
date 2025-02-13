/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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
#include "PlayerData.hpp"
#include "PlayerAvatar.hpp"
#include "CommonConsts.hpp"
#include "MenuConsts.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/SpriteSheet.hpp>

struct SharedStateData;
struct SharedProfileData;

namespace cro
{
    class SpriteSheet;
}

struct BallPreview final
{
    cro::Entity root;
    cro::Entity ball;

    std::int32_t type = 0;
};

struct AvatarPreview final
{
    std::int32_t type = 0;
    std::size_t hairIndex = 0; //TODO this doesn't really need to be per model...
    std::size_t hatIndex = 0; //TODO this doesn't really need to be per model...
    cro::Attachment* hairAttachment = nullptr;
    cro::Attachment* hatAttachment = nullptr;
    cro::Entity previewModel;
    std::vector<cro::Entity> previewAudio;
    std::size_t previewIndex = 0; //actual index may differ because of locked models
};

class ProfileState final : public cro::State, public cro::GuiClient
{
public:
    ProfileState(cro::StateStack&, cro::State::Context, SharedStateData&, SharedProfileData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Profile; }

private:

    cro::Scene m_uiScene;
    cro::Scene m_modelScene;
    SharedStateData& m_sharedData;
    SharedProfileData& m_profileData;

    PlayerData m_activeProfile;
    cro::String m_previousName; //so we can restore if we cancel an edit

    cro::RenderTexture m_avatarTexture;
    cro::RenderTexture m_ballTexture;
    cro::RenderTexture m_hairEditorTexture;
    cro::ResourceCollection& m_resources;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back, Select, Snap,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;

    struct EntityID final
    {
        enum
        {
            Root, HelpText,
            Mugshot, NameText,
            Swatch, AvatarPreview,
            BioText, TipText,

            BallBrowser,
            HairBrowser,
            ClubBrowser,
            HairEditor, HairHelp, 
            HairPreview, HairColourPreview,
            SpeechEditor,

            Count
        };
    };
    std::array<cro::Entity, EntityID::Count> m_menuEntities = {};

    struct CameraID final
    {
        enum
        {
            Avatar, Ball, Mugshot,
            HairEdit,

            Count
        };
    };
    std::array<cro::Entity, CameraID::Count> m_cameras;

    std::vector<BallPreview> m_ballModels;
    std::vector<cro::Entity> m_previewHairModels;
    std::size_t m_ballIndex;
    std::size_t m_ballHairIndex;

    std::vector<AvatarPreview> m_avatarModels;
    std::vector<cro::Entity> m_avatarHairModels;
    std::size_t m_avatarIndex;
    std::size_t m_lockedAvatarCount;

    std::vector<ProfileTexture> m_profileTextures;

    //TODO move to common header
    struct ClubData final
    {
        std::uint32_t uid = 0;
        std::string name;
        std::string thumbnail;
        bool userItem = false;
    };
    std::vector<ClubData> m_clubData;
    cro::Entity m_clubText;

    struct PaletteID final
    {
        enum
        {
            Hair, Skin,
            TopL, TopD,
            BotL, BotD,

            BallThumb,

            Count
        };
    };
    std::array<FlyoutMenu, PaletteID::Count> m_flyouts = {};
    FlyoutMenu m_ballColourFlyout;

    std::size_t m_lastSelected;
    float m_avatarRotation;

    void addSystems();
    void loadResources();
    void buildScene();
    void buildPreviewScene();
    void createPalettes(cro::Entity);
    void createItemThumbs();
    void createItemPage(cro::Entity, std::int32_t page, std::int32_t itemID);

    //used to pass menu callbacks between creation functions
    struct CallbackContext final
    {
        std::int32_t arrowSelected = 0;
        std::int32_t arrowUnselected = 0;
        std::int32_t closeSelected = 0;
        std::int32_t closeUnselected = 0;
        glm::vec3 closeButtonPosition = glm::vec3({ 468.f, 331.f, 0.1f });
        cro::SpriteSheet spriteSheet;
        std::function<cro::Entity(std::int32_t)> createArrow;
        std::function<void()> onClose;
    };

    struct HeadwearID final
    {
        enum
        {
            Hair, Hat,
            Count
        };
    };
    std::int32_t m_headwearID;
    std::array<cro::FloatRect, HeadwearID::Count> m_headwearPreviewRects = {};

    struct TransformBox final
    {
        cro::Entity entity;
        float minVal = 0.1f;
        float maxVal = 1.f;
        float step = 0.1f;

        std::uint32_t index = 0;
        std::uint32_t offset = 0;
    };
    std::array<TransformBox, 9> m_transformBoxes;

    struct Gizmo final
    {
        cro::Entity entity;
        std::vector<cro::Vertex2D> x;
        std::vector<cro::Vertex2D> y;
        std::vector<cro::Vertex2D> z;
    }m_gizmo;
    void updateGizmo();
    void updateHeadwearTransform();

    std::vector<cro::AudioScape> m_voices;
    std::size_t m_voiceIndex;
    void playPreviewAudio();

    void createBallBrowser(cro::Entity, const CallbackContext&);
    void createHairBrowser(cro::Entity, const CallbackContext&);
    void createHairEditor(cro::Entity, const CallbackContext&);
    void createSpeechEditor(cro::Entity, const CallbackContext&);
    void createClubBrowser(cro::Entity, const CallbackContext&);
    cro::FloatRect getHeadwearTextureRect(std::size_t);
    std::size_t fetchUIIndexFromColour(std::uint8_t colourIndex, std::int32_t paletteIndex);
    std::pair<cro::Entity, cro::Entity> createBrowserBackground(std::int32_t, const CallbackContext&);

    void quitState();

    //returns the highlight entity
    cro::Entity createPaletteBackground(cro::Entity parent, FlyoutMenu& target, std::uint32_t palleteIndex);
    void createPaletteButtons(FlyoutMenu& target, std::uint32_t palleteIndex, std::uint32_t menuID, std::size_t indexOffset);

    std::size_t indexFromAvatarID(std::uint32_t) const;
    std::size_t indexFromBallID(std::uint32_t) const;
    std::size_t indexFromHairID(std::uint32_t) const;

    void setAvatarIndex(std::size_t);
    void setHairIndex(std::size_t);
    void setHatIndex(std::size_t);
    void setBallIndex(std::size_t);

    struct PageHandles final
    {
        cro::Entity prevButton;
        cro::Entity nextButton;
        cro::Entity itemLabel;
        cro::Entity pageCount;
        std::size_t pageTotal = 1;
    };

    struct PaginatorContext final
    {
        cro::RenderTexture thumbnailTexture;
        std::vector<BrowserPage> pageList;
        PageHandles pageHandles;
        std::size_t menuID;
        std::uint32_t activateCallback;

        std::size_t pageIndex = 0;
        std::size_t itemCount = 0;
    };

    struct PaginationID final
    {
        enum
        {
            Balls, Hair, Clubs,

            Count
        };
    };
    std::array<PaginatorContext, PaginationID::Count> m_pageContexts = {};

    void nextPage(std::int32_t itemID);
    void prevPage(std::int32_t itemID);
    void activatePage(std::int32_t itemID, std::size_t page, bool);

    void refreshMugshot();
    void refreshNameString();
    void refreshSwatch();
    void refreshBio();

    struct TextEdit final
    {
        cro::String* string = nullptr;
        cro::Entity entity;
        std::size_t maxLen = ConstVal::MaxStringChars;
    }m_textEdit;

    cro::String m_previousString;
    void beginTextEdit(cro::Entity, cro::String*, std::size_t);
    void handleTextEdit(const cro::Event&);
    void cancelTextEdit();
    bool applyTextEdit(); //returns true if this consumed event

    std::string generateRandomBio() const;
    void setBioString(const std::string&);

    cro::RenderTexture m_mugshotTexture;
    bool m_mugshotUpdated;
    void generateMugshot();

    void renderBallFrames();
};