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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Shader.hpp>

#include <string>
#include <unordered_map>

namespace cro
{    
    /*!
    \brief Manages the lifetime of shader programs.
    */
    class CRO_EXPORT_API ShaderResource final : public Detail::SDLResource
    {
    public:
        enum BuiltIn
        {
            GBuffer            = 0x79000000,
            Unlit              = 0x7A000000,
            BillboardUnlit     = 0x7B000000,
            VertexLit          = 0x7C000000,
            BillboardVertexLit = 0x7D000000,
            ShadowMap          = 0x7E000000,
            PBR                = 0x7F000000
        };

        enum BuiltInFlags
        {
            VertexColour      = 0x1,
            DiffuseColour     = 0x2,
            DiffuseMap        = 0x4,
            MaskMap           = 0x8,
            NormalMap         = 0x10,
            LightMap          = 0x20,
            Skinning          = 0x40,
            Subrects          = 0x80,
            ReceiveProjection = 0x100,
            RimLighting       = 0x200,
            DepthMap          = 0x400,
            RxShadows         = 0x800,
            AlphaClip         = 0x1000,
            LockRotation      = 0x2000,
            LockScale         = 0x4000
        };
        
        ShaderResource();

        ~ShaderResource() = default;
        ShaderResource(const ShaderResource&) = delete;
        ShaderResource(const ShaderResource&&) = delete;
        ShaderResource& operator = (const ShaderResource&) = delete;
        ShaderResource& operator = (const ShaderResource&&) = delete;

        /*!
        \brief Preloads a shader from given files on disk, if possible
        \param ID Integer value, such as an enumeration, to map to the loaded shader.
        \param vertex String containing path to file containing the source code for the vertex shader
        \param fragment String containing the path to the file containing the source code for
        the fragment shader.
        \returns true if successful, else false.
        */
        bool loadFromFile(std::int32_t id, const std::string& vertex, const std::string& fragment);

        /*!
        \brief Preloads a shader and maps it to the given ID from source code in memory

        \param vertex String containing the source for the vertex shader. Version directives
        are automatically prepended depending on the current platform so should be omitted.
        \param fragment String containing the source code for the fragment shader. Version
        directives are automatically prepended depending on the current platform so should be omitted.
        \param ID Integer value, such as an enumeration, to be mapped to the shader.
        \param defines Optional list of newline delimited defines used with given shader source
        \returns true if successful, else returns false
        */
        bool loadFromString(std::int32_t id, const std::string& vertex, const std::string& fragment, const std::string& defines = "");

        /*!
        \brief Preloads one of the built in shaders.
        \param type BuiltIn type for shader. Vertex lit supports normal mapping, mask mapping and skinning
        and is lit by in-scene entities which posess lights.
        \param flags A combination of BuiltInFlags bitwise ORd together indicating which shader features are requested
        \returns std::int32_t representing the ID of the preloaded shader if it succeeds, else returns -1. The returned ID
        can be used with get() to return an instance of the shader.
        */
        std::int32_t loadBuiltIn(BuiltIn type, std::int32_t flags);

        /*!
        \brief Returns the shader with the given ID if it exists, else the default system shader.
        */
        Shader& get(std::int32_t ID);

    private:

        Shader m_defaultShader;
        std::unordered_map<std::int32_t, Shader> m_shaders;
    };
}