//Auto-generated header file for Scratchpad Stub 12/06/2026, 10:01:17

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ArrayTexture.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

class SurrealState final : public cro::State, public cro::GuiClient
{
public:
    SurrealState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::Surreal; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::ArrayTexture<std::uint8_t, 40> m_arrayTexture;

    struct WaveShader final
    {
        std::uint32_t ID = 0;
        std::int32_t timeUniform = -1;
        //std::int32_t skyUniform = -1;

        std::int32_t fogStart = -1;
        std::int32_t fogEnd = -1;
        std::int32_t fogDensity = -1;
    }m_waveShader;

    struct TerrainShader final
    {
        std::uint32_t ID = 0;
        std::int32_t blend = -1;
    }m_terrainShader;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();
};