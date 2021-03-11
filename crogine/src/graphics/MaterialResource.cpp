/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

#include <limits>

using namespace cro;

namespace
{
    std::int32_t autoID = std::numeric_limits<std::int32_t>::max();
}

Material::Data& MaterialResource::add(std::int32_t ID, const Shader& shader)
{
    if (m_materials.count(ID) > 0)
    {
        Logger::log("Material with this ID already exists!", Logger::Type::Warning);
        return m_materials.find(ID)->second;
    }

    Material::Data data;
    data.setShader(shader);

    m_materials.insert(std::make_pair(ID, data));
    return m_materials.find(ID)->second;
}

std::int32_t MaterialResource::add(const Shader& shader)
{
    std::int32_t nextID = autoID--;
    add(nextID, shader);
    return nextID;
}

Material::Data& MaterialResource::get(std::int32_t ID)
{
    CRO_ASSERT(m_materials.count(ID) > 0, "Material doesn't exist");
    return m_materials.find(ID)->second;
}