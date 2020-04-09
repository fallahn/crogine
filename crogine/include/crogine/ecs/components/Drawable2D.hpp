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
#include <crogine/graphics/Vertex2D.hpp>
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Shader.hpp>

#include <vector>

namespace cro
{
    class Texture;

    /*!
    \brief Component used for creating 2D drawable entities.
    The drawable 2D component encapsulates the vertex/shader/texture
    data used for rendering 2D entities such as those with a Text or
    Sprite component. Vertex data can also be accessed directly to allow
    custom drawables to be made such as sprite batches or more complex
    animated drawables which require vertex data manipulation.

    Drawable2D components are rendered with a RenderSystem2D which needs
    to be added to a scene. Entities with a Drawable2D component also
    require a Transform component. The draw order is based on the Z or
    Y axis position, based on the current setting of the RenderSystem2D
    used to draw the geometry. For example with the DepthAxis set to Z
    drawables are rendered with smallest Z values furthest away.
    Setting the DepthAxis to Y will render geometry with greater Y
    values first. This use useful for rendering tile maps with a top
    down perspective for example.
    */
    class CRO_EXPORT_API Drawable2D final
    {
    public:
        Drawable2D();

        /*!
        \brief Set the texture to be used with this drawable.
        Setting the texture to nullptr removes the texture.
        The texture is automatically bounds to texture unit 0
        which should be kept in mind if using a custom shader.
        This has no effect when using Sprite or Text components
        as they override this property with their own.
        */
        void setTexture(const cro::Texture*);

        /*!
        \brief Set a custom shader for this drawable.
        Drawables are assigned, by default, the relevant shader
        for drawing ordinary textured or vertex coloured geometry.
        Setting a custom shader will override this behaviour,
        although custom vertex shaders much match the attribute layout
        of Vertex2D. Setting this to nullptr returns the drawable
        to the default shader. Note that this also resets any
        bound uniform values so unforms must be set after setting a
        new shader for the drawable
        */
        void setShader(Shader*);

        /*!
        \brief Sets the blend mode to be used with the drawable
        */
        void setBlendMode(Material::BlendMode);

        /*!
        \brief Sets the vertex data to be used when the geometry is drawn.
        The vertex data is copied into the drawable. Setting new
        vertex data like this automatically calls updateLocalBounds(),
        so it is not required to do it manually.
        */
        void setVertexData(const std::vector<Vertex2D>&);

        /*!
        \brief Set the primitive type of the vertex data.
        This defaults to GL_TRIANGLE_STRIP but can be set to any
        supported OpenGL primitive type.
        */
        void setPrimitiveType(std::uint32_t);

        /*!
        \brief Returns a pointer to the active texture
        May be nullptr is no texture is set.
        */
        const Texture* getTexture() const;

        /*!
        \brief Returns a pointer to the active shader.
        May return nullptr
        */
        Shader* getShader();
        const Shader* getShader() const;

        /*!
        \brief Returns the current blend mode
        */
        Material::BlendMode getBlendMode() const;

        /*!
        \brief Returns a reference to the vector of vertex data
        currently being used to render this geometry. Modifying
        the vertex data via this function requires manually calling
        updateLocalBounds() afterwards.
        \see updateLocalBounds()
        */
        std::vector<Vertex2D>& getVertexData();
        const std::vector<Vertex2D>& getVertexData() const;

        /*!
        \brief Returns the current primitive type for this drawable.
        */
        std::uint32_t getPrimitiveType() const;

        /*!
        \brief Returns the FloatRect used for the AABB of this drawable.
        This needs to be updated with a call to updateLocalBounds()
        each time the vertex data is updated, as it is used by RenderSystem2D
        to cull drawables which do no appear within the active viewport.
        */
        FloatRect getLocalBounds() const;

        /*!
        \brief Updates the local bounds used to create the AABB of the component.
        This should be called each time the vertex data is updated to ensure the
        AABB is correct. Generally this would be done in the process() function
        of any custom system which modifies vertex data. This is automatically
        performed for Text and Sprite components by their respective Systems,
        as well as when vertex data is updated via setVertexData().
        */
        void updateLocalBounds();

