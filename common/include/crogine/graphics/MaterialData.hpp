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

#ifndef CRO_MATERIAL_DATA_HPP_
#define CRO_MATERIAL_DATA_HPP_

#include <crogine/Config.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/MeshData.hpp>
#include <crogine/graphics/Colour.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <unordered_map>

namespace cro
{
    class Texture;
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
            Projection,
            WorldViewProjection,
            Normal,
            Camera,
            Skinning,
            ProjectionMap,
            ProjectionMapCount,
            Total
        };
        
        enum class BlendMode
        {
            None,
            Alpha,
            Multiply,
            Additive
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
                Skinning,
                ProjectionMap
            }type = None;

            union
            {
                float numberValue;
                float vecValue[4];
                int32 textureID;
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
        using PropertyList = std::unordered_map<std::string, std::pair<int32, Property>>;

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
            uint32 shader = 0;
            //maps attrib location to attrib size between shader and mesh - index, size, pointer offset
            std::array<std::array<int32, 3u>, Mesh::Attribute::Total> attribs{}; 
            std::size_t attribCount = 0; //< count of attributes successfully mapped
            //maps uniform locations by indexing via Uniform enum
            std::array<int32, Uniform::Total> uniforms{};

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
            \param value Value of the unform
            */
            void setProperty(const std::string& name, glm::vec3 value);
            /*!
            \brief Sets a 4 component vector uniform
            \param name String containing the name of the uniform
            \param value Value of the unform
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
        };
    }
}

#endif //CRO_MATERIAL_DATA_HPP_