/*-----------------------------------------------------------------------

Matt Marchant 2020
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

#include "Patch.hpp"

#include <array>

namespace
{
    const std::int32_t TessellationLevel = 10;
    const std::int32_t pointCountX = 3;
    const std::int32_t pointCountY = 3;
}

Patch::Patch(const Q3::Face& face, const std::vector<Q3::Vertex>& mapVerts, std::vector<float>& verts)
{
    std::int32_t patchXCount = (face.size[0] - 1) >> 1;
    std::int32_t patchYCount = (face.size[1] - 1) >> 1;

    for (auto y = 0; y < patchYCount; ++y)
    {
        for (auto x = 0; x < patchXCount; ++x)
        {
            std::array<Q3::Vertex, 9> quad = {};
            for (auto col = 0; col < pointCountY; ++col)
            {
                for (auto row = 0; row < pointCountX; ++row)
                {
                    quad[row * pointCountX + col] = mapVerts[face.firstVertIndex +
                                                    (y * 2 * face.size[0] + x * 2) +
                                                    row * face.size[0] + col];
                }
            }

            tessellate(quad, verts);
        }
    }

    //remove the final degen tri
    m_indices.pop_back();
}

//private
void Patch::tessellate(const std::array<Q3::Vertex, 9u>& quad, std::vector<float>& verts)
{
    std::vector<Q3::Vertex> vertTemp((TessellationLevel + 1) * (TessellationLevel + 1));

    //first column
    for (auto i = 0; i <= TessellationLevel; ++i)
    {
        float a = static_cast<float>(i) / TessellationLevel;
        float b = 1.f - a;

        vertTemp[i].position = quad[0].position * (b * b) +
                                quad[3].position * (2.f * b * a) +
                                quad[6].position * (a * a);

        vertTemp[i].uv1.x = quad[0].uv1.x + ((quad[6].uv1.x - quad[0].uv1.x) * a);
        vertTemp[i].uv1.y = quad[0].uv1.y + ((quad[6].uv1.y - quad[0].uv1.y) * a);
    }

    for (int i = 1; i <= TessellationLevel; ++i)
    {
        float a = static_cast<float>(i) / TessellationLevel;
        float b = 1.f - a;

        std::array<Q3::Vertex, pointCountX> temp;

        //extend the row
        for (auto j = 0, k = 0; j < pointCountX; ++j, k = pointCountX * j)
        {
            temp[j].position = quad[k].position * (b * b) +
                                quad[k + 1].position * (2.f * b * a) +
                                quad[k + 2].position * (a * a);


            temp[j].uv1.x = quad[k].uv1.x + ((quad[k+2].uv1.x - quad[k].uv1.x) * a);
            temp[j].uv1.y = quad[k].uv1.y + ((quad[k+2].uv1.y - quad[k].uv1.y) * a);
        }

        //then crete next column
        for (auto j = 0; j <= TessellationLevel; ++j)
        {
            float c = static_cast<float>(j) / TessellationLevel;
            float d = 1.f - c;

            auto idx = i * (TessellationLevel + 1) + j;
            vertTemp[idx].position = temp[0].position * (d * d) +
                                        temp[1].position * (2.f * d * c) +
                                        temp[2].position * (c * c);

            vertTemp[idx].uv1.x = temp[0].uv1.x + ((temp[2].uv1.x - temp[0].uv1.x) * c);
            vertTemp[idx].uv1.y = temp[0].uv1.y + ((temp[2].uv1.y - temp[0].uv1.y) * c);
        }
    }

    //push temp verts to vert output
    const std::size_t vertSize = 12; //maaaaagic number!!! Actually the number of floats in a vert
    std::uint32_t indexOffset = static_cast<std::uint32_t>(verts.size() / vertSize);
    for (const auto& vertex : vertTemp)
    {
        verts.push_back(vertex.position.x);
        verts.push_back(vertex.position.z);
        verts.push_back(-vertex.position.y);

        verts.push_back(static_cast<float>(vertex.colour[0]) / 255.f);
        verts.push_back(static_cast<float>(vertex.colour[1]) / 255.f);
        verts.push_back(static_cast<float>(vertex.colour[2]) / 255.f);
        verts.push_back(static_cast<float>(vertex.colour[3]) / 255.f);

        verts.push_back(vertex.normal.x);
        verts.push_back(vertex.normal.z);
        verts.push_back(-vertex.normal.y);

        verts.push_back(vertex.uv1.x);
        verts.push_back(vertex.uv1.y);
    }


    std::vector<std::uint32_t> indexTemp(TessellationLevel * (TessellationLevel + 1) * 2);
    for (auto row = 0; row < TessellationLevel; ++row)
    {
        for (auto col = 0; col <= TessellationLevel; ++col)
        {
            indexTemp[(row * (TessellationLevel + 1) + col) * 2 + 1] = row * (TessellationLevel + 1) + col;
            indexTemp[(row * (TessellationLevel + 1) + col) * 2] = (row + 1) * (TessellationLevel + 1) + col;
        }
    }

    //arrange indices in correct order and add offset into the vertex array
    const auto colCount = 2 * (TessellationLevel + 1);
    std::vector<std::uint32_t> indices;
    for (auto row = 0; row < TessellationLevel; ++row)
    {
        if (row > 0)
        {
            //create the first degen
            indices.push_back(indexTemp[row * 2 * (TessellationLevel + 1)] + indexOffset);
        }

        //create the strip
        for (auto col = 0; col < colCount; ++col)
        {
            indices.push_back(indexTemp[row * 2 * (TessellationLevel + 1) + col] + indexOffset);
        }

        //add the final degen
        indices.push_back(indices.back());
    }

    //if this is not the first quad then prepend a degen tri
    if (!m_indices.empty())
    {
        m_indices.push_back(indices[0]);
    }

    m_indices.insert(m_indices.end(), indices.begin(), indices.end());
}