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
#include <crogine/detail/Types.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Shader.hpp>

#include <string>
#include <unordered_map>

namespace cro
{    
    /*!
    \brief Manages the lifetime of shader programs.

    The shader resource provides a range of pre-defined #include
    directives which can be used with shaders that are loaded with
    this resource manager.


    Vertex Shader Includes:
    -----------------------

    #include WVP_UNIFORMS
    provides:
        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;
        and
        uniform mat4 u_worldViewMatrix;
        uniform mat3 u_normalMatrix;
        if INSTANCING is not defined, else
        uniform mat4 u_viewMatrix;


    #include INSTANCE_ATTRIBS
    provides:
        vertex attributes for instanced matrix data:
        ATTRIBUTE mat4 a_instanceWorldMatrix;
        ATTRIBUTE mat3 a_instanceNormalMatrix;

    #include INSTANCE_MATRICES
    provides:
        mat4 worldMatrix
        mat4 worldViewMatrix
        mat3 normalMatrix

        Matrices are created from the given WVP uniforms
        and instance attributes.

    requires:
        WVP_UNIFORMS, INSTANCE_ATTRIBS


    #include SKIN_UNIFORMS
    provides:
        ATTRIBUTE vec4 a_boneIndices;
        ATTRIBUTE vec4 a_boneWeights;
        uniform mat4 u_boneMatrices[MAX_BONES];

        Adds both the vertex attributes and matrix uniform
        required for vertex skinning.

    #include SKIN_MATRIX
    provides:
        mat4 skinMatrix
        The matrix is created from the bone matrices, weights
        and indices provided by SKIN_UNIFORMS and is used to
        multiply the vertex position and normal data.
    requires:
        SKIN_UNIFORMS


    #include SHADOWMAP_UNIFORMS_VERT
    provides:
        #define MAX_CASCADES 4 if not otherwise defined
        uniform mat4 u_lightViewProjectionMatrix[MAX_CASCADES];
        uniform int u_cascadeCount = 1;

    #include SHADOWMAP_OUTPUTS
    provides:
        VARYING_OUT LOW vec4 v_lightWorldPosition[MAX_CASCADES];
        VARYING_OUT float v_viewDepth;

    #include SHADOWMAP_VERTEX_PROC
    provides:
        processing for the vertex data, which should be performed
        in main()

    requires:
        SHADOWMAP_UNIFORMS_VERT, SHADOWMAP_OUTPUTS
        also requires mat4 worldMatrix, mat4 worldViewMatrix and vec4 position


    Fragment Shader Includes:
    -------------------------

    #include SHADOWMAP_UNIFORMS_FRAG
    provides:
        #define MAX_CASCADES 4 if not already defined
        uniform sampler2DArray u_shadowMap;
        uniform int u_cascadeCount = 1;
        uniform float u_frustumSplits[MAX_CASCADES];

    #include SHADOWMAP_INPUTS
    provides:
        VARYING_IN LOW vec4 v_lightWorldPosition[MAX_CASCADES];
        VARYING_IN float v_viewDepth;

    #include PCF_SHADOWS
    provides:
        int getCascadeIndex()
        float shadowAmount(int cascadeIndex)
          or, if PBR is #defined
        float shadowAmount(int cascadeIndex, SurfaceProperties surfProp)

    requires:
        SHADOWMAP_UNIFORMS_FRAG, SHADOWMAP_INPUTS (fragment shader)

    \sa addInclude

    */
    class CRO_EXPORT_API ShaderResource final : public Detail::SDLResource
    {
    public:
        enum BuiltIn
        {
            PBRDeferred        = 0x76000000,
            VertexLitDeferred  = 0x77000000,
            Unlit              = 0x78000000,
            UnlitDeferred      = 0x79000000,
            BillboardUnlit     = 0x7A000000,
            VertexLit          = 0x7B000000,
            BillboardVertexLit = 0x7C000000,
            ShadowMap          = 0x7D000000,
            BillboardShadowMap = 0x7E000000,
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
            LockScale         = 0x4000,
            Instanced         = 0x8000,
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
        bool loadFromString(std::int32_t id, const std::string& vertex, const std::string& geom, const std::string& fragment, const std::string& defines);

        /*!
        \brief Preloads one of the built in shaders.
        \param type BuiltIn type for shader. Vertex lit supports normal mapping, mask mapping and skinning
        and is lit by in-scene entities which possess lights.
        \param flags A combination of BuiltInFlags bitwise ORd together indicating which shader features are requested
        \returns std::int32_t representing the ID of the preloaded shader if it succeeds, else returns -1. The returned ID
        can be used with get() to return an instance of the shader.
        */
        std::int32_t loadBuiltIn(BuiltIn type, std::int32_t flags);

        /*!
        \brief Returns the shader with the given ID if it exists, else the default system shader.
        */
        Shader& get(std::int32_t ID);

        /*!
        \brief Returns the shader mapped to the given string ID, if it exists, else the default system shader
        \see mapStringID()
        */
        Shader& get(const std::string& stringID);

        /*!
        \brief Returns true if a shader has already been assigned the given ID
        */
        bool hasShader(std::int32_t shaderID) const;

        /*!
        \brief Adds a string containing a portion of shader code to be used as an include.
        In lieu of an #include directive in GLSL the shader resource can be provided with
        pointers to strings which can be substituted for any #include directives found in
        source code. This function accepts a name for the include directive which, when
        encountered, will be substituted with the string pointed to in the map. For example
            addInclude("MyUniform", pchStr);
        will allow the shader resource to replace #include MyUniform with the source pointed
        to by pchStr.
        Note that this substitution is only performed on loadFromString() (currently) and
        that the string pointed to as the substitute must exist for as long as the shader
        resource manager is likely to compile shaders.
        \param inc The name of the include directive to replace
        \param source A pointer to the source code to insert.
        */
        void addInclude(const std::string& inc, const char* source);


        /*!
        \brief Maps the given string to the given shader ID.
        Material definition files can optionally contain a property
        named "shader_string_id". When loaded via a ModelDefinition
        instance the shader resource is searched to see if this string
        ID is mapped to a loaded shader and that shader is preferred
        over the default shader for the materialtype. In which case
        custom shaders should be first loaded into the resource then
        mapped to a string ID with this function before loading the
        model with the ModelDefinition instance.
        \param stringID the string to search from the model definition
        \param shaderID the already loaded shader ID to map to the string ID
        \returns true if successfully mapped else false if the string is
        already mapped to a shader or the shader with the given ID doesn't exist
        */
        bool mapStringID(const std::string& stringID, std::int32_t shaderID);

        /*!
        \brief Returns true if the requested mapping is available
        */
        bool hasStringID(const std::string& stringID) const { return m_stringMappings.count(stringID); }

    private:

        Shader m_defaultShader;
        std::unordered_map<std::int32_t, Shader> m_shaders;
        std::unordered_map<std::string, const char*> m_includes;

        std::unordered_map<std::string, std::int32_t> m_stringMappings;

        std::string parseIncludes(const std::string& src) const;
    };
}