/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

#include "Terrain.hpp"
#include "MenuConsts.hpp"
#include "SharedStateData.hpp"
#include "InputParser.hpp"
#include "CommandIDs.hpp"
#include "../GolfGame.hpp"

#include <Social.hpp>

#include <crogine/audio/AudioMixer.hpp>
#include <crogine/core/ConfigFile.hpp>
#include <crogine/core/GameController.hpp>
#include <crogine/core/SysTime.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/CommandTarget.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/ecs/components/Drawable2D.hpp>
#include <crogine/ecs/components/AudioEmitter.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/ImageArray.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Matrix.hpp>
#include <crogine/util/Maths.hpp>

#include <cstdint>
#include <sstream>
#include <iomanip>
#include <array>

static constexpr float ToYards = 1.09361f;
static constexpr float ToFeet = 3.281f;
static constexpr float ToInches = 12.f;

static inline constexpr std::int32_t CrowdDensityCount = 4;
//decreased for each additional player to a minimum of 2
//so max is actually 4 because we always have at least 2 players
static inline constexpr std::uint8_t StartLives = 6;
static inline constexpr std::uint8_t MaxNTPStrokes = 2; //nearest the pin
static inline constexpr std::int32_t CareerLeagueThreshold = 6; //placing higher than this unlocks next league

static constexpr float MaxBallRadius = 0.07f;
static constexpr float GreenCamRadiusLarge = 45.f;
static constexpr float GreenCamRadiusMedium = 10.f;
static constexpr float GreenCamRadiusSmall = 5.7f;
static constexpr float SkyCamRadius = 80.f;

static constexpr float CameraFarPlane = 450.f;// 360.f;// 320.f;
static constexpr float GreenCamZoomFast = 2.5f;
static constexpr float GreenCamZoomSlow = 1.8f;
static constexpr float SkyCamZoomSpeed = 1.1f;// 3.f;
static constexpr float FlightCamFOV = 90.f;
static constexpr glm::vec3 FlightCamOffset = glm::vec3(0.f, 0.098f, 0.f);
static constexpr float MinFlightCamDistance = 0.132f;
static constexpr float FlightCamRotation = -0.158f;

//static constexpr glm::uvec2 MapSize(320u, 200u);//320,200
static constexpr glm::uvec2 MapSize(560u, 320u);
static constexpr glm::vec2 MapSizeFloat(MapSize);
static constexpr glm::vec2 RangeSize(200.f, 250.f);
static constexpr float MaxSubTarget = (MapSize.x * 2.f) * (MapSize.x * 2.f); //used to validate the sub-target property of a hole
static constexpr glm::uvec2 MiniMapSize(320u, 200u);
static constexpr std::uint32_t MapSizeMultiplier = 8u;// 4u; //increases the texture resolution by this much
static constexpr glm::vec2 MapSizeRatio = glm::vec2(MiniMapSize) / MapSizeFloat;

static constexpr float WaterRadius = 240.f;

static constexpr float CameraStrokeHeight = 2.f;
static constexpr float CameraPuttHeight = 0.6f;// 0.3f;
static constexpr float CameraTeeMultiplier = 0.65f; //height reduced by this when not putting from tee
static constexpr float CameraStrokeOffset = 5.f;
static constexpr float CameraPuttOffset = 1.6f; //0.8f;
static constexpr glm::vec3 CameraBystanderOffset = glm::vec3(7.f, 2.f, 7.f);

static constexpr float PuttingZoom = 0.78f;// 0.83f;// 0.93f;
static constexpr float GolfZoom = 0.59f;

static constexpr float GreenFadeDistance = 0.8f;
static constexpr float CourseFadeDistance = 2.f;
static constexpr float ZoomFadeDistance = 10.f;

static constexpr float GreenCamHeight = 3.f;
static constexpr float SkyCamHeight = 8.f;// 16.f; //8.f for lower club set
static constexpr float MinDroneHeight = 12.f;// 20.f; //12.f pushes at least this far above terrain on hilly courses
static constexpr glm::vec3 DefaultSkycamPosition(MapSize.x / 2.f, SkyCamHeight, -static_cast<float>(MapSize.y) / 2.f);

static constexpr float BallPointSize = 1.4f;
static constexpr float LongPuttDistance = 6.f;

static constexpr float MinHook = 0.08f; //used to decide if we call a hook or slice in UI
static constexpr float MaxHook = -0.25f;
static constexpr float KnotsPerMetre = 1.94384f;
static constexpr float MPHPerMetre = 2.23694f;
static constexpr float KPHPerMetre = 3.6f;
static constexpr float HoleRadius = 0.058f;

static constexpr float WaterLevel = -0.02f;
static constexpr float TerrainLevel = WaterLevel - 0.48f;// 0.03f;
static constexpr float MaxTerrainHeight = 5.f;// 4.5f;

static constexpr float FlagRaiseDistance = 3.f * 3.f;
static constexpr float PlayerShadowOffset = 0.04f;
static constexpr float PlayerFootOffset = 0.65f; //distance to AV feet from ball
static constexpr glm::vec3 PlayerFootPos = glm::vec3(-PlayerFootOffset, 0.f, 0.f);

static constexpr float MinPixelScale = 1.f;
static constexpr float MaxPixelScale = 3.f;

static constexpr float PlaneHeight = 60.f;

static constexpr float IndicatorDarkness = 0.002f;
static constexpr float IndicatorLightness = 0.5f;

static constexpr glm::vec2 LabelIconSize(Social::IconSize);
static constexpr glm::uvec2 LabelTextureSize(160u, 128u + (Social::IconSize * 4));
static constexpr glm::vec3 OriginOffset(static_cast<float>(MapSize.x / 2), 0.f, -static_cast<float>(MapSize.y / 2));

