//Auto-generated header file for Scratchpad Stub 28/11/2023, 13:22:27

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/CubemapTexture.hpp>

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

    void loadCullingAssets();
    void createCullingScene();
};