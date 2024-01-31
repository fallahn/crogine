/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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
#include "shaders/Unlit.hpp"
#include "shaders/Billboard.hpp"
#include "shaders/VertexLit.hpp"
#include "shaders/ShadowMap.hpp"
#include "shaders/PBR.hpp"
#include "shaders/Deferred.hpp"
#ifdef PLATFORM_DESKTOP
#include "shaders/GBuffer.hpp"
#endif
#include "../detail/GLCheck.hpp"

using namespace cro;

namespace
{
#include "shaders/ShaderIncludes.inl"
    std::int32_t MAX_BONES = 0;
}

ShaderResource::ShaderResource()
{
    if (!m_defaultShader.loadFromString(Shaders::Default::Vertex, Shaders::Default::Fragment))
    {
        Logger::log("FAILED LOADING DEFAULT SHADER, SHADER RESOURCE INVALID STATE", Logger::Type::Error);
    }

    //register the default includes
    addInclude("WVP_UNIFORMS", WVPMatrices.c_str());

    addInclude("INSTANCE_ATTRIBS", InstanceAttribs.c_str());
    addInclude("INSTANCE_MATRICES", InstanceMatrices.c_str());

    addInclude("SKIN_UNIFORMS", SkinUniforms.c_str());
    addInclude("SKIN_MATRIX", SkinMatrix.c_str());

    addInclude("SHADOWMAP_UNIFORMS_VERT", ShadowmapUniformsVert.c_str());
    addInclude("SHADOWMAP_OUTPUTS", ShadowmapOutputs.c_str());
    addInclude("SHADOWMAP_VERTEX_PROC", ShadowmapVertProc.c_str());
    addInclude("SHADOWMAP_UNIFORMS_FRAG", ShadowmapUniformsFrag.c_str());
    addInclude("SHADOWMAP_INPUTS", ShadowmapInputs.c_str());
    addInclude("PCF_SHADOWS", PCFShadows.c_str());
    addInclude("FXAA", FXAA.c_str());
}

//public
bool ShaderResource::loadFromFile(std::int32_t ID, const std::string& vertex, const std::string& fragment)
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

bool ShaderResource::loadFromString(std::int32_t ID, const std::string& vertex, const std::string& fragment, const std::string& defines)
{
    if (m_shaders.count(ID) > 0)
    {
        Logger::log("Shader with this ID already exists!", Logger::Type::Error);
        return false;
    }

    auto pair = std::make_pair(ID, Shader());
    if (!m_includes.empty())
    {
        auto v = parseIncludes(vertex);
        auto f = parseIncludes(fragment);

        if (!pair.second.loadFromString(v, f, defines))
        {
            return false;
        }
    }
    else
    {
        if (!pair.second.loadFromString(vertex, fragment, defines))
        {
            return false;
        }
    }
    m_shaders.insert(std::move(pair));
    return true;
}

bool ShaderResource::loadFromString(std::int32_t ID, const std::string& vertex, const std::string& geom, const std::string& fragment, const std::string& defines)
{
    if (m_shaders.count(ID) > 0)
    {
        Logger::log("Shader with this ID already exists!", Logger::Type::Error);
        return false;
    }

    auto pair = std::make_pair(ID, Shader());
    if (!m_includes.empty())
    {
        auto v = parseIncludes(vertex);
        auto g = parseIncludes(geom);
        auto f = parseIncludes(fragment);

        if (!pair.second.loadFromString(v, g, f, defines))
        {
            return false;
        }
    }
    else
    {
        if (!pair.second.loadFromString(vertex, geom, fragment, defines))
        {
            return false;
        }
    }
    m_shaders.insert(std::move(pair));
    return true;
}

