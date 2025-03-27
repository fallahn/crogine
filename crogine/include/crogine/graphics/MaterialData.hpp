/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2024
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
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/Shader.hpp>

#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/vec4.hpp>
#include <crogine/detail/glm/mat4x4.hpp>

#include <unordered_map>

namespace cro
{
    class Texture;
    class CubemapTexture;

    /*!
    \brief Allows assigning Texture handles directly to material properties
    */
    struct CRO_EXPORT_API TextureID final
    {
        std::uint32_t textureID = 0;
        
        TextureID() = default;
        explicit TextureID(std::uint32_t id, bool a = false) : textureID(id), m_isArray(a) {}
        explicit TextureID(const Texture&);
        TextureID(const TextureID&) = default;

        TextureID& operator = (std::uint32_t id);
        TextureID& operator = (const Texture& t);

        TextureID& operator = (const TextureID&) = default;

        bool isArray() const { return m_isArray; }

    private:
        bool m_isArray = false;
    };

    /*!
    \brief Allows assigning Cubemap handles directly to material properties
    */
    struct CRO_EXPORT_API CubemapID final
    {
        std::uint32_t textureID = 0;

        CubemapID() = default;
        explicit CubemapID(std::uint32_t id, bool a = false) : textureID(id), m_isArray(a) {}
        explicit CubemapID(const cro::CubemapTexture&);

        CubemapID& operator = (std::uint32_t id);
        CubemapID& operator = (const CubemapTexture& t);
        
        bool isArray() const { return m_isArray; }
    private:
        bool m_isArray = false;
    };

    namespace Material
    {
        /*
        \brief Used to map standardised uniform locations
        */
        enum Uniform
        {
            World = 0,
            View,
            WorldView,
            ViewProjection,
            Projection,
            WorldViewProjection,
            Normal,
            Camera,
            Skinning,
            ProjectionMap,
            ProjectionMapCount,
            ShadowMapProjection,
            ShadowMapSampler,
            CameraView, //used in billboard generation when casting shadows
            CascadeSplits,
            CascadeCount,
            SunlightDirection,
            SunlightColour,
            ScreenSize,
            ClipPlane,
            ReflectionMap,
            RefractionMap,
            ReflectionMatrix,
            SkyBox,
            Total
        };
        
        enum class BlendMode
        {
            None,
            Alpha,
            Multiply,
            Additive,
            Custom
        };

        struct CRO_EXPORT_API Property final
        {
            enum
            {
                None,
                Number,
                Vec2,
                Vec3,
                Vec4,
                Mat4,
                Texture,
                TextureArray,
                Cubemap,
                CubemapArray
            }type = None;


            union
            {
                float numberValue;
                float vecValue[4];
                std::uint32_t textureID = 0;
                glm::mat4 matrixValue;
            };

            //used to interpolate when rendering
            /*union
            {
                float lastNumberValue;
                float lastVecValue[4];
            };*/

            Property();
        };

        //allows looking up uniform name when paired location/value
        using PropertyList = std::unordered_map<std::string, std::pair<std::int32_t, Property>>;

        /*!
        \brief Material data held by a model component and used for rendering.
        This should be created exclusively through a MaterialResource instance,
        manually trying to configure this will lead to undefined behaviour
        */
        struct CRO_EXPORT_API Data final
        {
            enum
            {
                Index = 0,
                Size,
                Offset
            };

            /*!
            \brief Blend mode.
            This affects how the material is rendered (and when).
            By default a blend mode of None will render the geometry
            with this material first, with depth testing enabled.
            Additive, multiplicative and alpha blended materials are
            rendered afterwards, with alpha blended materials rendered
            in a back to front order.
            */
            BlendMode blendMode = BlendMode::None;

            //arbitrary uniforms are stored as properties
            PropertyList properties;
            
            /*!
            \brief Sets a float value uniform
            \param name Name of the uniform
            \param value Value of the uniform
            */
            void setProperty(const std::string& name, float value);
            
            /*!
            \brief Sets a 2 component vector value uniform
            \param name Name of the uniform
            \param value Value of the uniform
            */
            void setProperty(const std::string& name, glm::vec2 value);
            
            /*!
            \brief Sets a 3 component vector uniform
            \param name String containing the name of the uniform
            \param value Value of the uniform
            */
            void setProperty(const std::string& name, glm::vec3 value);
            
            /*!
            \brief Sets a 4 component vector uniform
            \param name String containing the name of the uniform
            \param value Value of the uniform
            */
            void setProperty(const std::string& name, glm::vec4 value);
            
            /*!
            \brief Sets a Mat4x4 uniform.
            \param name String containing the name of the uniform
            \param value The 4x4 matrix to set
            */
            void setProperty(const std::string& name, glm::mat4 value);            
            