static constexpr cro::Colour WaterColour(0.02f, 0.078f, 0.578f);
static constexpr cro::Colour SkyTop(0.678f, 0.851f, 0.718f);
static constexpr cro::Colour SkyBottom(0.2f, 0.304f, 0.612f);
static constexpr cro::Colour SkyNight(std::uint8_t(101), 103, 178);
//static constexpr cro::Colour SkyNight(std::uint8_t(69), 71, 130);
static constexpr cro::Colour DropShadowColour(0.396f, 0.263f, 0.184f);

static constexpr cro::Colour SwingputDark(std::uint8_t(40), 23, 33);
//static constexpr cro::Colour SwingputLight(std::uint8_t(242), 207, 92);
static constexpr cro::Colour SwingputLight(std::uint8_t(236), 119, 61);
//static constexpr cro::Colour SwingputLight(std::uint8_t(236), 153, 61);

//moved to GameController but I'm too lazy to update all references
static constexpr std::int16_t LeftThumbDeadZone = cro::GameController::LeftThumbDeadZone;
static constexpr std::int16_t RightThumbDeadZone = cro::GameController::RightThumbDeadZone;
static constexpr std::int16_t TriggerDeadZone = cro::GameController::TriggerDeadZone;

static constexpr glm::vec3 PreviewHairScale(0.23f);
static constexpr glm::vec3 PreviewHairOffset(0.f, -0.29f, -0.008f);

static constexpr float MinMusicVolume = 0.001f;
static constexpr glm::uvec2 FlagTextureSize(336u, 240u);

class btVector3;
glm::vec3 btToGlm(btVector3 v);
btVector3 glmToBt(glm::vec3 v);

struct WeatherType final
{
    enum
    {
        Clear, Rain, Showers, Mist,
        Random,
        Count,
        Snow
    };
};
static inline const std::array<cro::String, WeatherType::Count> WeatherStrings =
{
    "Clear", "Rain",
    "Showers", "Mist",
    "Random"
};

struct MRTIndex final
{
    enum
    {
        Colour, Position, Normal, Light,

        Count
    };
};

struct TutorialID
{
    //note that these are in order in which they are displayed
    enum
    {
        One, Two, Three,
        Swing, Spin, Putt
    };
};

struct LobbyPager final
{
    cro::Entity rootNode;
    std::vector<cro::Entity> pages;
    std::vector<cro::Entity> slots;
    std::vector<std::uint64_t> serverIDs;

    std::array<cro::Entity, 2u> buttonLeft;
    std::array<cro::Entity, 2u> buttonRight;

    std::size_t currentPage = 0;
    std::size_t currentSlot = 0;
    static constexpr std::size_t ItemsPerPage = 10;
};

struct SpriteAnimID final
{
    enum
    {
        Swing = 0,
        Medal,
        BillboardSwing,
        BillboardRewind,
        Footstep,
        Pump,
        Swoosh
    };
};

//data blocks for uniform buffer
struct WindData final
{
    float direction[3] = {1.f, 0.f, 0.f};
    float elapsedTime = 0.f;
};

struct ResolutionData final
{
    glm::vec2 resolution = glm::vec2(1.f);
    glm::vec2 bufferResolution = glm::vec2(1.f);
    float nearFadeDistance = 2.f;
};

struct ShaderID final
{
    enum
    {
        Water = 100,
        Horizon,
        HorizonSun,
        Terrain,
        Billboard,
        BillboardShadow,
        Cel,
        CelSkinned,
        CelTextured,
        CelTexturedNoWind,
        CelTexturedMasked,
        CelTexturedMaskedLightMap,
        CelTexturedMaskedNoWind,
        CelTexturedInstanced,
        CelTexturedSkinned,
        CelTexturedSkinnedMasked,
        ShadowMap,
        ShadowMapInstanced,
        ShadowMapSkinned,
        Crowd,
        CrowdShadow,
        CrowdArray,
        CrowdShadowArray,
        Cloud,
        CloudRing,
        Leaderboard,
        Player,
        Hair,
        HairReflect,
        Course,
        CourseGreen,
        CourseGrid,
        Ball,
        BallSkinned,
        BallNight,
        BallNightSkinned,
        BallWasher,
        Slope,
        Minimap,
        MinimapView,
        TutorialSlope,
        Wireframe,
        WireframeCulled,
        WireframeCulledPoint,
        Weather,
        Transition,
        Trophy,
        Beacon,
        Target,
        Bow,
        Noise,
        BoxBlur,
        TreesetLeaf,
        TreesetBranch,
        TreesetShadow,
        TreesetLeafShadow,
        BallTrail,
        FXAA,
        Composite,
        CompositeDOF,
        Blur,
        Flag,
        TV,
        PointLight,
        Glass,
        LensFlare,
        PointFlare,
        Firework,
        Rope,
        Lantern,
        Roids,
        Tonemapping,
        FlagPreview,
        Emissive
    };
};

struct AnimationID final
{
    enum
    {
        Idle, Swing, Chip, Putt,
        Celebrate, Disappoint,
        Impatient, IdleStand,
        Count,

    };
    static constexpr std::size_t Invalid = std::numeric_limits<std::size_t>::max();
};

struct SkipState final
{
    std::int32_t state = -1;
    bool wasSkipped = false;

    static constexpr float SkipTime = 1.f;
    float currentTime = 0.f;

    std::int32_t previousState = state;
    bool displayControllerMessage = false;
};

struct Avatar final
{
    bool flipped = false;
    cro::Entity model;
    cro::Attachment* hands = nullptr;
    std::array<std::size_t, AnimationID::Count> animationIDs = {};
    cro::Entity ballModel;
    std::uint32_t clubModelID = 0;
};

