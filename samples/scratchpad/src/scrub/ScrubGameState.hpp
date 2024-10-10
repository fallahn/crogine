//Auto-generated header file for Scratchpad Stub 10/10/2024, 12:10:42

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>

#include <array>

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

    struct Handle final
    {
        bool hasBall = false; //don't score if moving when empty
        bool locked = false; //don't move if ball is being inserted (probably fine if being removed)

        static constexpr float Down = 1.f;
        static constexpr float Up = -1.f;
        float direction = Down;

        float progress = 0.f;
        float speed = 0.f;

        static constexpr float MaxSpeed = 6.f;

        float stroke = 0.f;
        float strokeStart = 0.f;

        static constexpr float MaxSoap = 10.f;
        static constexpr float MinSoap = 1.f;
        float soap = MaxSoap;

        //these both return the calc'd stroke
        float switchDirection(float);
        float calcStroke();

        cro::Entity entity;
    }m_handle;

    struct Ball final
    {
        float filth = 100.f;

        struct State final
        {
            enum
            {
                Idle, Insert,
                Clean, Extract
            };
        };
        std::int32_t state = State::Idle;

        static constexpr float Speed = 1.f;

        cro::Entity entity;
    }m_ball;

    std::int32_t m_soapCount;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();
};