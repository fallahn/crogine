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

Based on articles: http://www.extentofthejam.com/pseudo/
                   https://codeincomplete.com/articles/javascript-racer/

-----------------------------------------------------------------------*/

#pragma once

#include "../StateIDs.hpp"
#include "TrackCamera.hpp"
#include "TrackSprite.hpp"
#include "Track.hpp"
#include "Player.hpp"


#include <crogine/audio/AudioScape.hpp>
#include <crogine/core/State.hpp>
#include <crogine/core/ConsoleClient.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/gui/GuiClient.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/Vertex2D.hpp>

#include <array>

namespace els
{
    struct SharedStateData;
}

struct SharedStateData;
class EndlessDrivingState final : public cro::State, public cro::GuiClient, public cro::ConsoleClient
{
public:
    EndlessDrivingState(cro::StateStack&, cro::State::Context, SharedStateData&, els::SharedStateData&);

    cro::StateID getStateID() const override { return StateID::EndlessRunner; }

    bool handleEvent(const cro::Event&) override;
    void handleMessage(const cro::Message&) override;
    bool simulate(float) override;
    void render() override;

private:

    SharedStateData& m_sharedData;
    els::SharedStateData& m_sharedGameData;

    //vehicle is a 3D model in its own scene
    //rendered to a buffer to use as a sprite
    cro::Scene m_playerScene;
    cro::Scene m_gameScene;
    cro::Scene m_uiScene;
    cro::ResourceCollection m_resources;
    cro::AudioScape m_audioscape;

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

    struct BrakeLightShader final
    {
        std::uint32_t id = 0;
        std::int32_t uniform = -1;
    }m_brakeShader;

    cro::Entity m_debugEntity;

    //texture is fixed size and game is rendered to this
    //the ui scene then scales this to the current output
    cro::RenderTexture m_gameTexture;
    cro::Entity m_gameEntity;

    //logic ents
    TrackCamera m_trackCamera;
    Track m_road;

    std::vector<Track::GenerationContext> m_trackContexts;
    std::size_t m_contextIndex;

    Player m_player;

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


    struct GameRules final
    {
        float remainingTime = 30.f;
        float totalTime = 0.f;

        //awarded on lap line, reduced by 10
        //until < 10 in which case *= 0.5
        float lapAward = 10.f;
        std::int32_t awardLapTime()
        {
            auto oldTime = remainingTime;
            remainingTime = std::min(30.f, remainingTime + lapAward);
            std::int32_t award = static_cast<std::int32_t>(std::ceil(remainingTime - oldTime));

            lapAward *= 0.9f;
            return award; //used purely for notification
        }

        struct State final
        {
            enum
            {
                CountIn, Running
            };
        };
        std::int32_t state = State::CountIn;

        std::int32_t flagstickMultiplier = 0;
    }m_gameRules;


    std::array<std::vector<float>, TrackSprite::Animation::Count> m_wavetables = {};

    void addSystems();
    void loadAssets();
    void createPlayer();
    void createScene();
    void createRoad();
    void createUI();

    void floatingText(const std::string&);

    struct HapticType final
    {
        enum
        {
            Soft, Foliage, Medium, Hard, Rumble, Lap
        };
    };
    void applyHapticEffect(std::int32_t);

    void updateControllerInput();
    void updatePlayer(float dt);
    void updateRoad(float dt);
    void addRoadQuad(const TrackSegment& s1, const TrackSegment& s2, float widthMultiplier, cro::Colour, std::vector<cro::Vertex2D>&);
    void addRoadSprite(TrackSprite&, const TrackSegment&, std::vector<cro::Vertex2D>&);

    std::pair<glm::vec2, glm::vec2> getScreenCoords(TrackSprite&, const TrackSegment&, bool animate);
};