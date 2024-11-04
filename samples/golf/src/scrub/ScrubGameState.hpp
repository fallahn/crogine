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

#include "ScrubConsts.hpp"
#include "../StateIDs.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/EnvironmentMap.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleVertexArray.hpp>
#include <crogine/util/Wavetable.hpp>
#ifdef HIDE_BACKGROUND
#include <crogine/graphics/SimpleQuad.hpp>
#endif

#include <crogine/util/Easings.hpp>

#include <array>

struct SharedStateData;
struct SharedScrubData;
class ScrubSoundDirector;
class ScrubGameState final : public cro::State
#ifdef CRO_DEBUG_
    , public cro::GuiClient
#endif
{
public:
    ScrubGameState(cro::StateStack&, cro::State::Context, SharedStateData&, SharedScrubData&);

    cro::StateID getStateID() const override { return StateID::ScrubGame; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:
    SharedStateData& m_sharedData;
    SharedScrubData& m_sharedScrubData;
    ScrubSoundDirector* m_soundDirector;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    cro::EnvironmentMap m_environmentMap;

    cro::RenderTexture m_bucketTexture;
    cro::RenderTexture m_soapTexture;

    cro::Entity m_bucketCamera;

    std::int32_t m_pitchStage;
    cro::Entity m_music;
    cro::Entity m_body0;
    cro::Entity m_body1;

    std::vector<cro::Vertex2D> m_soapVertexData;
    cro::SimpleVertexArray m_soapVertices;
    bool m_soapAnimationActive;

#ifdef HIDE_BACKGROUND
    //temp background placeholder
    cro::SimpleQuad m_tempBground;
#endif

    //used for resizing the UI
    cro::Entity m_spriteRoot;

    struct AnimatedEntity final
    {
        enum
        {
            ScrubberRoot,
            UITop,

            Count
        };
    };
    std::array<cro::Entity, AnimatedEntity::Count> m_animatedEntities = {};
    

    struct ParticleEntity final
    {
        enum
        {
            LevelBubbles,
            NewSoapBubbles,

            Count
        };
    };
    std::array<cro::Entity, ParticleEntity::Count> m_particleEntities = {};

    std::int16_t m_axisPosition; //game controller axis
    std::int16_t m_leftTriggerPosition; //game controller axis
    std::int16_t m_rightTriggerPosition; //game controller axis
    std::int32_t m_controllerIndex; //last operated controller

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
        float switchDirection(float, std::int32_t ballsWashed);
        float calcStroke(std::int32_t ballsWashed);

        cro::Entity entity;


        //has to be a member *sigh*
        struct Soap final
        {
            static constexpr float MaxSoap = 10.f;
            static constexpr float MinSoap = 3.f;
            static constexpr float Reduction = 0.65f;
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

        void reset()
        {
            hasBall = false;
            locked = false;
            direction = Down;
            progress = 0.f;
            speed = 0.f;
            stroke = 0.f;
            strokeStart = 0.f;
            soap = {};
        }

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

        void reset()
        {
            filth = MaxFilth;
            state = State::Idle;
        }
    }m_ball;

    struct Score final
    {
        float threshold = 50.f; //ball must be cleaner than this to score

        float remainingTime = 30.f;
        std::int32_t ballsWashed = 0;
        std::int32_t perfectBalls = 0;
        float avgCleanliness = 0.f;

        //divide this by total count to get above
        float cleanlinessSum = 0.f;

        bool gameRunning = false;
        bool gameFinished = false;

        //how many balls were 100% in a row
        std::int32_t bonusRun = 0;
        static constexpr std::int32_t bonusRunThreshold = 8; //after this we only need awardLevel balls to get more soap
        std::int32_t countAtThreshold = 0; //number of balls when threshold met to correct for count offset
        std::int32_t awardLevel = 5;

        float totalRunTime = 0.f;
        std::int32_t totalScore = 0;
        float quitTimeout = 0.f;

        //this is the amount of time awarded multiplied
        //by how clean the ball is when it's removed.
        static constexpr float TimeBonus = 2.5f;

        //used when summarising scores
        struct Summary final
        {
            std::int32_t runtimeBonus = 0;
            std::int32_t ballCountBonus = 0;
            std::int32_t cleanAvgBonus = 0;
            std::int32_t perfectBonus = 0;

            enum
            {
                Time, BallCount, Avg, Perfect, Done
            };
            std::int32_t status = Time;
            std::int32_t counter = 0; //used when counting the animation
            bool active = false; //don't allow input until this is true
        }summary;
    }m_score;

    struct CameraShake final
    {
        cro::Entity camera;
        glm::vec3 basePos = glm::vec3(0.f);

        const std::vector<float> wavetable;
        std::size_t tableIndex = 0;
        float duration = 0.f;
        static constexpr float MaxDuration = 0.5f;
        static constexpr float Strength = 0.005f;
        bool active = false;

        CameraShake()
            : wavetable(cro::Util::Wavetable::sine(15.f))
        {

        }

        void update(float dt)
        {
            if (active)
            {
                tableIndex = (tableIndex + 1) % wavetable.size();
                const auto amount = wavetable[tableIndex] * ((duration / MaxDuration) * Strength);

                camera.getComponent<cro::Transform>().setPosition(basePos + glm::vec3(amount, 0.f, 0.f));

                duration = std::max(0.f, duration - dt);
                if (duration == 0.f)
                {
                    active = false;
                    camera.getComponent<cro::Transform>().setPosition(basePos);
                }
            }
        }

        void start()
        {
            active = true;
            duration = MaxDuration;
        }
    }m_cameraShake;


    void onCachedPush() override;
    void onCachedPop() override;

    void addSystems();
    void loadAssets();
    void createScene();
    void createUI();
    void createCountIn();

    void handleCallback(cro::Entity, float);
    void ballCallback(cro::Entity, float);
    bool updateScore(); //returns false if ball wasn't clean enough
    void endRound(); //displays the summary screen

    std::vector<cro::Entity> m_messageQueue;
    void showMessage(const std::string&);
    void showSoapEffect();

    void resetCamera();

    void attachText(cro::Entity); //calcs coords from screen space when spawning text items
    void attachSprite(cro::Entity); //calcs coords from screen space when spawning sprite items

    void levelMeterCallback(cro::Entity); //resize callbacks for level meters
};