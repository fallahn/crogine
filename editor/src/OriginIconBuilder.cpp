/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2021
http://trederia.blogspot.com

crogine editor - Zlib license.

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

#include "OriginIconBuilder.hpp"

#include <crogine/detail/OpenGL.hpp>

#include <vector>

cro::Mesh::Data OriginIconBuilder::build() const
{
    //position/colour
    std::vector<float> vboData =
    {
        0.f,0.f,0.f,    1.f,0.f,0.f,
        0.2f,0.f,0.f,   1.f,0.f,0.f,
        0.f,0.f,0.f,    0.f,1.f,0.f,
        0.f,0.2f,0.f,   0.f,1.f,0.f,
        0.f,0.f,0.f,    0.f,0.f,1.f,
        0.f,0.f,0.2f,   0.f,0.f,1.f
    };

    cro::Mesh::Data retVal;
    retVal.attributes[cro::Mesh::Attribute::Position].size = 3;
    retVal.attributes[cro::Mesh::Attribute::Colour].size = 3;
    retVal.attributeFlags = cro::VertexProperty::Position | cro::VertexProperty::Colour;

    retVal.primitiveType = GL_LINES;
    retVal.vertexSize = getVertexSize(retVal.attributes);
    retVal.vertexCount = 6;
    createVBO(retVal, vboData);

    std::vector<std::uint8_t> indices = { 0,1,2,3,4,5 };
    retVal.submeshCount = 1;
    retVal.indexData[0].format = GL_UNSIGNED_BYTE;
    retVal.indexData[0].primitiveType = GL_LINES;
    retVal.indexData[0].indexCount = 6;
    createIBO(retVal, indices.data(), 0, 1);

    retVal.boundingBox[0] = glm::vec3(0.f);
    retVal.boundingBox[1] = glm::vec3(0.1f);

    retVal.boundingSphere.centre = glm::vec3(0.05f);
    retVal.boundingSphere.radius = 0.05f;

    return retVal;
}