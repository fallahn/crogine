//Auto-generated header file for Scratchpad Stub 04/11/2026, 10:20:39

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

class VertexCloudState final : public cro::State, public cro::GuiClient
{
public:
    VertexCloudState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::VertexCloud; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;

    struct ShaderID final
    {
        enum
        {
            Static, Dynamic,
            DynamicEased, DynamicPerspective,

            Count
        };
    };

    struct Uniform final
    {
        std::uint32_t shader = 0;
        std::int32_t uniform = -1;
    };
    std::array<Uniform, ShaderID::Count> m_timeUniforms = {};

    std::array<cro::Entity, ShaderID::Count> m_cloudEnts = {};
    std::array<cro::Entity, ShaderID::Count> m_textEnts = {};

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

};