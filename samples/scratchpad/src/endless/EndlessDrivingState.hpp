//Auto-generated header file for Scratchpad Stub 08/01/2024, 12:14:56

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>

class EndlessDrivingState final : public cro::State, public cro::GuiClient
{
public:
    EndlessDrivingState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::EndlessDriving; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    //vehicle is a 3D model in its own scene
    //rendered to a buffer to use as a sprite
    cro::Scene m_playerScene;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    //buffer for player sprite
    cro::RenderTexture m_playerTexture;
    cro::Entity m_playerEntity;
    cro::Entity m_roadEntity;

    //texture is fixed size and game is rendered to this
    //the ui scene then scales this to the current output
    cro::RenderTexture m_gameTexture;
    cro::Entity m_gameEntity;

    void addSystems();
    void loadAssets();
    void createPlayer();
    void createScene();
    void createUI();

};