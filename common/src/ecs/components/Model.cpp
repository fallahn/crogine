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

}

//private
void Model::bindMaterial(Material::Data& material)
{
    //TODO init default material
    //map attributes to material
    //set all materials (up to mesh submesh size) to default
}