std::int32_t ShaderResource::loadBuiltIn(BuiltIn type, std::int32_t flags)
{
#ifdef PLATFORM_DESKTOP
    CRO_ASSERT(type >= BuiltIn::PBRDeferred && flags > 0, "Invalid type of flags value");
#else
    CRO_ASSERT(type >= BuiltIn::Unlit && flags > 0, "Invalid type of flags value");
#endif

    std::int32_t id = type | flags;

    //check not already loaded
    if (m_shaders.count(id) > 0)
    {
        return id;
    }

    //create shader defines based on flags
    std::string defines;
    bool needUVs = false;
    if (flags & BuiltInFlags::DiffuseMap)
    {
        needUVs = true;
        defines += "\n#define DIFFUSE_MAP";
    }
    if (flags & BuiltInFlags::NormalMap)
    {
        needUVs = true;
        defines += "\n#define BUMP";
    }
    if (flags & BuiltInFlags::MaskMap)
    {
        needUVs = true;
        defines += "\n#define MASK_MAP";
    }
    if (flags & BuiltInFlags::LightMap)
    {
        needUVs = true;
        defines += "\n#define LIGHTMAPPED";
    }
    if (flags & BuiltInFlags::VertexColour)
    {
        defines += "\n#define VERTEX_COLOUR";
    }
    if (flags & BuiltInFlags::Subrects)
    {
        defines += "\n#define SUBRECTS";
    }
    if (flags & BuiltInFlags::DiffuseColour)
    {
        defines += "\n#define COLOURED";
    }
    if (flags & BuiltInFlags::RimLighting)
    {
        defines += "\n#define RIMMING";
    }
    if (flags & BuiltInFlags::RxShadows)
    {
        defines += "\n#define RX_SHADOWS";
    }
    if (flags & BuiltInFlags::Skinning)
    {
        if (MAX_BONES == 0)
        {
            //query opengl for the limit (this can be pretty low on mobile!!)
            GLint maxVec;
            glCheck(glGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVec));
            MAX_BONES = maxVec / 4; //4 x 4-components make up a mat4.
            //we'll allow 64 vectors for other uniforms (cascaded maps take up a few)
            //64 / 4 = 16
            MAX_BONES = std::min(MAX_BONES - 16, 255); //VMs can incorrectly report this :(
            //MAX_BONES = 64;
            LOG("MAX BONES " + std::to_string(MAX_BONES), Logger::Type::Info);
        }
        defines += "\n#define SKINNED\n #define MAX_BONES " + std::to_string(MAX_BONES);
    }
    else if (flags & BuiltInFlags::ReceiveProjection)
    {
        //on mobile devices (which projection mapping is really aimed at) there are too
        //few vectors available for both bone matrices and projection matrices :(
        defines += "\n#define PROJECTIONS";
    }
    if (flags & BuiltInFlags::AlphaClip)
    {
        defines += "\n#define ALPHA_CLIP";
    }
    if (flags & BuiltInFlags::LockRotation)
    {
        defines += "\n#define LOCK_ROTATION";
    }
    if (flags & BuiltInFlags::LockScale)
    {
        defines += "\n#define LOCK_SCALE";
    }
    if (flags & BuiltInFlags::Instanced)
    {
        defines += "\n#define INSTANCING";
    }
    if (needUVs)
    {
        defines += "\n#define TEXTURED";
    }
    defines += "\n";

    bool success = false;
    switch (type)
    {
    default:
    case BuiltIn::BillboardVertexLit:
        defines += "#define VERTEX_LIT\n";
        [[fallthrough]];
    case BuiltIn::BillboardUnlit:
        success = loadFromString(id, Shaders::Billboard::Vertex, Shaders::Billboard::Fragment, defines);
        break;
    case BuiltIn::Unlit:
        success = loadFromString(id, Shaders::Unlit::Vertex, Shaders::Unlit::Fragment, defines);
        break;
    case BuiltIn::UnlitDeferred:
        success = loadFromString(id, Shaders::Unlit::Vertex, Shaders::Deferred::OITUnlitFragment, defines);
        break;
    case BuiltIn::VertexLit:
        success = loadFromString(id, Shaders::VertexLit::Vertex, Shaders::VertexLit::Fragment, defines);
        break;
    case BuiltIn::VertexLitDeferred:
        success = loadFromString(id, Shaders::VertexLit::Vertex, Shaders::Deferred::OITShadedFragment, defines);
        break;
    case BuiltIn::ShadowMap:
#ifdef PLATFORM_DESKTOP
        success = loadFromString(id, Shaders::ShadowMap::Vertex, Shaders::ShadowMap::FragmentDesktop, defines);
#else
        success = loadFromString(id, Shaders::ShadowMap::Vertex, Shaders::ShadowMap::FragmentMobile, defines);
#endif
        break;
    case BillboardShadowMap:
        defines += "#define SHADOW_MAPPING\n";
#ifdef PLATFORM_DESKTOP
        success = loadFromString(id, Shaders::Billboard::Vertex, Shaders::ShadowMap::FragmentDesktop, defines);
#else
        success = loadFromString(id, Shaders::Billboard::Vertex, Shaders::ShadowMap::FragmentMobile, defines);
#endif
        break;
    case BuiltIn::PBRDeferred:
        success = loadFromString(id, Shaders::Deferred::GBufferVertex, Shaders::Deferred::GBufferFragment, defines);
        break;
    case BuiltIn::PBR:
        success = loadFromString(id, Shaders::VertexLit::Vertex, Shaders::PBR::Fragment, defines);
        break;
    }

    if (success)
    {
        return id;
    }
    return -1;
}

Shader& ShaderResource::get(std::int32_t ID)
{
    if (m_shaders.count(ID) == 0)
    {
        Logger::log("Could not find shader with ID " + std::to_string(ID) + ", returning default shader", Logger::Type::Warning);
        return m_defaultShader;
    }
    return m_shaders.at(ID);// .second;
}

bool ShaderResource::hasShader(std::int32_t shaderID) const
{
    return m_shaders.count(shaderID) != 0;
}

void ShaderResource::addInclude(const std::string& include, const char* src)
{
    if (m_includes.count(include) != 0)
    {
        LogW << include << " already exists in shader resource and has been overwritten." << std::endl;
    }
    m_includes.insert(std::make_pair(include, src));
}

//private
std::string ShaderResource::parseIncludes(const std::string& src) const
{
    std::string ret;

    std::stringstream ss;
    ss << src;

    for (std::string line; std::getline(ss, line);)
    {
        if (auto pos = line.find("//"); pos != std::string::npos)
        {
            line = line.substr(0, pos);
        }

        if (line.find("#include") != std::string::npos)
        {
            auto separator = line.find_last_of(' ');

            if (separator != std::string::npos
                && separator != line.size() - 1)
            {
                auto includeName = line.substr(separator + 1);
                if (m_includes.count(includeName))
                {
                    ret += "\n";
                    ret += m_includes.at(includeName);
                    ret += "\n";
                }
                else
                {
                    LogW << line << ": include not found in shader resource" << std::endl;
                }
            }
            else
            {
                LogW << line << ": invalid include directive" << std::endl;
            }
        }
        else
        {
            ret += line + "\n";
        }
    }

    return ret;
}