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

#ifndef CRO_SHADER_RESOURCE_HPP_
#define CRO_SHADER_RESOURCE_HPP_

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
            Unlit = 0x7FFE0000,
            VertexLit = 0x7FFF0000
        };

        enum BuiltInFlags
        {
            VertexColour = 0x1,
            DiffuseColour = 0x2,
            DiffuseMap = 0x4,
            NormalMap = 0x8,
            LightMap = 0x10,
            Skinning = 0x20,
            Subrects = 0x40,
            ReceiveProjection = 0x80
        };
        
        ShaderResource();

        ~ShaderResource() = default;
        ShaderResource(const ShaderResource&) = delete;
        ShaderResource(const ShaderResource&&) = delete;
        ShaderResource& operator = (const ShaderResource&) = delete;
        ShaderResource& operator = (const ShaderResource&&) = delete;

        /*!
        \brief Preloads a shader from given files on disk, if possible
        \param vertex String containing path to file containing the source code for the vertex shader
        \param fragment String containing the path to the file containing the source code for
        the fragment shader.
        \param ID Integer value, such as an enumeration, to map to the load shader.
        \returns true if successful, else false.
        */
        bool preloadFromFile(const std::string& vertex, const std::string& fragment, int32 ID);

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
        bool preloadFromString(const std::string& vertex, const std::string& fragment, int32 ID, const std::string& defines = "");

        /*!
        \brief Preloads one of the built in shaders.
        \param type BuiltIn type for shader. Vertex lit supports normal mapping, mask mapping and skinning
        and is lit by in-scene entities which posess lights.
        \param flags A combination of BuiltInFlags bitwise ORd together indicating which shader features are requested
        \returns int32 representing the ID of the preloaded shader if it succeeds, else returns -1. The returned ID
        can be used with get() to return an instance of the shader.
        */
        int32 preloadBuiltIn(BuiltIn type, int32 flags);

        /*!
        \brief Returns the shader with the given ID if it exists, else the default system shader.
        */
        Shader& get(int32 ID);

    private:

        Shader m_defaultShader;
        std::unordered_map<int32, Shader> m_shaders;
    };
}


#endif //CRO_SHADER_RESOURCE_HPP_