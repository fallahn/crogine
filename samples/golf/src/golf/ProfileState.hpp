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

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/RenderTexture.hpp>

struct SharedStateData;
struct SharedProfileData;

struct AvatarPreview final
{
    cro::Attachment* hairAttachment = nullptr;
    cro::Entity previewModel;
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

    cro::RenderTexture m_avatarTexture;
    cro::RenderTexture m_ballTexture;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;

    cro::Entity m_avatarCam;
    cro::Entity m_ballCam;

    std::vector<cro::Entity> m_ballModels;
    std::vector<AvatarPreview> m_avatarModels;
    std::vector<cro::Entity> m_hairModels;
    std::vector<ProfileTexture> m_profileTextures;

    void addSystems();
    void loadResources();
    void buildScene();
    void buildPreviewScene();
    void createProfileTexture(std::int32_t);
    void quitState();

    std::size_t indexFromAvatarID(std::uint32_t);
    std::size_t indexFromBallID(std::uint32_t);
};