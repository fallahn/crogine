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

#include "GameConsts.hpp"
#include "Social.hpp"
#include "CollisionMesh.hpp"
#include "Terrain.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>
#include <crogine/graphics/Shader.hpp>

#include "../ErrorCheck.hpp"

#ifdef USE_GNS
#include "Input.hpp"
#endif

bool hasPSLayout(std::int32_t controllerID)
{
#ifdef USE_GNS
    //big picture mode means we have native controller handles so querying SteamInput is useless
    if (Social::isSteamdeck(false)) 
    {
        return Input::isPSController(cro::GameController::getSteamHandle(controllerID));  
    }
#endif
    return cro::GameController::hasPSLayout(controllerID);
}

void ControllerEffect::trigger(std::int32_t controllerID, std::int32_t effect)
{
    const bool isPS = cro::GameController::hasPSLayout(controllerID);

    switch (effect)
    {
    default: break;
    case ControllerEffect::Firework:
        if (isPS)
        {
            cro::GameController::rumbleStart(controllerID, (std::numeric_limits<std::uint16_t>::max() / 5) * 3, 0, 150);
        }
        else
        {
            cro::GameController::rumbleStart(controllerID, 10000, (std::numeric_limits<std::uint16_t>::max() / 5) * 2, 150);
        }
        break;
    case ControllerEffect::HIO:
        if (isPS)
        {
            cro::GameController::rumbleStart(controllerID, (std::numeric_limits<std::uint16_t>::max() / 5) * 4, 0, 1500);
        }
        else
        {
            cro::GameController::rumbleStart(controllerID, 8000, (std::numeric_limits<std::uint16_t>::max() / 5) / 2, 1500);
        }
        break;
    case ControllerEffect::Bounce:
        if (isPS)
        {
            cro::GameController::rumbleStart(controllerID, 1000, (std::numeric_limits<std::uint16_t>::max() / 5) * 3, 100);
        }
        else
        {
            cro::GameController::rumbleStart(controllerID, 6000, (std::numeric_limits<std::uint16_t>::max() / 5) * 1, 150);
        }
        break;
    }
}

void renderToNormalMap(const cro::Mesh::Data meshData, cro::Shader& normalShader, cro::MultiRenderTexture& normalMap)
{
    std::size_t normalOffset = 0;
    for (auto i = 0u; i < cro::Mesh::Attribute::Normal; ++i)
    {
        normalOffset += meshData.attributes[i].componentCount;
    }

    const auto& attribs = normalShader.getAttribMap();
    auto vaoCount = static_cast<std::int32_t>(meshData.submeshCount);

    std::vector<std::uint32_t> vaos(vaoCount);
    glCheck(glGenVertexArrays(vaoCount, vaos.data()));

    for (auto i = 0u; i < vaos.size(); ++i)
    {
        //oh boy are these some ugly casts
        glCheck(glBindVertexArray(vaos[i]));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.bufferID));

        glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Attribute::Position]));
        glCheck(glVertexAttribPointer(attribs[cro::Mesh::Attribute::Position], 3,
            GL_FLOAT, GL_FALSE,
            static_cast<std::int32_t>(meshData.vertexSize),
            reinterpret_cast<void*>(meshData.vboAllocation.offset)));


        glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Attribute::Normal]));
        glCheck(glVertexAttribPointer(attribs[cro::Mesh::Attribute::Normal], 3,
            meshData.attributes[cro::Mesh::Attribute::Normal].glType,
            meshData.attributes[cro::Mesh::Attribute::Normal].glNormalised,
            static_cast<std::int32_t>(meshData.vertexSize),
            (void*)(meshData.vboAllocation.offset + (normalOffset * sizeof(float)))));

        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.bufferID));
    }

    glCheck(glUseProgram(normalShader.getGLHandle()));
    glCheck(glDisable(GL_CULL_FACE));


    //clear the alpha to 0 so unrendered areas have zero height
    //then the heightmap image can be compared and highest value used
    static const cro::Colour ClearColour(0x7f7fff00);
    //static const cro::Colour ClearColour(0x7f7fffff);
    normalMap.clear(ClearColour);
    for (auto i = 0u; i < vaos.size(); ++i)
    {
        glCheck(glBindVertexArray(vaos[i]));
        glCheck(glDrawElements(meshData.indexData[i].primitiveType,
            meshData.indexData[i].indexCount, meshData.indexData[i].format,
            reinterpret_cast<void*>(meshData.indexData[i].iboAllocation.offset)));
    }
    normalMap.display();

    glCheck(glBindVertexArray(0));
    glCheck(glDeleteVertexArrays(vaoCount, vaos.data()));
}

