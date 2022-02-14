/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#include <crogine/detail/glm/vec4.hpp>

#include <polyvox/MarchingCubesSurfaceExtractor.h>

#include <cstdint>
#include <vector>

namespace pv = PolyVox;

namespace Voxel
{
    struct GLVertex final
    {
        pv::Vector3DFloat position;
        glm::vec4 colour = glm::vec4(1.f);
        pv::Vector3DFloat normal;
    };

    struct Data final
    {
        float density = 0.f; //normalised > 0.5 is solid
        std::int32_t terrain = 0; //TerrainID in golf.

        Data() = default;
        Data(float d, std::int32_t t = 0)
            : density(d), terrain(t) {}
    };

    class Mesh final
    {
    public:

        std::uint32_t addVertex(const pv::MarchingCubesVertex<Data>&);
        void addTriangle(std::uint32_t, std::uint32_t, std::uint32_t);
        void setOffset(const pv::Vector3DInt32& offset) {};

        const std::vector<GLVertex>& getVertexData() const { return m_vertices; }
        const std::vector<std::uint32_t>& getIndexData() const { return m_indices; }

        void clear();

    private:
        std::vector<GLVertex> m_vertices;
        std::vector<std::uint32_t> m_indices;
        std::vector<Data> m_data;
    };

    class ExtractionController final
    {
    public:
        using DensityType = float;
        using MaterialType = std::int32_t;

        DensityType convertToDensity(Data voxel) const
        {
            return voxel.density;
        }

        MaterialType convertToMaterial(Data voxel) const
        {
            return voxel.terrain;
        }

        Data blendMaterials(Data a, Data b, float weight) const
        {
            return weight < 0.5f ? a : b;
        }

        DensityType getThreshold() const
        {
            return m_threshold;
        }

    private:
        float m_threshold = 0.5f;
    };
}