static inline std::int32_t courseOfTheMonth()
{
    //return 9 + (cro::SysTime::now().months() % 3);
    return cro::SysTime::now().months() - 1;
}

static inline float getOffsetRotation(float heightToGround)
{
    //TODO measure distance 
    static constexpr float PlayerDist = (PlayerFootOffset*PlayerFootOffset);

    float a = std::abs(heightToGround);
    const float c = std::sqrt((a * a) + PlayerDist);
    return std::asin(a / c) * cro::Util::Maths::sgn(heightToGround);
}


static inline float getWindMultiplier(float ballHeight, float distanceToPin)
{
    //this is the distance to the pin before the wind stops affecting the ball
    static constexpr float MinWind = 10.f;
    static constexpr float MaxWind = 32.f;

    static constexpr float MinHeight = 26.f;// 40.f;
    static constexpr float MaxHeight = 40.f;// 50.f;
    const float HeightMultiplier = std::clamp((ballHeight - MinHeight) / (MaxHeight - MinHeight), 0.f, 1.f);
    
    float multiplier = std::clamp((distanceToPin - MinWind) / (MaxWind - MinWind), 0.f, 1.f);
    return cro::Util::Easing::easeInCubic(multiplier) * (0.5f + (0.5f * HeightMultiplier));
}

static inline std::int32_t activeControllerID(std::int32_t bestMatch)
{
    /*
    We need to use whichever game controller is currently available when the
    deck is docked or using an external controller which may be hot-seat
    else the deck's internal controller overrides the input for the current player...
    */
    if (Social::isSteamdeck() && cro::GameController::getControllerCount() > 1)
    {
        return cro::GameController::getLastControllerID();
    }

    if (cro::GameController::isConnected(bestMatch))
    {
        return bestMatch;
    }

    for (auto i = 3; i >= 0; --i)
    {
        if (cro::GameController::isConnected(i))
        {
            return i;
        }
    }
    return 0;
}

bool hasPSLayout(std::int32_t controllerID);

template <typename T>
constexpr T interpolate(T a, T b, float t)
{
    auto diff = b - a;
    return a + (diff * t);
}

template <typename T>
constexpr T step(T s, T v)
{
    return v < s ? static_cast<T>(0) : static_cast<T>(1);
}

//WHY do I keep defining this? (It's also in Util::Maths) - the std library has this
static inline constexpr float clamp(float t)
{
    return std::min(1.f, std::max(0.f, t));
}

static inline constexpr float smoothstep(float edge0, float edge1, float x)
{
    float t = clamp((x - edge0) / (edge1 - edge0));
    return t * t * (3.f - 2.f * t);
}

static inline cro::FloatRect getAvatarBounds(std::uint8_t player)
{
    cro::FloatRect bounds = { 0.f, LabelTextureSize.y - (LabelIconSize.x * 4.f), LabelIconSize.x, LabelIconSize.y };
    bounds.left = LabelIconSize.x * (player % 2);
    bounds.bottom += LabelIconSize.y * (player / 2);
    return bounds;
}

static inline cro::Image cropAvatarImage(const std::string& path)
{
    cro::ImageArray<std::uint8_t> arr;
    if (arr.loadFromFile(path, true)
        && arr.getChannels() > 2)
    {
        auto inSize = arr.getDimensions();
        auto outSize = glm::uvec2(inSize.x / 2, inSize.y);

        cro::Image tmp;
        tmp.create(outSize.x, outSize.y, cro::Colour::White);

        //copy only the left half (mugshout *ought* to be 2:1, however we'll scale to square when rendering)
        for (auto y = 0u; y < outSize.y; ++y)
        {
            for (auto x = 0u; x < outSize.x; ++x)
            {
                auto i = y * inSize.x + x;
                cro::Colour c =
                {
                    arr[i * arr.getChannels()],
                    arr[i * arr.getChannels() + 1],
                    arr[i * arr.getChannels() + 2]
                };

                //transparency outside radius
                glm::vec2 position(x, y);
                const float halfSize = static_cast<float>(outSize.x / 2);
                const float alpha = (1.f - glm::smoothstep(halfSize - 3.5f, halfSize - 1.5f, glm::length(position - glm::vec2(outSize / 2u))));
                c.setAlpha(alpha);

                tmp.setPixel(x, y, c);
            }
        }

        return tmp;
    }
    return cro::Image();
}

static inline glm::quat rotationFromNormal(glm::vec3 normal)
{
    glm::vec3 rotationY = normal;
    glm::vec3 rotationX = glm::cross(glm::vec3(0.f, 1.f, 0.f), rotationY);
    glm::vec3 rotationZ = glm::cross(rotationY, rotationX);
    glm::mat3 rotation(rotationX.x, rotationY.x, rotationZ.x, rotationX.y, rotationY.y, rotationZ.y, rotationX.z, rotationY.z, rotationZ.z);
    return glm::quat_cast(rotation);
}

static inline glm::mat4 lookFrom(glm::vec3 eye, glm::vec3 target, glm::vec3 up)
{
    auto z = glm::normalize(eye - target);
    auto x = glm::normalize(glm::cross(up, z));
    auto y = glm::cross(z, x);

    auto rotation = glm::mat4(
        x.x, y.x, z.x, 0.f,
        x.y, y.y, z.y, 0.f,
        x.z, y.z, z.z, 0.f,
        0.f, 0.f, 0.f, 1.f
    );

    return rotation * glm::translate(glm::mat4(1.f), eye);
}