std::vector<cro::Vertex2D> getStrokeIndicatorVerts(bool decimated)
{
    auto endColour = TextGoldColour;
    endColour.setAlpha(0.f);
    const cro::Colour Grey(0.419f, 0.435f, 0.447f);

    if (decimated)
    {
        static constexpr std::array Offsets =
        {
            0.0001f, 0.0002f, 0.0004f, 0.0008f
        };

        return
        {
            //gold
            cro::Vertex2D(glm::vec2(0.f, 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.f, -0.5f), TextGoldColour),


            //05 - 0.045 0.055
            cro::Vertex2D(glm::vec2(0.045f - Offsets[3], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.045f - Offsets[3], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.045f - Offsets[3], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.045f - Offsets[3], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.055f - Offsets[3], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.055f - Offsets[3], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.055f - Offsets[3], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.055f - Offsets[3], -0.5f), TextGoldColour),


            //1 - 0.095 0.105
            cro::Vertex2D(glm::vec2(0.095f - Offsets[2], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.095f - Offsets[2], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.095f - Offsets[2], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.095f - Offsets[2], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.105f - Offsets[2], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.105f - Offsets[2], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.105f - Offsets[2], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.105f - Offsets[2], -0.5f), TextGoldColour),


            //15 - 0.145 0.155
            cro::Vertex2D(glm::vec2(0.145f - Offsets[1], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.145f - Offsets[1], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.145f - Offsets[1], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.145f - Offsets[1], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.155f - Offsets[1], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.155f - Offsets[1], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.155f - Offsets[1], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.155f - Offsets[1], -0.5f), TextGoldColour),


            //2 - 0.195 0.205
            cro::Vertex2D(glm::vec2(0.195f - Offsets[0], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.195f - Offsets[0], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.195f - Offsets[0], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.195f - Offsets[0], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.205f - Offsets[0], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.205f - Offsets[0], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.205f - Offsets[0], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.205f - Offsets[0], -0.5f), TextGoldColour),



            //25 - 0.245 0.255
            cro::Vertex2D(glm::vec2(0.245f, 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.245f, -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.245f, 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.245f, -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.255f, 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.255f, -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.255f, 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.255f, -0.5f), TextGoldColour),


            //3 - 0.295 0.305
            cro::Vertex2D(glm::vec2(0.295f + Offsets[0], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.295f + Offsets[0], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.295f + Offsets[0], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.295f + Offsets[0], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.305f + Offsets[0], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.305f + Offsets[0], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.305f + Offsets[0], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.305f + Offsets[0], -0.5f), TextGoldColour),


            //35 - 0.345 0.355
            cro::Vertex2D(glm::vec2(0.345f + Offsets[1], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.345f + Offsets[1], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.345f + Offsets[1], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.345f + Offsets[1], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.355f + Offsets[1], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.355f + Offsets[1], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.355f + Offsets[1], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.355f + Offsets[1], -0.5f), TextGoldColour),


            //4 - 0.395 0.405
            cro::Vertex2D(glm::vec2(0.395f + Offsets[2], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.395f + Offsets[2], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.395f + Offsets[2], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.395f + Offsets[2], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.405f + Offsets[2], 0.5f), Grey),
            cro::Vertex2D(glm::vec2(0.405f + Offsets[2], -0.5f), Grey),

            cro::Vertex2D(glm::vec2(0.405f + Offsets[2], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.405f + Offsets[2], -0.5f), TextGoldColour),



            //45 0.445 0. 455
            cro::Vertex2D(glm::vec2(0.445f + Offsets[3], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.445f + Offsets[3], -0.5f), TextGoldColour),

            cro::Vertex2D(glm::vec2(0.445f + Offsets[3], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.445f + Offsets[3], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.455f + Offsets[3], 0.5f), LeaderboardTextDark),
            cro::Vertex2D(glm::vec2(0.455f + Offsets[3], -0.5f), LeaderboardTextDark),

            cro::Vertex2D(glm::vec2(0.455f + Offsets[3], 0.5f), TextGoldColour),
            cro::Vertex2D(glm::vec2(0.455f + Offsets[3], -0.5f), TextGoldColour),




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


glm::vec3 getImpactPoint(glm::vec3 pos, glm::vec3& impulse, glm::vec3 windVec, glm::vec3 pin, CollisionMesh& collisionMesh, float dt)
{
    float groundHeight = -1.f;
    TerrainResult t;
    do
    {
        t = collisionMesh.getTerrain(pos);
        groundHeight = t.height;

        //TODO this is lifted from the BallSystem - would prefer not to replicate.
        const auto windHeight = std::clamp(groundHeight, 0.f, 60.f);
        const auto windMultiplier = getWindMultiplier(windHeight, glm::length(pin - pos)) * 1.36f;

        pos += impulse * dt;
        impulse += Gravity * dt;
        impulse += windVec * windMultiplier * dt;
    } while (pos.y > groundHeight);
    
    impulse = glm::reflect(impulse, t.normal) * Restitution[t.terrain];
    return pos;
}

//assumes the 2D points are already in map-space
std::vector<cro::Vertex2D> strokeIndicatorFromPoints(const std::vector<glm::vec2>& points, float offsetScale)
{
    //SIGH ofc these colours are the wrong way around when we switch to decimated...
    static constexpr std::array col = { cro::Colour(0.419f, 0.435f, 0.447f), cro::Colour(0.f,0.f,0.f)};
    const std::size_t ColourOffset = (points.size() - 9) / 2; //so we hack around by looking at size diff

    const float OffsetX = 1.f * offsetScale;
    const float OffsetY = 0.5f * offsetScale;

    //assumes we're using triangle strip
    CRO_ASSERT(!points.empty(), "");
    std::vector<cro::Vertex2D> ret;
    for (auto i = 0u; i < points.size() - 2; ++i)
    {
        ret.emplace_back(points[i] + glm::vec2(OffsetX, OffsetY), TextGoldColour);
        ret.emplace_back(points[i] + glm::vec2(OffsetX, -OffsetY), TextGoldColour);
        
        ret.emplace_back(points[i + 1] + glm::vec2(-OffsetX, OffsetY), TextGoldColour);
        ret.emplace_back(points[i + 1] + glm::vec2(-OffsetX, -OffsetY), TextGoldColour);
        

        const auto colIndex = (i + ColourOffset)%2;
        ret.emplace_back(points[i + 1] + glm::vec2(-OffsetX, OffsetY), col[colIndex]);
        ret.emplace_back(points[i + 1] + glm::vec2(-OffsetX, -OffsetY), col[colIndex]);

        ret.emplace_back(points[i+1] + glm::vec2(OffsetX, OffsetY), col[colIndex]);
        ret.emplace_back(points[i+1] + glm::vec2(OffsetX, -OffsetY), col[colIndex]);
    }

    //hack to hide the confusing last impact point
    for (auto i = 1; i < 5; ++i)
    {
        ret[ret.size() - i].colour = TextHighlightColour;
    }

    //add a tail
    /*const auto dir = points.back() - points[points.size() - 2];
    const auto tail = points.back() + dir;*/
    constexpr auto TailColour = 
        cro::Colour(TextGoldColour.getRed(), TextGoldColour.getGreen(), TextGoldColour.getBlue(), 0.f);

    ret.emplace_back(points[points.size() - 2] + glm::vec2(OffsetX, OffsetY), TextGoldColour);
    ret.emplace_back(points[points.size() - 2] + glm::vec2(OffsetX, -OffsetY), TextGoldColour);

    ret.emplace_back(points.back() + glm::vec2(0.f, OffsetY), TailColour);
    ret.emplace_back(points.back() + glm::vec2(0.f, -OffsetY), TailColour);

    return ret;
}