            /*!
            \brief Sets a colour value uniform
            \param name String containing the name of the uniform
            \param value Colour value of the uniform
            */
            void setProperty(const std::string& name, Colour value);
            
            /*!
            \brief Sets a texture sampler uniform
            \param name String containing the name of the uniform to set
            \param value A reference to the texture to bind to the sampler
            */
            void setProperty(const std::string& name, const Texture& value);
            
            /*!
            \brief Sets a texture sampler uniform from a textureID
            \param name String containing the name of the uniform to set
            \param value TextureID containing the ID to set
            */
            void setProperty(const std::string& name, TextureID value);
            
            /*!
            \brief Sets a texture sampler uniform from a textureID
            \param name String containing the name of the uniform to set
            \param value CubemapID containing the ID to set
            */
            void setProperty(const std::string& name, CubemapID value);


            /*!
            \brief Overrides the material's depth test setting. 
            This is useful for models rendered as wireframe, although
            this is usually not needed and can be ignored.
            */
            bool enableDepthTest = true;

            /*!
            \brief If this is true the material is rendered via the GBuffer
            in the deferred renderer, else is rendered via forward rendering.
            This is only valid for PBR materials, settings this to true for
            other types will have undesirable results
            */
            bool deferred = false;

            /*!
            \brief If this material was loaded from a ModelDefinition then
            the name is copied from the file, if it exists
            */
            std::string name;

            /*!
            \brief If this is true then face culling is disabled for the
            material. Default value is false
            */
            bool doubleSided = false;

            /*!
            \brief Add up to 8 custom settings
            \param setting GLenum to be used with glEnable/glDisable
            Use sparingly - each of these are enabled before rendering
            a submesh with this material, and disabled immediately 
            afterwards. Excessive use can cause unnecessary context switching
            */
            void addCustomSetting(std::uint32_t setting);

            /*!
            \brief Remove and existing custom setting
            \param setting The setting to remove
            */
            void removeCustomSetting(std::uint32_t setting);

            /*!
            \brief Used when rendering to activate this material's
            custom settings.
            \see addCustomSetting()
            */
            void enableCustomSettings() const;

            /*!
            \brief Used when rendering to disable any active custom settings
            */
            void disableCustomSettings() const;

            /*!
            \brief Animation data used if material is animated
            Note that this cannot be changed once it is assigned
            to a Model, to apply updates the material needs to be
            re-applied.
            Frames are applied top->bottom, column by column.
            */
            struct Animation final
            {
                bool active = false;
                glm::vec4 frame = glm::vec4(0.f);
                float currentTime = 0.f;
                float frameTime = 1.f;
            }animation;

            /*!
            \brief Allows setting parameters for custom blend mode.
            Set the blendMode property to BlendMode::Custom to use these
            values. Must be explicitly initialised to sane values.
            */
            struct BlendData final
            {
                std::int32_t writeDepthMask = 0; //! < GLboolean
                std::uint32_t equation = 0; //! < GLenum passed to glBlendEquation()
                std::array<std::uint32_t, 2u> blendFunc = { 0,0 }; //! < GLenum pair passed to glBlendFunc()
                std::vector<std::uint32_t> enableProperties; //! < list of GLenum passed to glEnable()
            }blendData;

            /*!
            \brief Applies a new shader to this material by updating the
            the uniform and vertex attribute maps
            */
            void setShader(const Shader&);

            /*!
            \brief Returns true if the material shader supports the camera UBO block
            Used internally by the ModelRenderer system
            */
            bool hasCameraUBO() const { return m_hasCameraUBO; }

            /*!
            \brief Returns true if the material shader supports the lighting UBO block
            Used internally by the ModelRenderer system
            */
            bool hasLightUBO() const { return m_hasLightUBO; }


            /*
            Here be dragons! Don't modify these variables as they are configured
            by the above function. These values should be considered read-only
            */

            std::uint32_t shader = 0;
            //maps attrib location to attrib size between shader and mesh - index, size, pointer offset
            std::array<std::array<std::int32_t, 3u>, Shader::AttributeID::Count> attribs{};
            std::size_t attribCount = 0; //< count of attributes successfully mapped
            //maps uniform locations by indexing via Uniform enum
            std::array<std::int32_t, Uniform::Total> uniforms{-1,-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
            //optional uniforms are added to this list if they exist
            //for example skinning and projection map data which is
            //used internally, and not user-definable
            std::size_t optionalUniformCount = 0;
            std::array<std::int32_t, 10> optionalUniforms{};

        private:
            std::unordered_map<std::string, bool> m_warnings;
            void exists(const std::string&);

            bool m_hasCameraUBO = false;
            bool m_hasLightUBO = false;

            static constexpr std::size_t MaxCustomSettings = 10;
            std::size_t m_customSettingsCount = 0;
            std::array<std::uint32_t, MaxCustomSettings> m_customSettings = {};
        };
    }
}