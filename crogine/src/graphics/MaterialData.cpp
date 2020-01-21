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

#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Texture.hpp>
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