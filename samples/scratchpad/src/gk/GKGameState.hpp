//Auto-generated header file for Scratchpad Stub 24/12/2024, 12:04:36

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <crogine/graphics/Texture.hpp>

class GKGameState final : public cro::State, public cro::GuiClient
{
public:
    GKGameState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::GKGame; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    cro::Entity m_playerPoint;
    cro::Entity m_overheadCam;
    cro::RenderTexture m_overheadTexture;
    cro::Texture m_inputTexture;

    std::vector<cro::Entity> m_chunkEnts;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();


    bool loadMap(const std::string&);
    void createChunk(const std::vector<float>&, const std::vector<std::uint32_t>&, std::uint32_t vertexCount, glm::vec3 pos);
};