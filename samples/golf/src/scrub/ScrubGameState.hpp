/*-----------------------------------------------------------------------

Matt Marchant 2024
http://trederia.blogspot.com

Super Video Golf - zlib licence.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>

#include <crogine/util/Easings.hpp>

#include <array>

struct SharedStateData;
class ScrubGameState final : public cro::State, public cro::GuiClient
{
public:
    ScrubGameState(cro::StateStack&, cro::State::Context, SharedStateData&);

    cro::StateID getStateID() const override { return StateID::ScrubGame; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;

    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    std::int16_t m_axisPosition;

    struct Handle final
    {
        bool hasBall = false; //don't score if moving when empty
        bool locked = false; //don't move if ball is being inserted (probably fine if being removed)

        static constexpr float Down = 1.f;
        static constexpr float Up = -1.f;
        float direction = Down;

        float progress = 0.f;
        float speed = 0.f;

        static constexpr float MaxSpeed = 8.f;

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
            static constexpr float MinSoap = 3.f;
            static constexpr float Reduction = 0.7f;
            float amount = MaxSoap;

            //the older the soap is the more quickly it diminishes
            float lifeTime = 0.f;
            static constexpr float MaxLifetime = 12.f;

            std::int32_t count = 1;

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


    struct Ball final
    {
        static constexpr float MaxFilth = 100.f;
        float filth = MaxFilth;

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
        cro::Entity animatedEntity;

        void scrub(float amt)
        {
            //make the ball harder to clean the nearer it is to
            //actually being clean... TODO we could reduce this 
            //over time to make the game increase in difficulty
            const auto multiplier = 0.35f + (0.65f * (filth / MaxFilth));
            amt *= multiplier;

            filth = std::max(0.f, filth - amt);
        }
    }m_ball;

    struct Score final
    {
        float threshold = 80.f; //ball must be cleaner than this to score

        float remainingTime = 30.f;
        std::int32_t ballsWashed = 0;
        float avgCleanliness = 0.f;

        //divide this by total count to get above
        float cleanlinessSum = 0.f;

        bool gameRunning = false;

        //how many balls were 100% in a row
        std::int32_t bonusRun = 0;

        float totalRunTime = 0.f;
        std::int32_t totalScore = 0;


        //this is the amount of time awarded multiplied
        //by how clean the ball is when it's removed.
        static constexpr float TimeBonus = 3.f;
    }m_score;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();
    void createCountIn();

    void handleCallback(cro::Entity, float);
    void ballCallback(cro::Entity, float);
    void updateScore();

    std::vector<cro::Entity> m_messageQueue;
    void showMessage(const std::string&);
};