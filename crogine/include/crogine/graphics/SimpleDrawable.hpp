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

#pragma once

#include <crogine/Config.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Vertex2D.hpp>
#include <crogine/graphics/MaterialData.hpp>

namespace cro
{
    class Shader;
    class Texture;
    class RenderTarget;

    /*!
    \brief Simple drawable base class
    This class is used to create simple drawable items such as SimpleQuad or
    SimpleText. SimpleDrawable objects are drawn to the active target, using pixels
    as coordinates. To transform a SimpleDrawable you would normally also
    inherit Transformable2D. For more complex drawable items, such as those
    with a parentable scene graph or collision data then the ECS is to be preferred.

    SimpleDrawable objects are non-copyable and non-moveable.
    */
    class CRO_EXPORT_API SimpleDrawable : public Detail::SDLResource
    {
    public:
        SimpleDrawable();

        virtual ~SimpleDrawable();
        SimpleDrawable(const SimpleDrawable&) = delete;
        SimpleDrawable(SimpleDrawable&&) = delete;
        const SimpleDrawable& operator = (const SimpleDrawable&) = delete;
        const SimpleDrawable& operator = (SimpleDrawable&&) = delete;


        /*!
        \brief Sets the blend mode with which to draw
        Defaults to Material::BlendMode::Alpha
        */
        void setBlendMode(Material::BlendMode blendMode) { m_blendMode = blendMode; }

        /*!
        \brief Returns the current blend mode
        */
        Material::BlendMode getBlendMode() const { return m_blendMode; }

        /*!
        \brief Sets a custom shader with which to draw.
        A Vertex is defined as
        vec2 Position
        vec2 TextureCoords
        vec4 Colour

        Custom vertex shaders should expect this layout.
        \returns true if the shader was successfully set, else returns false
        \see Vertex2D
        */
        bool setShader(const Shader& shader);

        /*!
        \brief Sets the area in local coordinates to crop this drawable.
        If this area is larger or does not overlap the local bounds then
        no cropping is visible. Useful for UI components such as cropping
        Text components to fit text boxes. Note that Text components in
        particular have a *negative* bottom value as text is drawn (usually)
        from top to bottom.
        */
        void setCroppingArea(FloatRect);

        /*!
        \brief Returns the current cropping area.
        \see setCroppingArea()
        */
        FloatRect getCroppingArea() const { return m_croppingArea; }

        /*!
        \brief Draws the geometry to the active target
        This must be overridden and provides an opportunity
        to update any vertex data as well as to submit a
        deriving class's transform to the protected
        drawGeometry() function
        \param parentTransform An optional transform for creating
        parented scene-graph like hierarchies
        */
        virtual void draw(const glm::mat4&) = 0;

        /*!
        \brief Sets a uniform value for the active shader
        If the uniform doesn't exist in the current shader this
        does nothing. Note that setting a new shader also resets
        all existing uniform values, even if the new shader
        contains the same uniforms.
        Supported types are:
        float, Vec2, Vec3, Vec4, Mat4 and Texture
        \param name Name of the uniform to set
        \param value Value to set the uniform
        */
        template <typename T>
        void setUniform(const std::string& name, T value);

        /*!
        \brief Returns a string containing the default vertex shader
        Use this to prevent re-writing code when creating shaders with
        custom fragment code.
        */
        static const std::string& getDefaultVertexShader();


    protected:
        /*!
        \brief Sets the Texture to be used when rendering
        \returns true if the texture was updated, else false
        */
        bool setTexture(const Texture& texture);

        /*!
        \brief Set the texture via texture ID - it is up to the
        derived class to track its size if necessary. Note that
        when using this getTexture() will always return nullptr
        */
        void setTexture(TextureID texture);

        /*!
        \brief Returns a pointer to the active texture if there is one
        */
        const Texture* getTexture() const { return m_texture; }
        
        /*!
        \brief Sets the OpenGL primitive type with which to
        render the vertex data
        Must be GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN
        GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, or GL_POINTS
        Defaults to GL_TRIANGLE_STRIP
        */
        void setPrimitiveType(std::uint32_t primitiveType);

        /*!
        \brief Sets the vertex data with which to draw
        */
        void setVertexData(const std::vector<Vertex2D>&);

        /*!
        \brief Draws the geometry with the given transform
        */
        void drawGeometry(const glm::mat4& worldTransform) const;

    private:
        std::uint32_t m_primitiveType;
        std::uint32_t m_vbo;
#ifdef PLATFORM_DESKTOP
        std::uint32_t m_vao;
        void updateVAO(const Shader&);
#endif
        std::uint32_t m_vertexCount;

        std::uint32_t m_textureID;
        const Texture* m_texture;
        Material::BlendMode m_blendMode;

        struct ShaderUniforms final
        {
            std::int32_t projectionMatrix = -1;
            std::int32_t worldMatrix = -1;
            std::int32_t texture = -1;
            std::uint32_t shaderID = 0;
        }m_uniforms;

        FloatRect m_croppingArea;
        bool m_cropped;

        void applyBlendMode() const;



        struct UniformValue final
        {
            enum
            {
                None,
                Number,
                Vec2,
                Vec3,
                Vec4,
                Mat4,
                Texture
            }type = None;

            union
            {
                float numberValue;
                float vecValue[4];
                std::uint32_t textureID = 0;
                glm::mat4 matrixValue;
            };
        };
        std::unordered_map<std::string, std::pair<std::int32_t, UniformValue>> m_uniformValues;
    };

#include "SimpleDrawable.inl"
}
