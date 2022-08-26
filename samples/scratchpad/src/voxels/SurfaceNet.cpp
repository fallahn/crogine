#include "IsoSurface.hpp"

#include <array>

std::vector<std::int32_t> SurfaceNet::m_cubeEdges;
std::vector<std::int32_t> SurfaceNet::m_edgeTable;

Mesh SurfaceNet::calculate(const std::vector<float>& volume, glm::ivec3 dimensions)
{
    if (m_cubeEdges.empty() || m_edgeTable.empty())
    {
        m_cubeEdges.resize(24);
        m_edgeTable.resize(256);

        auto edgeIndex = 0;
        for (auto i = 0; i < 8; ++i)
        {
            for (auto j = 1; j < 4; j = (j << 1))
            {
                auto p = i ^ j;
                if (i <= p)
                {
                    m_cubeEdges[edgeIndex++] = i;
                    m_cubeEdges[edgeIndex++] = p;
                }
            }
        }

        for (auto i = 0u; i < m_edgeTable.size(); ++i)
        {
            auto em = 0;
            for (auto j = 0u; j < m_cubeEdges.size(); j += 2)
            {
                std::int32_t a = !(i & (1 << m_cubeEdges[j]));
                std::int32_t b = !(i & (1 << m_cubeEdges[j + 1]));
                em |= a != b ? (1 << (j >> 1)) : 0;
            }
            m_edgeTable[i] = em;
        }
    }

    std::array<std::int32_t, 3u> R = 
    {
        1,
        dimensions[0] + 1,
        (dimensions[0] + 1) * (dimensions[1] + 1)
    };
    Mesh output;

    std::vector<std::int32_t> buffer(R[2] * 2);
    std::fill(buffer.begin(), buffer.end(), 0);

    std::int32_t bufferNumber = 1;
    std::array<float, 3u> x = {};
    std::int32_t n = 0;
    for (x[2] = 0; x[2] < dimensions[2] - 1; ++x[2], n += dimensions[0], bufferNumber ^= 1, R[2] = -R[2])
    {
        std::int32_t m = 1 + (dimensions[0] + 1) * (1 + bufferNumber * (dimensions[1] + 1));

        for (x[1] = 0; x[1] < dimensions[1] - 1; ++x[1], ++n, m += 2)
        {
            for (x[0] = 0; x[0] < dimensions[0] - 1; ++x[0], ++n, ++m)
            {
                std::int32_t mask = 0;
                std::int32_t g = 0;
                std::int32_t idx = n;

                for (auto k = 0; k < 2; ++k, idx += dimensions[0] * (dimensions[1] - 2))
                {
                    for (auto j = 0; j < 2; ++j, idx += dimensions[0] - 2)
                    {
                        std::array<float, 8u> grid = {};

                        for (auto i = 0; i < 2; ++i, ++g, ++idx)
                        {
                            float p = volume[idx];
                            grid[g] = p;
                            mask |= (p < 0) ? (1 << g) : 0;
                        }

                        if (mask == 0 || mask == 0xff)
                        {
                            continue;
                        }

                        auto edgeMask = m_edgeTable[mask];
                        glm::vec3 v(0.f);
                        std::int32_t edgeCount = 0;

                        for (auto i = 0; i < 12; ++i)
                        {
                            if (!(edgeMask & (1 << i)))
                            {
                                continue;
                            }
                            ++edgeCount;

                            auto e0 = m_cubeEdges[(i << 1)];
                            auto e1 = m_cubeEdges[(i << 1) + 1];
                            float g0 = grid[e0];
                            float g1 = grid[e1];
                            float t = g0 - g1;

                            //if (std::abs(t) > 1e-6) //hack around this being ambiguoug on xcode
                            {
                                t = g0 / t;
                            }
                            //else
                            {
                                continue;
                            }

                            for (auto q = 0, r = 1; q < 3; ++q, r<<=1)
                            {
                                std::int32_t a = e0 & r;
                                std::int32_t b = e1 & r;
                                if (a != b)
                                {
                                    v[q] += a ? 1.f - t : t;
                                }
                                else
                                {
                                    v[q] += a ? 1.f : 0;
                                }
                            }

                            float s = 1.f / edgeCount;

                            for(auto l = 0; l < 3; ++l)
                            {
                                v[l] = x[l] + s * v[l];
                            }
                            buffer[m] = static_cast<std::int32_t>(output.vertices.size());
                            output.vertices.push_back(v);


                            for(auto l = 0; l < 3; ++l)
                            {
                                if (!(edgeMask & (1 << l)))
                                {
                                    continue;
                                }

                                std::int32_t iu = (l + 1) % 3;
                                std::int32_t iv = (l + 2) % 3;
                                if (x[iu] == 0 || x[iv] == 0)
                                {
                                    continue;
                                }

                                std::int32_t du = R[iu];
                                std::int32_t dv = R[iv];

                                if (mask & 1)
                                {
                                    output.indices.emplace_back(buffer[m], buffer[m - du - dv], buffer[m - dv]);
                                }
                                else
                                {
                                    output.indices.emplace_back(buffer[m], buffer[m - du - dv], buffer[m - du]);
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return output;
}