        /*!
        \brief Adds a uniform binding to be applied to any shader this
        drawable may have.
        When sharing a shader between multiple drawables it may be, for
        instance, desirable to the apply a different texture for each drawble.
        This function allows mapping a uniform name (assuming it is available
        in the current shader) to a texture or other value.
        Note that these are reset if the drawable's shader is changed and
        must be re-applied if necessary
        */
        void bindUniform(const std::string& name, const Texture& texture);

        void bindUniform(const std::string& name, float value);

        void bindUniform(const std::string& name, glm::vec2 value);

        void bindUniform(const std::string& name, glm::vec3 value);

        void bindUniform(const std::string& name, glm::vec4 value);

        void bindUniform(const std::string& name, bool value);

        void bindUniform(const std::string& name, Colour colour);

        /*!
        \brief Binds a pointer to a float array containing a 4x4 matrix
        */
        void bindUniform(const std::string& name, const float* value);

        //TODO filter flags, cropping area

    private:

        const Texture* m_texture;
        Shader* m_shader;
        bool m_customShader;
        bool m_applyDefaultShader;
        std::int32_t m_textureUniform;
        std::int32_t m_worldViewUniform;
        std::int32_t m_projectionUniform;

        Material::BlendMode m_blendMode;
        std::uint32_t m_primitiveType;
        std::vector<Vertex2D> m_vertices;

        std::uint32_t m_vbo;
        std::uint32_t m_vao; //!< only used in desktop builds
        bool m_updateBufferData;

        struct AttribData final
        {
            std::int32_t id = -1;
            std::uint32_t size = 0;
            std::uint32_t offset = 0;
        };
        std::vector<AttribData> m_vertexAttributes;

        FloatRect m_localBounds;

        //if this changes entities need to be sorted
        float m_lastSortValue;

        std::vector<std::pair<std::int32_t, const Texture*>> m_textureBindings;
        std::vector<std::pair<std::int32_t, float>> m_floatBindings;
        std::vector<std::pair<std::int32_t, glm::vec2>> m_vec2Bindings;
        std::vector<std::pair<std::int32_t, glm::vec3>> m_vec3Bindings;
        std::vector<std::pair<std::int32_t, glm::vec4>> m_vec4Bindings;
        std::vector<std::pair<std::int32_t, std::int32_t>> m_boolBindings;
        std::vector<std::pair<std::int32_t, const float*>> m_matBindings;

        template <typename T>
        void bindUniform(const std::string& name, T value, std::vector<std::pair<std::int32_t, T>>& dest)
        {
            CRO_ASSERT(m_shader != nullptr, "Shader is nullptr");
            if (m_shader->getUniformMap().count(name) == 0)
            {
                Logger::log(name + " not found in shader", Logger::Type::Warning);
            }
            else
            {
                auto uniform = m_shader->getUniformMap().at(name);

                auto result = std::find_if(dest.begin(), dest.end(),
                    [uniform](const std::pair<std::int32_t, T>& pair)
                    {
                        return pair.first == uniform;
                    });

                if (result == dest.end())
                {

                    dest.emplace_back(std::make_pair(uniform, value));

                }
                else
                {
                    result->second = value;
                }
            }
        }
        //seems a shame to do basically the same thing for const pointers. If anyone knows a clever
        //way around this answers on a postcard please :)
        template <typename T>
        void bindUniform(const std::string& name, const T* value, std::vector<std::pair<std::int32_t, T>>& dest)
        {
            CRO_ASSERT(m_shader != nullptr, "Shader is nullptr");
            if (m_shader->getUniformMap().count(name) == 0)
            {
                Logger::log(name + " not found in shader", Logger::Type::Warning);
            }
            else
            {
                auto uniform = m_shader->getUniformMap().at(name);

                auto result = std::find_if(dest.begin(), dest.end(),
                    [uniform](const std::pair<std::int32_t, T>& pair)
                    {
                        return pair.first == uniform;
                    });

                if (result == dest.end())
                {

                    dest.emplace_back(std::make_pair(uniform, value));

                }
                else
                {
                    result->second = value;
                }
            }
        }

        friend class RenderSystem2D;

        void applyShader();
    };
}
