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

#include <crogine/graphics/MaterialResource.hpp>
#include <crogine/graphics/Shader.hpp>

#include <crogine/detail/Assert.hpp>

using namespace cro;

Material::Data& MaterialResource::add(int32 ID, const Shader& shader)
{
    if (m_materials.count(ID) > 0)
    {
        Logger::log("Material with this ID already exists!", Logger::Type::Error);
        return m_materials.find(ID)->second;
    }

    Material::Data data;
    data.shader = shader.getGLHandle();

    //get the available attribs. This is sorted and culled
    //when added to a model according to the requirements of
    //the model's mesh
    const auto& shaderAttribs = shader.getAttribMap();
    for (auto i = 0u; i < shaderAttribs.size(); ++i)
    {
        data.attribs[i][Material::Data::Index] = shaderAttribs[i];
    }

    m_materials.insert(std::make_pair(ID, data));
    return m_materials.find(ID)->second;
}

Material::Data MaterialResource::get(int32 ID) const
{
    CRO_ASSERT(m_materials.count(ID) > 0, "Material doesn't exist");
    return m_materials.find(ID)->second;
}