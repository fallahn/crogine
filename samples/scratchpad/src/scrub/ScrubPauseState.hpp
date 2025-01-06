#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>

#include <crogine/ecs/Scene.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

class ScrubPauseState final : public cro::State
{
public:
    ScrubPauseState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::ScrubPause; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    void addSystems();
    void buildScene();
};
