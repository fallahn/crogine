/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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
#include "../GolfGame.hpp"

#include <Social.hpp>

#include <crogine/core/ConfigFile.hpp>
#include <crogine/ecs/Scene.hpp>
#include <crogine/ecs/components/Model.hpp>
#include <crogine/ecs/components/Callback.hpp>
#include <crogine/ecs/components/Transform.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/graphics/CubeBuilder.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Matrix.hpp>

#include <cstdint>

static constexpr glm::uvec2 MapSize(320u, 200u);
static constexpr glm::vec2 RangeSize(200.f, 250.f);

static constexpr float CameraStrokeHeight = 2.f;
static constexpr float CameraPuttHeight = 0.6f;// 0.3f;
static constexpr float CameraTeeMultiplier = 0.65f; //height reduced by this when not putting from tee
static constexpr float CameraStrokeOffset = 5.f;
static constexpr float CameraPuttOffset = 1.6f; //0.8f;
static constexpr glm::vec3 CameraBystanderOffset = glm::vec3(7.f, 2.f, 7.f);

static constexpr float PuttingZoom = 0.93f;
static constexpr float GolfZoom = 0.59f;

static constexpr float GreenFadeDistance = 0.8f;
static constexpr float CourseFadeDistance = 2.f;

static constexpr float GreenCamHeight = 3.f;
static constexpr float SkyCamHeight = 16.f;
static constexpr glm::vec3 DefaultSkycamPosition(MapSize.x / 2.f, SkyCamHeight, -static_cast<float>(MapSize.y) / 2.f);

static constexpr float BallPointSize = 1.4f;

static constexpr float MaxHook = -0.25f;
static constexpr float KnotsPerMetre = 1.94384f;
static constexpr float HoleRadius = 0.058f;

static constexpr float WaterLevel = -0.02f;
static constexpr float TerrainLevel = WaterLevel - 0.03f;
static constexpr float MaxTerrainHeight = 4.5f;

static constexpr float FlagRaiseDistance = 3.f * 3.f;
static constexpr float PlayerShadowOffset = 0.04f;

static constexpr float MinPixelScale = 1.f;
static constexpr float MaxPixelScale = 3.f;

static constexpr float PlaneHeight = 60.f;

static constexpr float IndicatorDarkness = 0.002f;
static constexpr float IndicatorLightness = 0.5f;

static constexpr glm::vec2 LabelIconSize(Social::IconSize);
static constexpr glm::uvec2 LabelTextureSize(160u, 64u + Social::IconSize);
static constexpr glm::vec3 OriginOffset(static_cast<float>(MapSize.x / 2), 0.f, -static_cast<float>(MapSize.y / 2));

static const cro::Colour WaterColour(0.02f, 0.078f, 0.578f);
static const cro::Colour SkyTop(0.678f, 0.851f, 0.718f);
static const cro::Colour SkyBottom(0.2f, 0.304f, 0.612f);
static const cro::Colour DropShadowColour(0.396f, 0.263f, 0.184f);

struct LobbyPager final
{
    cro::Entity rootNode;
    std::vector<cro::Entity> pages;
    std::vector<cro::Entity> slots;
    std::vector<std::uint64_t> lobbyIDs;

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
        Footstep
    };
};

struct MixerChannel final
{
    enum
    {
        Music, Effects, Menu,
        Voice, Vehicles,

