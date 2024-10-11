//Auto-generated header file for Scratchpad Stub 10/10/2024, 12:10:42

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <crogine/util/Easings.hpp>

#include <array>

class ScrubGameState final : public cro::State, public cro::GuiClient
{
public:
    ScrubGameState(cro::StateStack&, cro::State::Context);
    ~ScrubGameState(); //TODO remove this
    cro::StateID getStateID() const override { return States::ScratchPad::Scrub; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

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

        //these both return the calc'd stroke
        float switchDirection(float);
        float calcStroke();

        cro::Entity entity;


        //has to be a member *sigh*
        struct Soap final
        {
            static constexpr float MaxSoap = 10.f;
            static constexpr float MinSoap = 1.f;
            static constexpr float Reduction = 0.5f;
            float amount = MaxSoap;

            //the older the soap is the more quickly it diminishes
            float lifeTime = 0.f;
            static constexpr float MaxLifetime = 12.f;

            float getReduction() const
            {
                return (Soap::Reduction * cro::Util::Easing::easeInExpo(std::min((lifeTime / Soap::MaxLifetime), 1.f)));
            }

            void refresh()
            {
                lifeTime = 0.f;
                amount = MaxSoap;
            }
        }soap;

    }m_handle;
    std::int32_t m_soapCount;

    cro::Clock soapTimer; //TODO remove this


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
        std::size_t colourIndex = 0;

        cro::Entity entity;
    }m_ball;

    struct Score final
    {
        float remainingTime = 30.f;
        std::int32_t ballsWashed = 0;
        float avgCleanliness = 0.f;

        //divide this by total count to get above
        float cleanlinessSum = 0.f;

        bool gameRunning = false;


        //this is the amount of time added multiplied
        //by how clean the ball is when it's removed.
        //currently calc'd by the avg time it takes to
        //scrub a ball clean with 50% soap available.
        static constexpr float TimeBonus = 6.f;
    }m_score;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();

    void handleCallback(cro::Entity, float);
    void ballCallback(cro::Entity, float);
    void updateScore();
};