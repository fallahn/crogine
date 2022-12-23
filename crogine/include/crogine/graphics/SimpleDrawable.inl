/*-----------------------------------------------------------------------

Matt Marchant 2022
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

#pragma once

//by default assert (or do nothing) because we're using specialisation for accepted types.
template <typename T>
void cro::SimpleDrawable::setUniform(const std::string&, T)
{
    CRO_ASSERT(false, "invalid type");
}


template <>
inline void cro::SimpleDrawable::setUniform<float>(const std::string& name, float value)
{
    if (m_uniformValues.count(name))
    {
        m_uniformValues.at(name).second.type = UniformValue::Number;
        m_uniformValues.at(name).second.numberValue = value;
    }
    else
    {
        LogE << "SimpleDrawable: " << name << " doesn't exist in shader" << std::endl;
    }
}

template <>
inline void cro::SimpleDrawable::setUniform<glm::vec2>(const std::string& name, glm::vec2 value)
{
    if (m_uniformValues.count(name))
    {
        m_uniformValues.at(name).second.type = UniformValue::Vec2;
        m_uniformValues.at(name).second.vecValue[0] = value.x;
        m_uniformValues.at(name).second.vecValue[1] = value.y;
    }
    else
    {
        LogE << "SimpleDrawable: " << name << " doesn't exist in shader" << std::endl;
    }
}

template <>
inline void cro::SimpleDrawable::setUniform<glm::vec3>(const std::string& name, glm::vec3 value)
{
    if (m_uniformValues.count(name))
    {
        m_uniformValues.at(name).second.type = UniformValue::Vec3;
        m_uniformValues.at(name).second.vecValue[0] = value.x;
        m_uniformValues.at(name).second.vecValue[1] = value.y;
        m_uniformValues.at(name).second.vecValue[2] = value.z;
    }
    else
    {
        LogE << "SimpleDrawable: " << name << " doesn't exist in shader" << std::endl;
    }
}

template <>
inline void cro::SimpleDrawable::setUniform<glm::vec4>(const std::string& name, glm::vec4 value)
{
    if (m_uniformValues.count(name))
    {
        m_uniformValues.at(name).second.type = UniformValue::Vec4;
        m_uniformValues.at(name).second.vecValue[0] = value.r;
        m_uniformValues.at(name).second.vecValue[1] = value.g;
        m_uniformValues.at(name).second.vecValue[2] = value.b;
        m_uniformValues.at(name).second.vecValue[3] = value.a;
    }
    else
    {
        LogE << "SimpleDrawable: " << name << " doesn't exist in shader" << std::endl;
    }
}

template <>
inline void cro::SimpleDrawable::setUniform<glm::mat4>(const std::string& name, glm::mat4 value)
{
    if (m_uniformValues.count(name))
    {
        m_uniformValues.at(name).second.type = UniformValue::Mat4;
        m_uniformValues.at(name).second.matrixValue = value;
    }
    else
    {
        LogE << "SimpleDrawable: " << name << " doesn't exist in shader" << std::endl;
    }
}

template <>
inline void cro::SimpleDrawable::setUniform<cro::TextureID>(const std::string& name, cro::TextureID value)
{
    if (m_uniformValues.count(name))
    {
        m_uniformValues.at(name).second.type = UniformValue::Texture;
        m_uniformValues.at(name).second.textureID = value.textureID;
    }
    else
    {
        LogE << "SimpleDrawable: " << name << " doesn't exist in shader" << std::endl;
    }
}