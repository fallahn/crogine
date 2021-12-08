/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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
#include "../GolfGame.hpp"

#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Image.hpp>
#include <crogine/graphics/ModelDefinition.hpp>
#include <crogine/util/Constants.hpp>
#include <crogine/util/Easings.hpp>
#include <crogine/util/Matrix.hpp>

#include <cstdint>

static constexpr float CameraStrokeHeight = 3.2f;// 2.6f;
static constexpr float CameraPuttHeight = 0.3f;
static constexpr float CameraStrokeOffset = 7.5f;// 5.1f;
static constexpr float CameraPuttOffset = 0.8f;
static constexpr float FOV = 60.f * cro::Util::Const::degToRad;

static constexpr float GreenCamHeight = 3.f;
static constexpr float SkyCamHeight = 16.f;

static constexpr float BallPointSize = 1.4f;

static constexpr float MaxHook = -0.25f;
static constexpr float KnotsPerMetre = 1.94384f;
static constexpr float HoleRadius = 0.058f;

static constexpr float WaterLevel = -0.02f;
static constexpr float TerrainLevel = WaterLevel - 0.03f;
static constexpr float MaxTerrainHeight = 4.5f;

static constexpr float FlagRaiseDistance = 2.5f * 2.5f;
static constexpr float PlayerShadowOffset = 0.04f;

static constexpr glm::uvec2 MapSize(320u, 200u);
static constexpr glm::vec2 RangeSize(200.f, 250.f);
static constexpr glm::uvec2 LabelTextureSize(128u, 64u);
static constexpr glm::vec3 OriginOffset(static_cast<float>(MapSize.x / 2), 0.f, -static_cast<float>(MapSize.y / 2));

static const cro::Colour WaterColour(0.02f, 0.078f, 0.578f);

struct SpriteAnimID final
{
    enum
    {
        Swing = 0,
        Medal
    };
};

struct MixerChannel final
{
    enum
    {
        Music, Effects, Menu, Voice
    };
};

struct ShaderID final
{
    enum
    {
        Water = 100,
        Terrain,
        Cel,
        CelTextured,
        Course,
        Slope,
        Minimap,
        TutorialSlope,
        Wireframe,
        WireframeCulled,
        Weather,
        Transition
    };
};

struct AnimationID final
{
    enum
    {
        Idle, Swing,
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
static constexpr float ViewportHeightWide = 300.f;

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

static inline glm::vec2 calcVPSize()
{
    glm::vec2 size(GolfGame::getActiveTarget()->getSize());
    const float ratio = size.x / size.y;
    static constexpr float Widescreen = 16.f / 9.f;
    static constexpr float ViewportWidth = 640.f;

    return glm::vec2(ViewportWidth, ratio < Widescreen ? ViewportHeightWide : ViewportHeight);
}

static inline void setTexture(const cro::ModelDefinition& modelDef, cro::Material::Data& dest, std::size_t matID = 0)
{
    if (auto* m = modelDef.getMaterial(matID); m != nullptr)
    {
        if (m->properties.count("u_diffuseMap"))
        {
            dest.setProperty("u_diffuseMap", cro::TextureID(m->properties.at("u_diffuseMap").second.textureID));
        }
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
    terrain = std::min(static_cast<std::uint8_t>(TerrainID::Scrub), terrain);

    auto height = static_cast<float>(img.getPixelData()[index + 1]) / 255.f;
    height *= MaxTerrainHeight;

    switch (terrain)
    {
    default:
        return { terrain, height };
    case TerrainID::Scrub:
        return height > -(TerrainLevel - WaterLevel) ? std::make_pair(terrain, height) : std::make_pair(TerrainID::Water, height);
    }
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

//TODO use this for interpolating slope height on a height map
static inline float readHeightmap(glm::vec3 position, const std::vector<float>& heightmap)
{
    return 0.f;
    /*const auto lerp = [](float a, float b, float t) constexpr
    {
        return a + t * (b - a);
    };*/

    //const auto getHeightAt = [&](std::int32_t x, std::int32_t y)
    //{
    //    //heightmap is flipped relative to the world innit
    //    x = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, x));
    //    y = std::min(static_cast<std::int32_t>(IslandTileCount), std::max(0, y));
    //    return heightmap[(IslandTileCount - y) * IslandTileCount + x];
    //};

    //float posX = position.x / TileSize;
    //float posY = position.z / TileSize;

    //float intpart = 0.f;
    //auto modX = std::modf(posX, &intpart) / TileSize;
    //auto modY = std::modf(posY, &intpart) / TileSize; //normalise this for lerpitude

    //std::int32_t x = static_cast<std::int32_t>(posX);
    //std::int32_t y = static_cast<std::int32_t>(posY);

    //float topLine = lerp(getHeightAt(x, y), getHeightAt(x + 1, y), modX);
    //float botLine = lerp(getHeightAt(x, y + 1), getHeightAt(x + 1, y + 1), modX);
    //return lerp(topLine, botLine, modY) * MaxTerrainHeight;
}