        Count
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
    float nearFadeDistance = 2.f;
};

struct ShaderID final
{
    enum
    {
        Water = 100,
        Horizon,
        Terrain,
        Billboard,
        BillboardShadow,
        Cel,
        CelSkinned,
        CelTextured,
        CelTexturedInstanced,
        CelTexturedSkinned,
        ShadowMap,
        ShadowMapSkinned,
        Crowd,
        CrowdShadow,
        Cloud,
        Leaderboard,
        Player,
        Hair,
        Course,
        CourseGrid,
        Ball,
        Slope,
        Minimap,
        TutorialSlope,
        Wireframe,
        WireframeCulled,
        Weather,
        Transition,
        Trophy,
        Beacon,
        Bow,
        Noise,
        TreesetLeaf,
        TreesetBranch,
        TreesetShadow,
        TreesetLeafShadow,
        PuttAssist
    };
};

struct AnimationID final
{
    enum
    {
        Idle, Swing, Chip, Putt,
        Count
    };
};

static const std::array BallTints =
{
    cro::Colour(1.f,0.937f,0.752f), //default
    cro::Colour(1.f,0.364f,0.015f), //pumpkin
    cro::Colour(0.1176f, 0.2f, 0.45f), //planet
    cro::Colour(0.937f, 0.76f, 0.235f), //snail
    cro::Colour(0.015f,0.031f,1.f), //bowling
    cro::Colour(0.964f,1.f,0.878f) //snowman
};

static constexpr float ViewportHeight = 360.f;
static constexpr float ViewportHeightWide = 320.f;// 300.f;

static inline glm::vec3 interpolate(glm::vec3 a, glm::vec3 b, float t)
{
    auto diff = b - a;
    return a + (diff *t);
}

static inline constexpr float interpolate(float a, float b, float t)
{
    auto diff = b - a;
    return a + (diff * t);
}

static inline constexpr float clamp(float t)
{
    return std::min(1.f, std::max(0.f, t));
}

static inline constexpr float smoothstep(float edge0, float edge1, float x)
{
    float t = clamp((x - edge0) / (edge1 - edge0));
    return t * t * (3.f - 2.f * t);
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

static inline glm::vec2 calcVPSize()
{
    glm::vec2 size(GolfGame::getActiveTarget()->getSize());
    const float ratio = size.x / size.y;
    static constexpr float Widescreen = 16.f / 9.f;
    static constexpr float ViewportWidth = 640.f;

    return glm::vec2(ViewportWidth, ratio < Widescreen ? ViewportHeightWide : ViewportHeight);
}

static inline void togglePixelScale(SharedStateData& sharedData, bool on)
{
    if (on != sharedData.pixelScale)
    {
        sharedData.pixelScale = on;

        //raise a window resize message to trigger callbacks
        auto size = cro::App::getWindow().getSize();
        auto* msg = cro::App::getInstance().getMessageBus().post<cro::Message::WindowEvent>(cro::Message::WindowMessage);
        msg->data0 = size.x;
        msg->data1 = size.y;
        msg->event = SDL_WINDOWEVENT_SIZE_CHANGED;
    }
}

static inline void saveAvatars(const SharedStateData& sd)
{
    cro::ConfigFile cfg("avatars");
    for (const auto& player : sd.localConnectionData.playerData)
    {
        auto* avatar = cfg.addObject("avatar");
        avatar->addProperty("name", player.name.empty() ? "Player" : player.name.toAnsiString()); //hmmm shame we can't save the encoding here
        avatar->addProperty("ball_id").setValue(player.ballID);
        avatar->addProperty("hair_id").setValue(player.hairID);
        avatar->addProperty("skin_id").setValue(player.skinID);
        avatar->addProperty("flipped").setValue(player.flipped);
        avatar->addProperty("flags0").setValue(player.avatarFlags[0]);
        avatar->addProperty("flags1").setValue(player.avatarFlags[1]);
        avatar->addProperty("flags2").setValue(player.avatarFlags[2]);
        avatar->addProperty("flags3").setValue(player.avatarFlags[3]);
        avatar->addProperty("cpu").setValue(player.isCPU);
    }

    auto path = cro::App::getPreferencePath() + "avatars.cfg";
    cfg.save(path);
}

//applies material data loaded in a model definition such as texture info to custom materials
static inline void applyMaterialData(const cro::ModelDefinition& modelDef, cro::Material::Data& dest, std::size_t matID = 0)
{
    if (auto* m = modelDef.getMaterial(matID); m != nullptr)
    {
        //skip over materials with alpha blend as they are
        //probably glass or shadow materials
        if (m->blendMode == cro::Material::BlendMode::Alpha)
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

static inline std::pair<std::uint8_t, float> readMap(const cro::Image& img, float px, float py)
{
    auto size = glm::vec2(img.getSize());
    //I forget why our coords are float - this makes for horrible casts :(
    std::uint32_t x = static_cast<std::uint32_t>(std::min(size.x - 1.f, std::max(0.f, std::floor(px))));
    std::uint32_t y = static_cast<std::uint32_t>(std::min(size.y - 1.f, std::max(0.f, std::floor(py))));

    std::uint32_t stride = 4;
    //TODO we should have already asserted the format is RGBA elsewhere...
    if (img.getFormat() == cro::ImageFormat::RGB)
    {
        stride = 3;
    }

    auto index = (y * static_cast<std::uint32_t>(size.x) + x) * stride;

    std::uint8_t terrain = img.getPixelData()[index] / 10;
    terrain = std::min(static_cast<std::uint8_t>(TerrainID::Stone), terrain);

    auto height = static_cast<float>(img.getPixelData()[index + 1]) / 255.f;
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
        img.create(320, 300, DefaultColour);
    }

    auto size = img.getSize();
    if (size != MapSize)
    {
        LogW << path << ": not loaded, image not 320x200" << std::endl;
        img.create(320, 300, DefaultColour);
    }

    loadNormalMap(dst, img);

    return img;
}

//return the path to cloud sprites if it is found
static inline std::string loadSkybox(const std::string& path, cro::Scene& skyScene, cro::ResourceCollection& resources, std::int32_t materialID, std::int32_t skinMatID = -1)
{
    auto skyTop = SkyTop;
    auto skyMid = TextNormalColour;
    std::string  cloudPath;

    cro::ConfigFile cfg;

    struct PropData final
    {
        std::string path;
        glm::vec3 position = glm::vec3(0.f);
        glm::vec3 scale = glm::vec3(0.f);
        float rotation = 0.f;
    };
    std::vector<PropData> propModels;

    if (!path.empty()
        && cfg.loadFromFile(path))
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
            else if (name == "clouds")
            {
                cloudPath = p.getValue<std::string>();
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

            if (materialID > -1)
            {
                auto material = resources.materials.get(materialID);

                if (md.hasSkeleton() && skinMatID > -1)
                {
                    material = resources.materials.get(skinMatID);
                    entity.getComponent<cro::Skeleton>().play(0);
                }

                for (auto i = 0u; i < entity.getComponent<cro::Model>().getMeshData().submeshCount; ++i)
                {
                    applyMaterialData(md, material, i);
                    entity.getComponent<cro::Model>().setMaterial(i, material);
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

    return cloudPath;
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

static inline void formatDistanceString(float distance, cro::Text& target, bool imperial)
{
    static constexpr float ToYards = 1.094f;
    static constexpr float ToFeet = 3.281f;
    static constexpr float ToInches = 12.f;

    if (imperial)
    {
        if (distance > 7) //TODO this should read the putter value
        {
            auto dist = static_cast<std::int32_t>(distance * ToYards);
            target.setString("Pin: " + std::to_string(dist) + "yds");
        }
        else
        {
            distance *= ToFeet;
            if (distance > 1)
            {
                auto dist = static_cast<std::int32_t>(distance);
                target.setString("Distance: " + std::to_string(dist) + "ft");
            }
            else
            {
                auto dist = static_cast<std::int32_t>(distance * ToInches);
                target.setString("Distance: " + std::to_string(dist) + "in");
            }
        }
    }
    else
    {
        if (distance > 5)
        {
            auto dist = static_cast<std::int32_t>(distance);
            target.setString("Pin: " + std::to_string(dist) + "m");
        }
        else
        {
            auto dist = static_cast<std::int32_t>(distance * 100.f);
            target.setString("Distance: " + std::to_string(dist) + "cm");
        }
    }
}