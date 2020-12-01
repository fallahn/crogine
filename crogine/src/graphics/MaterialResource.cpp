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
    int32 autoID = std::numeric_limits<int32>::max();
}

Material::Data& MaterialResource::add(int32 ID, const Shader& shader)
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

int32 MaterialResource::add(const Shader& shader)
{
    int32 nextID = autoID--;
    add(nextID, shader);
    return nextID;
}

Material::Data& MaterialResource::get(int32 ID)
{
    CRO_ASSERT(m_materials.count(ID) > 0, "Material doesn't exist");
    return m_materials.find(ID)->second;
}