/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include "ChunkBuilder.hpp"
#include "ErrorCheck.hpp"


//private
cro::Mesh::Data ChunkBuilder::build() const
{
    cro::Mesh::Data data;

    data.attributes[cro::Mesh::Position] = 2;
    data.attributes[cro::Mesh::Colour] = 3;
    data.primitiveType = GL_TRIANGLE_STRIP;
    data.submeshCount = 1;
    data.vertexCount = 0;
    data.vertexSize = (data.attributes[cro::Mesh::Position] + data.attributes[cro::Mesh::Position]) * sizeof(float);
    data.indexData[0].format = GL_UNSIGNED_SHORT;
    data.indexData[0].indexCount = 0;
    data.indexData[0].primitiveType = data.primitiveType;

    glCheck(glGenBuffers(1, &data.vbo));
    glCheck(glGenBuffers(1, &data.indexData[0].ibo));

    return data;
}