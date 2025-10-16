/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

#include "GameConsts.hpp"
#include "Social.hpp"

#include <crogine/detail/OpenGL.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/MultiRenderTexture.hpp>
#include <crogine/graphics/Shader.hpp>

#include "../ErrorCheck.hpp"

#ifdef USE_GNS
#include "Input.hpp"
#endif

bool hasPSLayout(std::int32_t controllerID)
{
#ifdef USE_GNS
    //big picture mode means we have native controller handles so querying SteamInput is useless
    if (Social::isSteamdeck(false)) 
    {
        return Input::isPSController(cro::GameController::getSteamHandle(controllerID));  
    }
#endif
    return cro::GameController::hasPSLayout(controllerID);
}

void renderToNormalMap(const cro::Mesh::Data meshData, cro::Shader& normalShader, cro::MultiRenderTexture& normalMap)
{
    std::size_t normalOffset = 0;
    for (auto i = 0u; i < cro::Mesh::Attribute::Normal; ++i)
    {
        normalOffset += meshData.attributes[i].componentCount;
    }

    const auto& attribs = normalShader.getAttribMap();
    auto vaoCount = static_cast<std::int32_t>(meshData.submeshCount);

    std::vector<std::uint32_t> vaos(vaoCount);
    glCheck(glGenVertexArrays(vaoCount, vaos.data()));

    for (auto i = 0u; i < vaos.size(); ++i)
    {
        //oh boy are these some ugly casts
        glCheck(glBindVertexArray(vaos[i]));
        glCheck(glBindBuffer(GL_ARRAY_BUFFER, meshData.vboAllocation.bufferID));

        glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Attribute::Position]));
        glCheck(glVertexAttribPointer(attribs[cro::Mesh::Attribute::Position], 3,
            GL_FLOAT, GL_FALSE,
            static_cast<std::int32_t>(meshData.vertexSize),
            reinterpret_cast<void*>(meshData.vboAllocation.offset)));


        glCheck(glEnableVertexAttribArray(attribs[cro::Mesh::Attribute::Normal]));
        glCheck(glVertexAttribPointer(attribs[cro::Mesh::Attribute::Normal], 3,
            meshData.attributes[cro::Mesh::Attribute::Normal].glType,
            meshData.attributes[cro::Mesh::Attribute::Normal].glNormalised,
            static_cast<std::int32_t>(meshData.vertexSize),
            (void*)(meshData.vboAllocation.offset + (normalOffset * sizeof(float)))));

        glCheck(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshData.indexData[i].iboAllocation.bufferID));
    }

    glCheck(glUseProgram(normalShader.getGLHandle()));
    glCheck(glDisable(GL_CULL_FACE));


    //clear the alpha to 0 so unrendered areas have zero height
    //then the heightmap image can be compared and highest value used
    static const cro::Colour ClearColour(0x7f7fff00);
    //static const cro::Colour ClearColour(0x7f7fffff);
    normalMap.clear(ClearColour);
    for (auto i = 0u; i < vaos.size(); ++i)
    {
        glCheck(glBindVertexArray(vaos[i]));
        glCheck(glDrawElements(meshData.indexData[i].primitiveType,
            meshData.indexData[i].indexCount, meshData.indexData[i].format,
            reinterpret_cast<void*>(meshData.indexData[i].iboAllocation.offset)));
    }
    normalMap.display();

    glCheck(glBindVertexArray(0));
    glCheck(glDeleteVertexArrays(vaoCount, vaos.data()));
}