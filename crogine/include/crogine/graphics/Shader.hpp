/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2025
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
#include <crogine/detail/SDLResource.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/MeshData.hpp>

#include <string>
#include <array>
#include <unordered_map>

namespace cro
{
    class Texture;

    /*!
    \brief Encapsulates a compiled/linked shader program.
    */
    class CRO_EXPORT_API Shader : public Detail::SDLResource
    {
    public:
        struct AttributeID final
        {
            enum
            {
                InstanceTransform = Mesh::Attribute::Total,
                InstanceNormal,

                Count
            };
        };        
        
        Shader();
        ~Shader();

        Shader(const Shader&) = delete;
        Shader(Shader&&) noexcept;
        Shader& operator = (const Shader&) = delete;
        Shader& operator = (Shader&&) noexcept;

        /*!
        \brief Attempts to load the shader source from given files on disk.
        \param vertexPath Path to the file containing vertex shader
        \param fragmentPath Path to file containing source for fragment shader
        \returns true on success, else returns false.
        NOTE In general this will fail on mobile platforms if trying to read from
        and asset, for example, embedded in an apk. In these cases include the
        shader sources as a raw string written in a header file or similar, and use
        loadFromString() to read it directly.
        */
        bool loadFromFile(const std::string& vertexPath, const std::string& fragmentPath);
        bool loadFromFile(const std::string& vertexPath, const std::string& geometryPath, const std::string& fragmentPath);

        /*!
        \brief Attempts to load the shader source from given strings
        \param vertex A string containing the source of the vertex shader
        \param fragment A string containing the source for the fragment shader
        \param defines Optional list of newline delimited defines for the GLSL preprocessor
        \returns true on success, else returns false.
        NOTE to ensure correct version number for specific platforms (100 for ES2,
        410 for desktop) these are automatically appended here. Therefore #version
        directives should be omitted from any source code.
        */
        bool loadFromString(const std::string& vertex, const std::string& fragment, const std::string& defines = "");
        bool loadFromString(const std::string& vertex, const std::string& geometry, const std::string& fragment, const std::string& defines);

        /*!
        \brief Returns the OpenGL handle for the shader program
        */
        std::uint32_t getGLHandle() const;

        /*!
        \brief Returns the shader's available vertex attributes mapped
        to the Mesh::Attribute layout
        */
        const std::array<std::int32_t, Shader::AttributeID::Count>& getAttribMap() const;

        /*!
        \brief Returns a list of active uniforms mapped to their location and their type
        */
        const std::unordered_map<std::string, std::pair<std::int32_t, std::uint32_t>>& getUniformMap() const;

        /*!
        \brief Returns the uniform ID of the given string if it exists, else returns -1
        \param uniformName A string containing the name of the requested uniform
        */
        std::int32_t getUniformID(const std::string& uniformName) const;


        /*!
        \brief Returns the type of the uniform with the given name, if it exists, as a GL_ENUM
        else return GL_INVALID_ENUM
        \param uniformName A string containing the name of the uniform to query
        */
        std::uint32_t getUniformType(const std::string& uniformName) const;

    private:
        bool loadFromSource(const char* v, const char* g, const char* f, const char* d);

        std::uint32_t m_handle;
        std::array<std::int32_t, AttributeID::Count> m_attribMap;
        bool fillAttribMap();
        void resetAttribMap();
        std::unordered_map<std::string, std::pair<std::int32_t, std::uint32_t>> m_uniformMap;
        void fillUniformMap();
        void resetUniformMap();
        std::string parseFile(const std::string&);
    };
}

/*!
\class cro::Shader

The shader class is used to encapsulate an OpenGL shader created from
a vertex program and fragment program. Shaders are generally managed
by the shader resource class, from which the built in types Unlit and
VertexLit can be requested. See the ShaderResource class for more detail.
While these built-in shaders take care of most of the basic rendering
tasks in crogine, custom shaders can (and often will) be necessary to
give a particular application that edge. While this is possible it is
also necessary to impose some restrictions on the shader code to
be able to maintain smooth transitions between the mobile and desktop
targets that crogine supports. To aid this the shader class automatically
supplies some information to the compiler so the it is not required
to add it to the source manually.

For example mobile targets support GLES2 with glsl version 100. Desktop
targets, on the other hand, require glsl version 410 for the provided
OpenGL 4.1 context. This is auto detected at compilation time and means
that you DO NOT need to add any version directives to your shader source
as they will be added automatically. It also means that there are a few
caveats, listed below:

Glsl version 100 uses the attribute and varying keywords for shader
input and output, whereas version 150 uses the keywords in and out.
To simplify the amount of code written and to prevent multiple versions
of the same shader being written the shader class provides some macros
which substitute the correct keywords at compile time. When writing
vertex shaders vertex attribute inputs should use the ATTRIBUTE macro.
This will expand to the correct attribute or in keyword when compiled

\code
ATTRIBUTE vec3 a_position;
\endcode

For vertex shader outputs the VARYING_OUT macro is used. This expands
to varying on glsl 100 or out in glsl 150

\code
VARYING_OUT vec3 v_worldPosition;
\endcode

Fragment shaders also have a selection of macros used for portability.
When using a varying variable or input in a fragment shader use the
VARYING_IN macro which expands to varying or in as appropriate.

\code
VARYING_IN vec3 v_worldPosition;
\endcode

Fragment shaders in glsl 150 also require a special out variable which
replaces the gl_FragColor variable of earlier versions of glsl. To declare
this use the OUTPUT macro which expands to the correct output in glsl 150
but is ignored in mobile shaders

\code
VARYING_IN vec3 v_worldPosition;
VARYING_IN vec3 v_normal;

OUTPUT
\endcode

When writing to the fragment shader output use the FRAG_OUT macro. This
will expand to gl_FragColor on mobile targets and a variable named
fragOut, declared by the OUTPUT macro on desktop targets.

\code
FRAG_OUT.rgb = v_vertexColour.rgb;
\endcode

Finally, when sampling textures, the texture2D() function from glsl 100
needs to be substituted with texture() in glsl 150. For this the TEXTURE
macro is supplied. This macro unfortunately doesn't cover other variations
such as texture3D, which will need to be taken in to consideration for
any custom shaders which may require it.

\code
FRAG_OUT = TEXTURE(u_sampler, v_texCoord);
\endcode

*/