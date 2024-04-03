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

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/audio/AudioScape.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Shader.hpp>

#include <array>

struct SharedStateData;

class UnlockState final : public cro::State
{
public:
    UnlockState(cro::StateStack&, cro::State::Context, SharedStateData&);

    bool handleEvent(const cro::Event&) override;

    void handleMessage(const cro::Message&) override;

    bool simulate(float) override;

    void render() override;

    cro::StateID getStateID() const override { return StateID::Unlock; }

private:

    cro::Scene m_scene;
    cro::Scene m_modelScene;
    SharedStateData& m_sharedData;

    struct UnlockCollection final
    {
        cro::Entity root;
        cro::Entity backgroundInner;
        cro::Entity description;
        cro::Entity name;
        cro::Entity title;
        cro::Entity modelSprite;
        cro::Entity modelNode;
        float modelScale = 1.f;
    };
    std::size_t m_itemIndex;
    std::vector<UnlockCollection> m_unlockCollections;

    cro::AudioScape m_menuSounds;
    struct AudioID final
    {
        enum
        {
            Accept, Back,
            Fireworks,
            Cheer,

            Count
        };
    };
    std::array<cro::Entity, AudioID::Count> m_audioEnts = {};

    glm::vec2 m_viewScale;
    cro::Entity m_rootNode;
    cro::Entity m_particleNode;

    cro::RenderTexture m_modelTexture;

    //TODO these would probably be faster to load
    //in shader resources, and would be more consistent
    cro::Shader m_celShader;
    cro::Shader m_reflectionShader;
    cro::CubemapTexture m_cubemap;
    std::array<cro::Material::Data, 2u> m_materials = {};

    void addSystems();
    void buildScene();
    void buildUI();
    void quitState();
};