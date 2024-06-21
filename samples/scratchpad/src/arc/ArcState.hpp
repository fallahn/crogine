//Auto-generated header file for Scratchpad Stub 20/06/2024, 15:19:50

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

class ArcState final : public cro::State, public cro::GuiClient
{
public:
    ArcState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::Arc; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    std::int32_t m_clubID;
    std::int32_t m_clubLevel;
    float m_zoom;
    cro::Entity m_plotEntity;

    struct ShotType final
    {
        enum
        {
            Default, Flop, Punch,
            Count
        };
    };
    std::array<float, ShotType::Count> m_distances = {};

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void plotArc();
};