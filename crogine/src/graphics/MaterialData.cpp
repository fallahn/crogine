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

#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/core/Log.hpp>

using namespace cro;
using namespace cro::Material;

#ifdef CRO_DEBUG_
#define  VERIFY(x, y) exists(x, y)
#else
#define VERIFY(x, y)
#endif //CRO_DEBUG_

namespace
{
#ifdef CRO_DEBUG_
    void exists(const std::string& name, const Material::PropertyList& properties)
    {
        if (properties.count(name) == 0)
        {
            Logger::log("Property " + name + " doesn't exist in shader", Logger::Type::Warning);
        }
    }
#endif //CRO_DEBUG_
}

Property::Property()
{
        //lastVecValue[0] = 0.f;
        //lastVecValue[1] = 0.f;
        //lastVecValue[2] = 0.f;
        //lastVecValue[3] = 0.f;
}

void Data::setProperty(const std::string& name, float value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.numberValue = value;
        result->second.second.type = Property::Number;
    }
}

void Data::setProperty(const std::string& name, glm::vec2 value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        //result->second.second.lastVecValue[0] = result->second.second.vecValue[0];
        //result->second.second.lastVecValue[1] = result->second.second.vecValue[1];
        result->second.second.vecValue[0] = value.x;
        result->second.second.vecValue[1] = value.y;
        result->second.second.type = Property::Vec2;
    }
}

void Data::setProperty(const std::string& name, glm::vec3 value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.vecValue[0] = value.x;
        result->second.second.vecValue[1] = value.y;
        result->second.second.vecValue[2] = value.z;
        result->second.second.type = Property::Vec3;
    }
}

void Data::setProperty(const std::string& name, glm::vec4 value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.vecValue[0] = value.x;
        result->second.second.vecValue[1] = value.y;
        result->second.second.vecValue[2] = value.z;
        result->second.second.vecValue[3] = value.w;
        result->second.second.type = Property::Vec4;
    }
}

void Data::setProperty(const std::string& name, glm::mat4 value)
{
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.matrixValue = value;
        result->second.second.type = Property::Mat4;
    }
}

void Data::setProperty(const std::string& name, Colour value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.vecValue[0] = value.getRed();
        result->second.second.vecValue[1] = value.getGreen();
        result->second.second.vecValue[2] = value.getBlue();
        result->second.second.vecValue[3] = value.getAlpha();
        result->second.second.type = Property::Vec4;
    }
}

void Data::setProperty(const std::string& name, const Texture& value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.textureID = value.getGLHandle();
        result->second.second.type = Property::Texture;
    }
}

void Data::setProperty(const std::string& name, TextureID value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.textureID = value.textureID;
        result->second.second.type = Property::Texture;
    }
}

void Data::setProperty(const std::string& name, CubemapID value)
{
    VERIFY(name, properties);
    auto result = properties.find(name);
    if (result != properties.end())
    {
        result->second.second.textureID = value.textureID;
        result->second.second.type = Property::Cubemap;
    }
}

void Data::setShader(const Shader& s)
{
    //this will get remapped if the uniform location changes
    auto oldProperties = properties;

    properties.clear();
    optionalUniformCount = 0;

    shader = s.getGLHandle();

    //get the available attribs. This is sorted and culled
    //when added to a model according to the requirements of
    //the model's mesh
    const auto& shaderAttribs = s.getAttribMap();
    for (auto i = 0u; i < shaderAttribs.size(); ++i)
    {
        attribs[i][Material::Data::Index] = shaderAttribs[i];
    }

    //check the shader for standard uniforms and map them if they exist
    const auto& uniformMap = s.getUniformMap();
    for (auto& uniform : uniforms)
    {
        uniform = -1;
    }

    /*for (auto& optional : data.optionalUniforms)
    {
        optional = -1;
    }*/

    for (const auto& [uniform, handle] : uniformMap)
    {
        if (uniform == "u_worldMatrix")
        {
            uniforms[Material::World] = handle;
        }
        else if (uniform == "u_viewMatrix")
        {
            uniforms[Material::View] = handle;
        }
        else if (uniform == "u_worldViewMatrix")
        {
            uniforms[Material::WorldView] = handle;
        }
        else if (uniform == "u_viewProjectionMatrix")
        {
            uniforms[Material::ViewProjection] = handle;
        }
        else if (uniform == "u_projectionMatrix")
        {
            uniforms[Material::Projection] = handle;
        }
        else if (uniform == "u_worldViewProjectionMatrix")
        {
            uniforms[Material::WorldViewProjection] = handle;
        }
        else if (uniform == "u_normalMatrix")
        {
            uniforms[Material::Normal] = handle;
        }
        else if (uniform == "u_cameraWorldPosition")
        {
            uniforms[Material::Camera] = handle;
        }
        else if (uniform == "u_screenSize")
        {
            uniforms[Material::ScreenSize] = handle;
        }
        else if (uniform == "u_clipPlane")
        {
            uniforms[Material::ClipPlane] = handle;
        }
        //these are optionally standard so they are added to 'optional' list to to mark that they exist
        //but not added as a property as they are not user settable - rather they are used internally by renderers
        else if (uniform == "u_boneMatrices[0]")
        {
            uniforms[Material::Skinning] = handle;
            optionalUniforms[optionalUniformCount++] = Material::Skinning;
        }
        else if (uniform == "u_projectionMapMatrix[0]")
        {
            uniforms[Material::ProjectionMap] = handle;
            optionalUniforms[optionalUniformCount++] = Material::ProjectionMap;
        }
        else if (uniform == "u_projectionMapCount")
        {
            uniforms[Material::ProjectionMapCount] = handle;
            optionalUniforms[optionalUniformCount++] = Material::ProjectionMapCount;
        }
        else if (uniform == "u_lightViewProjectionMatrix")
        {
            uniforms[Material::ShadowMapProjection] = handle;
            optionalUniforms[optionalUniformCount++] = Material::ShadowMapProjection;
        }
        else if (uniform == "u_shadowMap")
        {
            uniforms[Material::ShadowMapSampler] = handle;
            optionalUniforms[optionalUniformCount++] = Material::ShadowMapSampler;
        }
        else if (uniform == "u_lightDirection")
        {
            uniforms[Material::SunlightDirection] = handle;
            optionalUniforms[optionalUniformCount++] = Material::SunlightDirection;
        }
        else if (uniform == "u_lightColour")
        {
            uniforms[Material::SunlightColour] = handle;
            optionalUniforms[optionalUniformCount++] = Material::SunlightColour;
        }
        else if (uniform == "u_reflectionMap")
        {
            uniforms[Material::ReflectionMap] = handle;
            optionalUniforms[optionalUniformCount++] = Material::ReflectionMap;
        }
        else if (uniform == "u_refractionMap")
        {
            uniforms[Material::RefractionMap] = handle;
            optionalUniforms[optionalUniformCount++] = Material::RefractionMap;
        }
        else if (uniform == "u_reflectionMatrix")
        {
            uniforms[Material::ReflectionMatrix] = handle;
            optionalUniforms[optionalUniformCount++] = Material::ReflectionMatrix;
        }
        //else these are user settable uniforms - ie optional, but set by user such as textures
        else
        {
            //add to list of material properties
            properties.insert(std::make_pair(uniform, std::make_pair(handle, Material::Property())));
        }
    }

    //remap existing properties if they appear in the new shader
    for (const auto& [name, prop] : oldProperties)
    {
        auto result = properties.find(name);
        if (result != properties.end())
        {
            result->second.second = prop.second;
        }
    }
}