//Auto-generated header file for Scratchpad Stub 10/10/2024, 12:10:42

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

class ScrubGameState final : public cro::State, public cro::GuiClient
{
public:
    ScrubGameState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::Scrub; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

};