static inline glm::quat lookRotation(glm::vec3 eye, glm::vec3 target, glm::vec3 up = glm::vec3(0.f, 1.f, 0.f))
{
    auto forward = eye - target;
    CRO_ASSERT(glm::length2(forward) != 0, "");

    forward = glm::normalize(forward);
    auto right = glm::normalize(glm::cross(up, forward));
    CRO_ASSERT(!std::isnan(right.x), "Right vec is NaN");
    auto upNew = glm::normalize(glm::cross(forward, right));
    CRO_ASSERT(!std::isnan(upNew.x), "Up vec is NaN");

    glm::mat3 m(right, upNew, forward);
    return glm::normalize(glm::toQuat(m));
}

static inline float getViewScale(glm::vec2 size = GolfGame::getActiveTarget()->getSize())
{
    const float ratio = size.x / size.y;

    if (ratio < 1.7f)
    {
        //4:3
        return std::min(8.f, std::floor(size.x / 512.f));
    }

    if (ratio < 2.37f)
    {
        //widescreen - clamp at 6x for 4k
        return std::min(6.f, std::floor(size.x / 540.f));
    }

    //ultrawide
    return std::min(6.f, std::floor(size.y / 360.f));
}

static inline void togglePixelScale(SharedStateData& sharedData, bool on)
{
    if (on != sharedData.pixelScale)
    {
        sharedData.pixelScale = on;
        sharedData.antialias = on ? false : sharedData.multisamples != 0;

        //raise a window resize message to trigger callbacks
        auto size = cro::App::getWindow().getSize();
        auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
        msg->data0 = size.x;
        msg->data1 = size.y;
        msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;
    }
}

static inline void toggleAntialiasing(SharedStateData& sharedData, bool on, std::uint32_t samples)
{
    if (on != sharedData.antialias
        || samples != sharedData.multisamples)
    {
        sharedData.antialias = on;
        if (!on)
        {
            sharedData.multisamples = 0;
        }
        else
        {
            sharedData.pixelScale = false;
            sharedData.multisamples = samples;
        }

        auto size = cro::App::getWindow().getSize();
        auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
        msg->data0 = size.x;
        msg->data1 = size.y;
        msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;
    }
}

//static inline void saveAvatars(const SharedStateData& sd)
//{
//    cro::ConfigFile cfg("avatars");
//    for (const auto& player : sd.localConnectionData.playerData)
//    {
//        auto* avatar = cfg.addObject("avatar");
//        avatar->addProperty("name", player.name.empty() ? "Player" : reinterpret_cast<const char*>(player.name.toUtf8().c_str()));
//        avatar->addProperty("ball_id").setValue(player.ballID);
//        avatar->addProperty("hair_id").setValue(player.hairID);
//        avatar->addProperty("skin_id").setValue(player.skinID);
//        avatar->addProperty("flipped").setValue(player.flipped);
//        avatar->addProperty("flags0").setValue(player.avatarFlags[0]);
//        avatar->addProperty("flags1").setValue(player.avatarFlags[1]);
//        avatar->addProperty("flags2").setValue(player.avatarFlags[2]);
//        avatar->addProperty("flags3").setValue(player.avatarFlags[3]);
//        avatar->addProperty("cpu").setValue(player.isCPU);
//    }
//
//    auto path = cro::App::getPreferencePath() + "avatars.cfg";
//    cfg.save(path);
//}

static inline std::vector<cro::Vertex2D> getStrokeIndicatorVerts(bool decimated)
{
    auto endColour = TextGoldColour;
    endColour.setAlpha(0.f);
    const cro::Colour Grey(0.419f, 0.435f, 0.447f);

    if (decimated)
    {
        return
        {
            //gold
            cro::Vertex2D(glm::vec2(0.f, 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.f, -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.4575f, 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.4575f, -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.4575f, 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.4575f, -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.5f, 0.5f), endColour),
            cro::Vertex2D(glm::vec2(0.5f, -0.5f), endColour)
        };
    }

    return
    {
        //gold
        cro::Vertex2D(glm::vec2(0.f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.f, -0.5f), TextGoldColour),

        //grey
        cro::Vertex2D(glm::vec2(0.0188f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.0188f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.0188f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.0188f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.0288f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.0288f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.0288f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.0288f, -0.5f), TextGoldColour),


        //black
        cro::Vertex2D(glm::vec2(0.07f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.07f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.07f, 0.5f), LeaderboardTextDark),
        cro::Vertex2D(glm::vec2(0.07f, -0.5f), LeaderboardTextDark),

        cro::Vertex2D(glm::vec2(0.08f, 0.5f), LeaderboardTextDark),
        cro::Vertex2D(glm::vec2(0.08f, -0.5f), LeaderboardTextDark),

        cro::Vertex2D(glm::vec2(0.08f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.08f, -0.5f), TextGoldColour),

        //grey
        cro::Vertex2D(glm::vec2(0.1525f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.1525f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.1525f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.1525f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.1625f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.1625f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.1625f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.1625f, -0.5f), TextGoldColour),

        //black
        cro::Vertex2D(glm::vec2(0.245f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.245f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.245f, 0.5f), LeaderboardTextDark),
        cro::Vertex2D(glm::vec2(0.245f, -0.5f), LeaderboardTextDark),

        cro::Vertex2D(glm::vec2(0.255f, 0.5f), LeaderboardTextDark),
        cro::Vertex2D(glm::vec2(0.255f, -0.5f), LeaderboardTextDark),

        cro::Vertex2D(glm::vec2(0.255f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.255f, -0.5f), TextGoldColour),


        //grey
        cro::Vertex2D(glm::vec2(0.3175f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.3175f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.3175f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.3175f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.3275f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.3275f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.3275f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.3275f, -0.5f), TextGoldColour),


        //black
        cro::Vertex2D(glm::vec2(0.395f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.395f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.395f, 0.5f), LeaderboardTextDark),
        cro::Vertex2D(glm::vec2(0.395f, -0.5f), LeaderboardTextDark),

        cro::Vertex2D(glm::vec2(0.405f, 0.5f), LeaderboardTextDark),
        cro::Vertex2D(glm::vec2(0.405f, -0.5f), LeaderboardTextDark),

        cro::Vertex2D(glm::vec2(0.405f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.405f, -0.5f), TextGoldColour),

        //grey
        cro::Vertex2D(glm::vec2(0.4475f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.4475f, -0.5f), TextGoldColour),

        cro::Vertex2D(glm::vec2(0.4475f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.4475f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.4575f, 0.5f), Grey),
        cro::Vertex2D(glm::vec2(0.4575f, -0.5f), Grey),

        cro::Vertex2D(glm::vec2(0.4575f, 0.5f), TextGoldColour),
        cro::Vertex2D(glm::vec2(0.4575f, -0.5f), TextGoldColour),

        //gold
        cro::Vertex2D(glm::vec2(0.5f, 0.5f), endColour),
        cro::Vertex2D(glm::vec2(0.5f, -0.5f), endColour)
    };
}

