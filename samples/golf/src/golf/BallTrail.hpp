/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2024
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

#include <crogine/graphics/MeshData.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/gui/GuiClient.hpp>

//#include <crogine/core/HiResTimer.hpp>

#include <vector>
#include <array>

namespace cro
{
    class Scene;
    struct ResourceCollection;
    class Colour;
}

class BallTrail final //: public cro::GuiClient
{
public:
    BallTrail();

    void create(cro::Scene&, cro::ResourceCollection&, std::int32_t, bool courseSize = true);

    void setNext();
    void addPoint(glm::vec3, std::uint32_t = 0);

    void update();
    void reset();
    void setUseBeaconColour(bool);

private:

    struct Vertex final
    {
        glm::vec3 p = glm::vec3(0.f);
        glm::vec4 c = glm::vec4(1.f);

        Vertex() = default;
        Vertex(glm::vec3 pos, glm::vec4 colour) :p(pos), c(colour) {}
    };


    struct Trail final
    {
        std::vector<Vertex> vertexData;
        std::vector<std::uint32_t> indices;
        std::size_t front = 0;
        bool active = false;
        cro::Mesh::Data* meshData = nullptr;
    };


    static constexpr std::size_t BufferCount = 2;
    std::array<Trail, BufferCount> m_trails = {};
    std::size_t m_bufferIndex;

    glm::vec4 m_baseColour;

    //float m_insertTime = 0.f;
    //float m_updateTime = 0.f;
    //cro::HiResTimer m_insertTimer;
    //cro::HiResTimer m_updateTimer;
};