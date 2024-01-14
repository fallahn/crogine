//Auto-generated header file for Scratchpad Stub 08/01/2024, 12:14:56

#pragma once

#include "../StateIDs.hpp"
#include "TrackCamera.hpp"
#include "TrackSprite.hpp"
#include "Track.hpp"

#include <crogine/core/State.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Vertex2D.hpp>

#include <array>

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

    struct BackgroundLayer final
    {
        cro::Entity entity;
        cro::FloatRect textureRect;
        float speed = 0.f;
        float verticalSpeed = 0.f;
        enum
        {
            Sky, Hills, Trees,
            Count
        };
    };
    std::array<BackgroundLayer, BackgroundLayer::Count> m_background = {};

    std::array<TrackSprite, TrackSprite::Count> m_trackSprites = {};
    cro::Entity m_trackSpriteEntity;


    //buffer for player sprite
    cro::RenderTexture m_playerTexture;
    cro::Entity m_playerEntity; //3D model
    cro::Entity m_playerSprite;
    cro::Entity m_roadEntity;

    //texture is fixed size and game is rendered to this
    //the ui scene then scales this to the current output
    cro::RenderTexture m_gameTexture;
    cro::Entity m_gameEntity;

    //logic ents
    TrackCamera m_trackCamera;
    Track m_road;

    struct Player final
    {
        //float x = 0.f; //+/- 1 from X centre
        //float y = 0.f;
        //float z = 0.f; //rel distance from camera
        glm::vec3 position = glm::vec3(0.f);
        float speed = 0.f;

        static inline constexpr float MaxSpeed = SegmentLength * 120.f; //60 is our frame time
        static inline constexpr float Acceleration = MaxSpeed / 3.f;
        static inline constexpr float Braking = -MaxSpeed;
        static inline constexpr float Deceleration = -Acceleration;
        static inline constexpr float OffroadDeceleration = -MaxSpeed / 2.f;
        static inline constexpr float OffroadMaxSpeed = MaxSpeed / 4.f;
        static inline constexpr float Centrifuge = 530.f; // "pull" on cornering

        struct Model final
        {
            float targetRotationX = 0.f;
            float rotationX = 0.f;
            float rotationY = 0.f;

            static constexpr float MaxX = 0.1f;
            static constexpr float MaxY = 0.2f;
        }model;
    }m_player;

    struct InputFlags final
    {
        enum
        {
            Up    = 0x1,
            Down  = 0x2,
            Left  = 0x4,
            Right = 0x8
        };
        std::uint16_t flags = 0;
        std::uint16_t prevFlags = 0;

        float steerMultiplier = 1.f;
        float accelerateMultiplier = 1.f;
        float brakeMultiplier = 1.f;

        std::int32_t keyCount = 0; //tracks how many keys are pressed and only allows controller to override when 0
    }m_inputFlags;


    void addSystems();
    void loadAssets();
    void createPlayer();
    void createScene();
    void createUI();

    void updateControllerInput();
    void updatePlayer(float dt);
    void updateRoad(float dt);
    void addRoadQuad(float x1, float x2, float y1, float y2, float w1, float w2, cro::Colour, std::vector<cro::Vertex2D>&);
    void addRoadSprite(const TrackSprite&, glm::vec2, float scale, float clip, float fogAmount, std::vector<cro::Vertex2D>&);
};