static inline void createSwingputMeter(cro::Entity entity, InputParser& inputParser)
{
    static constexpr float Width = 4.f;
    static constexpr float Height = 40.f;

    entity.addComponent<cro::Transform>();
    entity.addComponent<cro::Drawable2D>().setVertexData(
        {
            cro::Vertex2D(glm::vec2(-Width, -Height), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  -Height), SwingputDark),
            cro::Vertex2D(glm::vec2(-Width,  -0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  -0.5f), SwingputDark),

            cro::Vertex2D(glm::vec2(-Width,  -0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(Width,  -0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(-Width,  0.5f), TextNormalColour),
            cro::Vertex2D(glm::vec2(Width,  0.5f), TextNormalColour),

            cro::Vertex2D(glm::vec2(-Width,  0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  0.5f), SwingputDark),
            cro::Vertex2D(glm::vec2(-Width, Height), SwingputDark),
            cro::Vertex2D(glm::vec2(Width,  Height), SwingputDark),


            cro::Vertex2D(glm::vec2(-Width, -Height), SwingputLight),
            cro::Vertex2D(glm::vec2(Width,  -Height), SwingputLight),
            cro::Vertex2D(glm::vec2(-Width, 0.f), SwingputLight),
            cro::Vertex2D(glm::vec2(Width,  0.f), SwingputLight),
        });
    entity.addComponent<cro::Callback>().active = true;
    entity.getComponent<cro::Callback>().setUserData<float>(0.f);
    entity.getComponent<cro::Callback>().function =
        [&](cro::Entity e, float dt)
        {
            auto& verts = e.getComponent<cro::Drawable2D>().getVertexData();
            float height = verts[14].position.y;
            float targetAlpha = -0.01f;

            if (inputParser.isSwingputActive())
            {
                height = inputParser.getSwingputPosition() * ((Height * 2.f) / MaxSwingputDistance);
                targetAlpha = 1.f;
            }

            auto& currentAlpha = e.getComponent<cro::Callback>().getUserData<float>();
            const float InSpeed = dt * 6.f;
            const float OutSpeed = inputParser.getPower() < 0.5 ? InSpeed : dt * 0.5f;
            if (currentAlpha <= targetAlpha)
            {
                currentAlpha = std::min(1.f, currentAlpha + InSpeed);
            }
            else
            {
                currentAlpha = std::max(0.f, currentAlpha - OutSpeed);
            }

            for (auto& v : verts)
            {
                v.colour.setAlpha(currentAlpha);
            }
            verts[14].position.y = height;
            verts[15].position.y = height;
        };

    entity.addComponent<cro::CommandTarget>().ID = CommandID::UI::UIElement;
    entity.addComponent<UIElement>().depth = 0.2f;
    entity.getComponent<UIElement>().relativePosition = { 1.f, 0.f };
    entity.getComponent<UIElement>().absolutePosition = { -10.f, 50.f };
}

//applies material data loaded in a model definition such as texture info to custom materials
static inline void applyMaterialData(const cro::ModelDefinition& modelDef, cro::Material::Data& dest, std::size_t matID = 0)
{
    if (auto* m = modelDef.getMaterial(matID); m != nullptr)
    {
        //skip over materials with alpha blend as they are
        //probably shadow materials if not explicitly glass
        if (m->blendMode == cro::Material::BlendMode::Alpha
            && !modelDef.hasTag(matID, "glass"))
        {
            dest = *m;
            return;
        }
        else
        {
            dest.blendMode = m->blendMode;
        }

        if (m->properties.count("u_diffuseMap"))
        {
            dest.setProperty("u_diffuseMap", cro::TextureID(m->properties.at("u_diffuseMap").second.textureID));
        }

        if (m->properties.count("u_maskMap"))
        {
            dest.setProperty("u_maskMap", cro::TextureID(m->properties.at("u_maskMap").second.textureID));
        }

        if (m->properties.count("u_colour")
            && dest.properties.count("u_colour"))
        {
            const auto* c = m->properties.at("u_colour").second.vecValue;
            glm::vec4 colour(c[0], c[1], c[2], c[3]);

            dest.setProperty("u_colour", colour);
        }

        if (m->properties.count("u_maskColour")
            && dest.properties.count("u_maskColour"))
        {
            const auto* c = m->properties.at("u_maskColour").second.vecValue;
            glm::vec4 colour(c[0], c[1], c[2], c[3]);

            dest.setProperty("u_maskColour", colour);
        }

        if (m->properties.count("u_subrect"))
        {
            const float* v = m->properties.at("u_subrect").second.vecValue;
            glm::vec4 subrect(v[0], v[1], v[2], v[3]);
            dest.setProperty("u_subrect", subrect);
        }
        else if (dest.properties.count("u_subrect"))
        {
            dest.setProperty("u_subrect", glm::vec4(0.f, 0.f, 1.f, 1.f));
        }

        dest.doubleSided = m->doubleSided;
        dest.animation = m->animation;
        dest.name = m->name;
    }
}

//finds an intersecting point on the water plane.
static inline bool planeIntersect(const glm::mat4& camTx, glm::vec3& result)
{
    //we know we have a fixed water plane here, so no need to generalise
    static constexpr glm::vec3 PlaneNormal(0.f, 1.f, 0.f);
    static constexpr glm::vec3 PlanePoint(0.f, WaterLevel, 0.f);
    static const float rd = glm::dot(PlaneNormal, PlanePoint);

    //this is normalised all the time the camera doesn't have a scale (it shouldn't)
    auto ray = -cro::Util::Matrix::getForwardVector(camTx);
    auto origin = glm::vec3(camTx[3]);

    if (glm::dot(PlaneNormal, ray) <= 0)
    {
        //parallel or plane is behind camera
        return false;
    }

    float pointDist = (rd - glm::dot(PlaneNormal, origin)) / glm::dot(PlaneNormal, ray);
    result = origin + (ray * pointDist);
    //clamp the result so overhead cams don't produce the effect of the water
    //plane 'zooming' off into the sunset :)
    //result.x = std::max(0.f, std::min(static_cast<float>(MapSize.x), result.x));
    //result.z = std::max(-static_cast<float>(MapSize.y), std::min(0.f, result.z));
    return true;
}

static inline std::pair<std::uint8_t, float> readMap(const cro::ImageArray<std::uint8_t>& img, float px, float py)
{
    auto size = glm::vec2(img.getDimensions());

    //if the point is out of bounds of our texture map, assume water
    const cro::FloatRect bounds(glm::vec2(0.f), size - glm::vec2(1.f));
    if (!bounds.contains(glm::vec2(px, py)))
    {
        return { TerrainID::Water, 0.f };
    }

    //I forget why our coords are float - this makes for horrible casts :(
    std::uint32_t x = static_cast<std::uint32_t>(std::min(size.x - 1.f, std::max(0.f, std::floor(px))));
    std::uint32_t y = static_cast<std::uint32_t>(std::min(size.y - 1.f, std::max(0.f, std::floor(py))));

    std::uint32_t stride = img.getChannels();
    //TODO we should have already asserted the format is RGBA elsewhere...

    auto index = (y * static_cast<std::uint32_t>(size.x) + x) * stride;

    std::uint8_t terrain = img[index] / 10;
    terrain = std::min(static_cast<std::uint8_t>(TerrainID::Stone), terrain);

    auto height = static_cast<float>(img[index + 1]) / 255.f;
    height *= MaxTerrainHeight;

    return { terrain, height };
}

static inline cro::Image loadNormalMap(std::vector<glm::vec3>& dst, const cro::Image& img)
{
    dst.resize(MapSize.x * MapSize.y, glm::vec3(0.f, 1.f, 0.f));

    std::uint32_t stride = 0;
    if (img.getFormat() == cro::ImageFormat::RGB)
    {
        stride = 3;
    }
    else if (img.getFormat() == cro::ImageFormat::RGBA)
    {
        stride = 4;
    }

    if (stride != 0)
    {
        auto pixels = img.getPixelData();
        for (auto i = 0u, j = 0u; i < dst.size(); ++i, j += stride)
        {
            dst[i] = { pixels[j], pixels[j + 2], pixels[j + 1] };
            dst[i] /= 255.f;

            dst[i].x = std::max(0.45f, std::min(0.55f, dst[i].x)); //clamps to +- 0.05f. I think. :3
            dst[i].z = std::max(0.45f, std::min(0.55f, dst[i].z));

            dst[i] *= 2.f;
            dst[i] -= 1.f;
            dst[i] = glm::normalize(dst[i]);
            dst[i].z *= -1.f;
        }
    }

    return img;
}

static inline cro::Image loadNormalMap(std::vector<glm::vec3>& dst, const std::string& path)
{
    static const cro::Colour DefaultColour(0x7f7fffff);

    auto extension = cro::FileSystem::getFileExtension(path);
    auto filePath = path.substr(0, path.length() - extension.length());
    filePath += "n" + extension;

    cro::Image img;
    if (!img.loadFromFile(filePath))
    {
        img.create(MapSize.x, MapSize.y, DefaultColour);
    }

    auto size = img.getSize();
    if (size != MapSize)
    {
        LogW << path << ": not loaded, image not " << MapSize << std::endl;
        img.create(MapSize.x, MapSize.y, DefaultColour);
    }

    loadNormalMap(dst, img);

    return img;
}

struct SkyboxMaterials final
{
    std::int32_t horizon = -1;
    std::int32_t horizonSun = -1;
    std::int32_t skinned = -1;
    std::int32_t glass = -1;
    
    //if loading the skybox finds a 
    //sun position this is set to true
    //it's then up to the current game
    //mode to act on it.
    bool requestLensFlare = false;
    glm::vec3 sunPos = glm::vec3(0.f);
    cro::Colour sunColour = cro::Colour::White;
};

//returns the entity with the cloud ring (so we can apply material)
//and sets requesting the lensflare effect if the sun position is found
static inline cro::Entity loadSkybox(const std::string& path, cro::Scene& skyScene, cro::ResourceCollection& resources, SkyboxMaterials& materials)
{
    auto skyTop = SkyTop;
    auto skyMid = TextNormalColour;
    float stars = 0.f;
    bool loadClouds = true;

    cro::ConfigFile cfg;

    struct PropData final
    {
        std::string path;
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 scale = glm::vec3(0.f);
        float rotation = 0.f;
        bool useSunlight = false;
    };
    std::vector<PropData> propModels;

    if (!path.empty()
        && cfg.loadFromFile(path))
    {
        {
            const auto& props = cfg.getProperties();
            for (const auto& p : props)
            {
                const auto& name = p.getName();
                if (name == "sky_top")
                {
                    skyTop = p.getValue<cro::Colour>();
                }
                else if (name == "sky_bottom")
                {
                    skyMid = p.getValue<cro::Colour>();
                }
                else if (name == "stars")
                {
                    stars = p.getValue<float>();
                }
                else if (name == "sun_pos")
                {
                    materials.requestLensFlare = true;
                    materials.sunPos = p.getValue<glm::vec3>();
                }
                else if (name == "sun_colour")
                {
                    materials.sunColour = p.getValue<cro::Colour>();
                }
                else if (name == "clouds")
                {
                    loadClouds = p.getValue<bool>();
                }
            }
        }

        const auto& objs = cfg.getObjects();
        for (const auto& obj : objs)
        {
            const auto& name = obj.getName();
            if (name == "prop")
            {
                auto& data = propModels.emplace_back();
                const auto& props = obj.getProperties();
                for (const auto& p : props)
                {
                    const auto& propName = p.getName();
                    if (propName == "model")
                    {
                        data.path = p.getValue<std::string>();
                    }
                    else if (propName == "position")
                    {
                        data.position = p.getValue<glm::vec3>();
                    }
                    else if (propName == "rotation")
                    {
                        data.rotation = p.getValue<float>();
                    }
                    else if (propName == "scale")
                    {
                        data.scale = p.getValue<glm::vec3>();
                    }
                    else if (propName == "skylight")
                    {
                        data.useSunlight = p.getValue<bool>();
                    }
                }
            }
        }
    }

    cro::ModelDefinition md(resources);
    for (const auto& model : propModels)
    {
        if (md.loadFromFile(model.path))
        {
            auto entity = skyScene.createEntity();
            entity.addComponent<cro::Transform>().setPosition(model.position);
            entity.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, model.rotation * cro::Util::Const::degToRad);
            entity.getComponent<cro::Transform>().setScale(model.scale);
            entity.setLabel(cro::FileSystem::getFileName(model.path));
            md.createModel(entity);

            std::int32_t matID = -1;
            if (model.useSunlight && materials.horizonSun != -1)
            {
                matID = materials.horizonSun;
            }
            else
            {
                matID = materials.horizon;
            }

            if (matID > -1)
            {
                auto material = resources.materials.get(matID);

                if (md.hasSkeleton() && materials.skinned > -1)
                {
                    material = resources.materials.get(materials.skinned);
                    entity.getComponent<cro::Skeleton>().play(0);
                }

                for (auto i = 0u; i < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++i)
                {
                    if (md.hasTag(i, "glass"))
                    {
                        if (materials.glass != -1)
                        {
                            auto glassMat = resources.materials.get(materials.glass);
                            applyMaterialData(md, glassMat, i);
                            entity.getComponent<cro::Model>().setMaterial(i, glassMat);
                        }
                        else
                        {
                            LogW << "Model " << model.path << " has glass material but no material available." << std::endl;
                            applyMaterialData(md, material, i);
                            entity.getComponent<cro::Model>().setMaterial(i, material);
                        }
                    }
                    else
                    {
                        applyMaterialData(md, material, i);
                        entity.getComponent<cro::Model>().setMaterial(i, material);
                    }
                }
            }

            //add auto rotation if this model is set to > 360
            if (model.rotation > 360)
            {
                float speed = (std::fmod(model.rotation, 360.f) * cro::Util::Const::degToRad) / 4.f;

                entity.addComponent<cro::Callback>().active = true;
                entity.getComponent<cro::Callback>().setUserData<float>(1.f);
                entity.getComponent<cro::Callback>().function =
                    [speed](cro::Entity e, float dt)
                {
                    auto currSpeed = speed * e.getComponent<cro::Callback>().getUserData<float>();
                    e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, currSpeed * dt);
                };
            }
        }
    }

    skyScene.enableSkybox();
    skyScene.setSkyboxColours(SkyBottom, skyMid, skyTop);
    skyScene.setStarsAmount(stars);

    cro::Entity cloudEnt;
    if (loadClouds &&
        md.loadFromFile("assets/golf/models/skybox/cloud_ring.cmt"))
    {
        auto entity = skyScene.createEntity();
        entity.addComponent<cro::Transform>();
        md.createModel(entity);

        const float speed = cro::Util::Const::degToRad / 4.f;
        entity.addComponent<cro::Callback>().active = true;
        entity.getComponent<cro::Callback>().setUserData<float>(1.f);
        entity.getComponent<cro::Callback>().function =
            [speed](cro::Entity e, float dt)
        {
            auto currSpeed = speed * e.getComponent<cro::Callback>().getUserData<float>();
            e.getComponent<cro::Transform>().rotate(cro::Transform::Y_AXIS, currSpeed * dt);
        };

        cloudEnt = entity;
    }

    return cloudEnt;
}

