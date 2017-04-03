/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/ecs/components/Model.hpp>
#include <crogine/detail/Assert.hpp>

#include <algorithm>

using namespace cro;

Model::Model(Mesh::Data data, Material::Data material)
    : m_meshData(data)
{
    //sets all materials to given default
    bindMaterial(material);
    for (auto& mat : m_materials)
    {
        mat = material;
    }
}

void Model::setMaterial(std::size_t idx, Material::Data data)
{
    CRO_ASSERT(idx < m_materials.size(), "Index out of range");
    bindMaterial(data);
    m_materials[idx] = data;
}

//private
void Model::bindMaterial(Material::Data& material)
{
    //map attributes to material
    std::size_t pointerOffset = 0;
    for (auto i = 0u; i < Mesh::Attribute::Total; ++i)
    {
        if (material.attribs[i][Material::Data::Index] > -1)
        {
            //attrib exists in shader so map its size
            material.attribs[i][Material::Data::Size] = static_cast<int32>(m_meshData.attributes[i]);

            //calc the pointer offset for each attrib
            material.attribs[i][Material::Data::Offset] = static_cast<int32>(pointerOffset * sizeof(float));           
        }
        pointerOffset += m_meshData.attributes[i]; //count the offset regardless as the mesh may have more attributes than material
    }

    //sort by size
    std::sort(std::begin(material.attribs), std::end(material.attribs),
        [](const std::array<int32, 3>& ip,
            const std::array<int32, 3>& op)
    {
        return ip[Material::Data::Size] > op[Material::Data::Size];
    });

    //count attribs with size > 0
    int i = 0;
    while (material.attribs[i++][Material::Data::Size] != 0)
    {
        material.attribCount++;
    }
}
