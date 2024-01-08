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

struct SharedStateData;
struct SharedProfileData;

struct BallPreview final
{
    cro::Entity root;
    cro::Entity ball;
};

struct AvatarPreview final
{
    std::size_t hairIndex = 0; //TODO this doesn't really need to be per model...
    cro::Attachment* hairAttachment = nullptr;
    cro::Entity previewModel;
    std::vector<cro::Entity> previewAudio;
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

    cro::RenderTexture m_avatarTexture;
    cro::RenderTexture m_ballTexture;
    cro::ResourceCollection m_resources;

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
            BioText,

            Count
        };
    };
    std::array<cro::Entity, EntityID::Count> m_menuEntities = {};

    struct CameraID final
    {
        enum
        {
            Avatar, Ball, Mugshot,

            Count
        };
    };
    std::array<cro::Entity, CameraID::Count> m_cameras;

    std::vector<BallPreview> m_ballModels;
    std::vector<cro::Entity> m_ballHairModels;
    std::size_t m_ballIndex;
    std::size_t m_ballHairIndex;
    cro::RenderTexture m_ballThumbs;

    std::vector<AvatarPreview> m_avatarModels;
    std::vector<cro::Entity> m_avatarHairModels;
    std::size_t m_avatarIndex;

    std::vector<ProfileTexture> m_profileTextures;

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
    std::size_t m_lastSelected;

    void addSystems();
    void loadResources();
    void buildScene();
    void buildPreviewScene();
    void createPalettes(cro::Entity);
    void createBallFlyout(cro::Entity);
    void createBallThumbs();
    void quitState();

    std::size_t indexFromAvatarID(std::uint32_t) const;
    std::size_t indexFromBallID(std::uint32_t) const;
    std::size_t indexFromHairID(std::uint32_t) const;

    void setAvatarIndex(std::size_t);
    void setHairIndex(std::size_t);
    void setBallIndex(std::size_t);

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
};