static inline void createFallbackModel(cro::Entity target, cro::ResourceCollection& resources)
{
    CRO_ASSERT(target.isValid(), "");
    static auto shaderID = resources.shaders.loadBuiltIn(cro::ShaderResource::Unlit, cro::ShaderResource::BuiltInFlags::DiffuseColour);
    auto& shader = resources.shaders.get(shaderID);

    static auto materialID = resources.materials.add(shader);
    auto material = resources.materials.get(materialID);
    material.setProperty("u_colour", cro::Colour::Magenta);

    static auto meshID = resources.meshes.loadMesh(cro::CubeBuilder(glm::vec3(0.25f)));
    auto meshData = resources.meshes.getMesh(meshID);

    target.addComponent<cro::Model>(meshData, material);
}

static inline void formatElevationString(float distance, cro::Text& target, bool imperial, bool decimal)
{
    if (imperial)
    {
        if (decimal)
        {
            std::stringstream ss;
            ss.precision(2);
            ss << "Elevation: ";

            distance *= ToYards;
            if (distance < 1 && distance > -1)
            {
                ss << std::fixed << ((distance * ToFeet) * ToInches);
                ss << "in";
            }
            else
            {
                ss << std::fixed << distance;
                ss << "yds";
            }

            target.setString(ss.str());
        }
        else
        {
            distance *= ToFeet;
            if (distance > 3 || distance < -3)
            {
                std::stringstream ss;
                ss.precision(1);
                ss << "Elevation: ";
                ss << std::fixed << distance;
                ss << "ft";

                target.setString(ss.str());
            }
            else
            {
                auto dist = static_cast<std::int32_t>(distance * ToInches);
                target.setString("Elevation: " + std::to_string(dist) + "in");
            }
        }
    }
    else
    {
        if (distance > 5 || distance < -5)
        {
            auto dist = static_cast<std::int32_t>(std::round(distance));
            target.setString("Elevation: " + std::to_string(dist) + "m");
        }
        else
        {
            if (decimal)
            {
                std::stringstream ss;
                ss.precision(2);
                ss << "Elevation: ";
                ss << std::fixed << distance;
                ss << "m";

                target.setString(ss.str());
            }
            else
            {
                auto dist = static_cast<std::int32_t>(distance * 100.f);
                target.setString("Elevation: " + std::to_string(dist) + "cm");
            }
        }
    }
}

