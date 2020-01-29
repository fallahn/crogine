/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include <crogine/graphics/MeshBuilder.hpp>

struct CMFHeader final
{
    std::uint8_t flags = 0;
    std::uint8_t arrayCount = 0;
    std::int32_t arrayOffset = 0;
    std::vector<std::int32_t> arraySizes;
};

class ImportedMeshBuilder final : public cro::MeshBuilder
{
public:
    ImportedMeshBuilder(const CMFHeader&, const std::vector<float>&, const std::vector<std::vector<std::uint32_t>>&);

private:
    const CMFHeader& m_header;
    const std::vector<float>& m_vboData;
    const std::vector<std::vector<std::uint32_t>> m_indexArrays;

    cro::Mesh::Data build() const override;

};