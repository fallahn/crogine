/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#pragma once

#include "../StateIDs.hpp"
#include "Consts.hpp"
#include "VoxelData.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/Texture.hpp>

#include <polyvox/RawVolume.h>

#include <array>

namespace pv = PolyVox;

class VoxelState final : public cro::State, public cro::GuiClient
{
public:
    VoxelState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::Voxels; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_scene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    cro::Texture m_terrainTexture;
    std::vector<glm::vec4> m_textureBuffer;

    std::array<std::int32_t, Shader::Count> m_shaderIDs = {};
    std::array<std::int32_t, Material::Count> m_materialIDs = {};

    std::array<cro::Entity, Layer::Count> m_layers;
    std::vector<cro::Entity> m_chunks;
    cro::Entity m_cursor;
    std::int32_t m_activeLayer;


    struct TerrainVertex final
    {
        glm::vec3 position = glm::vec3(0.f);
        glm::vec4 colour = glm::vec4(0.1568f, 0.305f, 0.2627f, 1.f);
        glm::vec3 normal = glm::vec3(0.f, 1.f, 0.f);
    };
    std::vector<TerrainVertex> m_terrainBuffer;

    pv::RawVolume<Voxel::Data> m_voxelVolume;

    void buildScene();
    void createLayers();

    void updateCursorPosition();
    void loadSettings();
    void saveSettings();

    struct Brush final
    {
        float feather = 1.f;
        float strength = 1.f;

        struct EditMode final
        {
            enum
            {
                Add = 1,
                Subtract = -1
            };
        };
        std::int32_t editMode = EditMode::Add;

        struct PaintMode final
        {
            enum
            {
                Paint, Carve
            };
        };
        std::int32_t paintMode = PaintMode::Paint;

        std::int32_t terrain = TerrainID::Rough;

    }m_brush;
    bool m_showBrushWindow;

    void applyEdit();
    void editTerrain();
    void updateTerrainImage(cro::IntRect);
    void updateTerrainMesh(cro::IntRect);
    void editVoxel();
    void updateVoxelMesh(const pv::Region&);

    //---VoxelStateUI.cpp---//
    bool m_showLayerWindow;
    std::array<bool, Layer::Count> m_showLayer = {};
    
    void drawMenuBar();
    void drawLayerWindow();
    void drawBrushWindow();

    void handleKeyboardShortcut(const SDL_KeyboardEvent&);
};
