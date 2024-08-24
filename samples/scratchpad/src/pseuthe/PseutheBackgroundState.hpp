//Auto-generated header file for Scratchpad Stub 20/08/2024, 12:39:17

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

class PseutheBackgroundState final : public cro::State, public cro::GuiClient
{
public:
    PseutheBackgroundState(cro::StateStack&, cro::State::Context);

    cro::StateID getStateID() const override { return States::ScratchPad::PseutheBackground; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::ResourceCollection m_resources;

    cro::RenderTexture m_blurBufferH;
    cro::SimpleQuad m_blurQuadH;

    cro::RenderTexture m_backgroundBuffer;
    cro::SimpleQuad m_backgroundQuad;


    //TODO we could arrange some of these in a UBO
    struct ShaderBlocks final
    {
        struct LightRay final
        {
            std::uint32_t shaderID = 0;
            std::int32_t intensityID = -1;
        }lightRay;

        struct Ball final
        {
            std::uint32_t shaderID = 0;
            std::int32_t lightPosID = -1;
            std::int32_t intensityID = -1;
        }ball;
    }m_shaderBlocks;

    void addSystems();
    void loadAssets();
    void createScene();

    void createLightRays();
    void createParticles();
    void createBalls();
};