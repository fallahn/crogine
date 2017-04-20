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

#include <crogine/graphics/ShaderResource.hpp>
#include "shaders/Default.hpp"
#include "shaders/VertexLit.hpp"

using namespace cro;

ShaderResource::ShaderResource()
{
    if (!m_defaultShader.loadFromString(Shaders::Default::Vertex, Shaders::Default::Fragment))
    {
        Logger::log("FAILED LOADING DEFAULT SHADER, SHADER RESOURCE INVALID STATE", Logger::Type::Error);
    }
}

//public
bool ShaderResource::preloadFromFile(const std::string& vertex, const std::string& fragment, int32 ID)
{
    if (m_shaders.count(ID) > 0)
    {
        Logger::log("Shader with this ID already exists!", Logger::Type::Error);
        return false;
    }
    
    auto pair = std::make_pair(ID, Shader());
    if (!pair.second.loadFromFile(vertex, fragment))
    {
        return false;
    }
    m_shaders.insert(std::move(pair));
    return true;
}

bool ShaderResource::preloadFromString(const std::string& vertex, const std::string& fragment, int32 ID)
{
    if (m_shaders.count(ID) > 0)
    {
        Logger::log("Shader with this ID already exists!", Logger::Type::Error);
        return false;
    }

    //we have to insert before loading else moving
    //shader will destroy it...
    m_shaders.insert(std::make_pair(ID, Shader()));
    if (!m_shaders[ID].loadFromString(vertex, fragment))
    {
        m_shaders.erase(ID);
        return false;
    }
    return true;
}

int32 ShaderResource::preloadBuiltIn(BuiltIn type, int32 flags)
{
    CRO_ASSERT(type >= BuiltIn::Unlit && flags > 0, "Invalid type of flags value");
    int32 id = type | flags;

    //TODO create shader defines based on flags

    if (preloadFromString(Shaders::VertexLit::Vertex, Shaders::VertexLit::Fragment, id))
    {
        return id;
    }
    return -1;
}

cro::Shader& ShaderResource::get(int32 ID)
{
    if (m_shaders.count(ID) == 0)
    {
        Logger::log("Could not find shader with ID " + std::to_string(ID) + ", returning default shader", Logger::Type::Warning);
        return m_defaultShader;
    }
    return m_shaders.find(ID)->second;
}