static inline void formatDistanceString(float distance, cro::Text& target, bool imperial, bool decimal, bool onGreen, bool isTarget = false)
{
    const std::string Prefix = isTarget ? "Target: " : "Pin: ";

    if (imperial)
    {
        //if (distance > 7) //TODO this should read the putter value (?)
        if (!onGreen)
        {
            auto dist = static_cast<std::int32_t>(std::round(distance * ToYards));
            target.setString(Prefix + std::to_string(dist) + "yds");
        }
        else
        {
            /*float dist = std::ceil((distance * ToYards) * 100.f) / 100.f;
            std::stringstream ss;
            ss.precision(2);
            ss << "Distance: ";
            ss << std::fixed << dist;
            ss << "yds";

            target.setString(ss.str());*/

            if (decimal)
            {
                std::stringstream ss;
                ss.precision(2);
                ss << "Distance: ";

                if (!onGreen)
                {
                    ss << std::fixed << (distance * ToYards);
                    ss << "yds";
                }
                else
                {
                    ss << std::fixed << ((distance * ToYards) * ToFeet);
                    ss << "ft";
                }
                target.setString(ss.str());
            }
            else
            {
                distance *= ToFeet;
                if (distance > 1)
                {
                    std::stringstream ss;
                    ss.precision(1);
                    ss << "Distance: ";
                    ss << std::fixed << distance;
                    ss << "ft";

                    target.setString(ss.str());

                    /*auto dist = static_cast<std::int32_t>(distance);
                    target.setString("Distance: " + std::to_string(dist) + "ft");*/
                }
                else
                {
                    auto dist = static_cast<std::int32_t>(distance * ToInches);
                    target.setString("Distance: " + std::to_string(dist) + "in");
                }
            }
        }
    }
    else
    {
        if (distance > 5)
        {
            auto dist = static_cast<std::int32_t>(std::round(distance));
            target.setString(Prefix + std::to_string(dist) + "m");
        }
        else
        {
            if (decimal)
            {
                std::stringstream ss;
                ss.precision(2);
                ss << "Distance: ";
                ss << std::fixed << distance;
                ss << "m";

                target.setString(ss.str());
            }
            else
            {
                auto dist = static_cast<std::int32_t>(distance * 100.f);
                target.setString("Distance: " + std::to_string(dist) + "cm");
            }
        }
    }
}

