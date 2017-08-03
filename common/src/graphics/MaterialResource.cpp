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
    data.shader = shader.getGLHandle();

    //get the available attribs. This is sorted and culled
    //when added to a model according to the requirements of
    //the model's mesh
    const auto& shaderAttribs = shader.getAttribMap();
    for (auto i = 0u; i < shaderAttribs.size(); ++i)
    {
        data.attribs[i][Material::Data::Index] = shaderAttribs[i];
    }

    //check the shader for standard uniforms and map them if they exist
    const auto& uniformMap = shader.getUniformMap();
    for (auto& uniform : data.uniforms)
    {
        uniform = -1;
    }

    /*for (auto& optional : data.optionalUniforms)
    {
        optional = -1;
    }*/

    for (const auto& uniform : uniformMap)
    {
        if (uniform.first == "u_worldMatrix")
        {
            data.uniforms[Material::World] = uniform.second;
        }
        else if (uniform.first == "u_viewMatrix")
        {
            data.uniforms[Material::View] = uniform.second;
        }
        else if (uniform.first == "u_worldViewMatrix")
        {
            data.uniforms[Material::WorldView] = uniform.second;
        }
        else if (uniform.first == "u_projectionMatrix")
        {
            data.uniforms[Material::Projection] = uniform.second;
        }
        else if (uniform.first == "u_worldViewProjectionMatrix")
        {
            data.uniforms[Material::WorldViewProjection] = uniform.second;
        }
        else if (uniform.first == "u_normalMatrix")
        {
            data.uniforms[Material::Normal] = uniform.second;
        }
        else if (uniform.first == "u_cameraWorldPosition")
        {
            data.uniforms[Material::Camera] = uniform.second;
        }
        //these are optionally standard so they are added to 'optional' list to to mark that they exist
        //but not added as a property as they are not user settable - rather they are used internally by renderers
        else if (uniform.first == "u_boneMatrices[0]")
        {
            data.uniforms[Material::Skinning] = uniform.second;
            data.optionalUniforms[data.optionalUniformCount++] = Material::Skinning;
        }
        else if (uniform.first == "u_projectionMapMatrix[0]")
        {
            data.uniforms[Material::ProjectionMap] = uniform.second;
            data.optionalUniforms[data.optionalUniformCount++] = Material::ProjectionMap;
        }
        else if (uniform.first == "u_projectionMapCount")
        {
            data.uniforms[Material::ProjectionMapCount] = uniform.second;
            data.optionalUniforms[data.optionalUniformCount++] = Material::ProjectionMapCount;
        }
        else if (uniform.first == "u_lightViewProjectionMatrix")
        {
            data.uniforms[Material::ShadowMapLight] = uniform.second;
            data.optionalUniforms[data.optionalUniformCount++] = Material::ShadowMapLight;
        }
        else if (uniform.first == "u_shadowMap")
        {
            data.uniforms[Material::ShadowMapSampler] = uniform.second;
            data.optionalUniforms[data.optionalUniformCount++] = Material::ShadowMapSampler;
        }
        //else these are user settable uniforms - ie optional, but set by user such as textures
        else
        {
            //add to list of material properties
            data.properties.insert(std::make_pair(uniform.first, std::make_pair(uniform.second, Material::Property())));
        }
    }

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