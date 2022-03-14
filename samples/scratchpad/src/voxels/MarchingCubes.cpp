#include "IsoSurface.hpp"

#include <array>

namespace
{
#include "Tables.inl"
}

Mesh MarchingCubes::calculate(const std::vector<float>& volume, glm::ivec3 dimensions)
{
    Mesh output;
    std::int32_t n = 0;
    std::array<float, 8u> grid = {};
    std::array<std::int32_t, 12u> edges = {};
    std::array<std::int32_t, 3u> x = {};

    for (x[2] = 0; x[2] < dimensions[2] - 1; ++x[2], n += dimensions[0])
    {
        for (x[1] = 0; x[1] < dimensions[1] - 1; ++x[1], ++n)
        {
            for (x[0] = 0; x[0] < dimensions[0] - 1; ++x[0], ++n)
            {
                auto cubeIndex = 0;
                for (auto i = 0u; i < CubeVerts.size(); ++i)
                {
                    auto vert = glm::ivec3(CubeVerts[i]);
                    auto s = volume[n + vert[0] + dimensions[0] * (vert[1] + dimensions[1] * vert[2])];
                    grid[i] = s;
                    cubeIndex |= (s > 0) ? (1 << i) : 0;
                }

                auto edgeMask = EdgeTable[cubeIndex];
                if (edgeMask == 0)
                {
                    continue;
                }

                for (auto i = 0u; i < EdgeIndices.size(); ++i)
                {
                    if ((edgeMask & (1 << i)) == 0)
                    {
                        continue;
                    }

                    edges[i] = static_cast<std::int32_t>(output.vertices.size());

                    glm::vec3 newVert(0.f);
                    auto edgeIndex = EdgeIndices[i];
                    auto point0 = CubeVerts[edgeIndex[0]];
                    auto point1 = CubeVerts[edgeIndex[1]];
                    auto a = grid[edgeIndex[0]];
                    auto b = grid[edgeIndex[1]];
                    auto dist = a - b;
                    auto t = 0.f;

                    if (std::abs(dist) > 1e-6)
                    {
                        t = a / dist;
                    }

                    for (auto j = 0; j < 3; ++j)
                    {
                        newVert[j] = (x[j] + point0[j]) + t * (point1[j] - point0[j]);
                    }
                    output.vertices.push_back(newVert);
                }


                auto face = TriTable[cubeIndex];
                for (auto i = 0u; i < face.size(); i+=3)
                {
                    output.indices.emplace_back(edges[face[i]], edges[face[i+1]], edges[face[i+2]]);
                }
            }
        }
    }

    return output;
}