static inline glm::vec3 rgb2hsv(glm::vec3 c)
{
    constexpr glm::vec4 K = glm::vec4(0.f, -1.f / 3.f, 2.f / 3.f, -1.f);
    const glm::vec4 p = interpolate(glm::vec4(c.b, c.g, K.w, K.z), glm::vec4(c.g, c.b, K.x, K.y), step(c.b, c.g));
    const glm::vec4 q = interpolate(glm::vec4(p.x, p.y, p.w, c.r), glm::vec4(c.r, p.y, p.z, p.x), step(p.x, c.r));

    const float d = q.x - glm::min(q.w, q.y);
    constexpr float e = float(1.0e-10);
    return glm::vec3(glm::abs(q.z + (q.w - q.y) / (6.f * d + e)), d / (q.x + e), q.x);
}

static inline glm::vec3 hsv2rgb(glm::vec3 c)
{
    constexpr glm::vec4 K = glm::vec4(1.f, 2.f / 3.f, 1.f / 3.f, 3.f);
    const glm::vec3 p = glm::abs(glm::fract(glm::vec3(c.x, c.x, c.x) + glm::vec3(K.x, K.y, K.z)) * 6.f - glm::vec3(K.w, K.w, K.w));
    
    return c.z * interpolate(glm::vec3(K.x, K.x, K.x), glm::clamp(p - glm::vec3(K.x, K.x, K.x), 0.f, 1.f), c.y);
}

static inline cro::Colour getBeaconColour(float rotation)
{
    glm::vec3 c(1.f, 0.f, 1.f);
    c = rgb2hsv(c);
    c.x += rotation;
    c = hsv2rgb(c);

    return cro::Colour(c.r, c.g, c.b, 1.f);
}

static inline bool ndcVisible(glm::vec2 p)
{
    return p.x >= -1.f && p.x <= 1.f && p.y >= -1.f && p.y <= 1.f;
}