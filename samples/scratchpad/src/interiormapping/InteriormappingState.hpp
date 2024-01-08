//Auto-generated header file for Scratchpad Stub 28/11/2023, 13:22:27

#pragma once

#include "../StateIDs.hpp"
#include "../chunkvis/ChunkVisSystem.hpp"

#include <crogine/core/State.hpp>
#include <crogine/core/ProfileTimer.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/CubemapTexture.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>

class InteriorMappingState final : public cro::State, public cro::GuiClient
{
public:
    InteriorMappingState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::InteriorMapping; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::CubemapTexture m_cubemap;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    cro::RenderTexture m_cullingDebugTexture;
    cro::SimpleVertexArray m_cullingDebugVerts;
    cro::ProfileTimer<30> m_profileTimer;

    struct EntityID final
    {
        enum
        {
            Instanced,
            InstancedCulled,

            Count
        };
    };

    std::array<cro::Entity, EntityID::Count> m_entities = {};


    struct Cell final
    {
        std::vector<glm::mat4> transforms;
        std::vector<glm::mat3> normals;
    };
    std::array<Cell, ChunkVisSystem::RowCount* ChunkVisSystem::ColCount> m_cells = {};


    void loadCullingAssets();
    void createCullingScene();
};