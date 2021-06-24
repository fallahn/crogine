/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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
#include <crogine/graphics/MaterialData.hpp>
#include <crogine/detail/SDLResource.hpp>
#include <crogine/detail/glm/vec2.hpp>

namespace cro
{
    class Texture;

    /*
    \brief Simple Quad drawable.
    The Simple Quad draws a textured quad to the active target, in target coordinates.
    Useful for quick previewing of textures. SimpleQuads are non-copyable.
    */
    class CRO_EXPORT_API SimpleQuad final : public Detail::SDLResource
    {
    public:
        /*!
        \brief Default constructor
        */
        SimpleQuad();
        
        /*!
        \brief Constructor
        \param texture A valid texture with which to draw the quad.
        The quad is automatically set to the size of the given texture, in pixels.
        */
        explicit SimpleQuad(const cro::Texture& texture);

        ~SimpleQuad();
        SimpleQuad(const SimpleQuad&) = delete;
        SimpleQuad(SimpleQuad&&) = delete;
        const SimpleQuad& operator = (const SimpleQuad&) = delete;
        const SimpleQuad& operator = (SimpleQuad&&) = delete;

        /*!
        \brief Sets a new texture for the quad.
        \param texture The new texture to use. The quad is automatically resized
        to the size of the texture, in pixels.
        */
        void setTexture(const cro::Texture& texture);

        /*!
        \brief Sets the position of the quad, in screen coordinates.
        */
        void setPosition(glm::vec2 position);

        /*!
        \brief Returns the current position of the quad
        */
        glm::vec2 getPosition() const { return m_position; }

        /*!
        \brief Set the blend mdoe with which to draw the quad
        Defaults to Material::BlendMode::Alpha
        */
        void setBlendMode(Material::BlendMode blendMode) { m_blendMode = blendMode; }

        /*!
        \brief Returns the current blend mode for this quad
        */
        Material::BlendMode getBlendMode() const { return m_blendMode; }

        /*!
        \brief Draws the quad at its current position to the screen
        */
        void draw() const;

    private:

        glm::vec2 m_position;
        std::uint32_t m_vbo;

#ifdef PLATFORM_DESKTOP
        std::uint32_t m_vao;
#endif
        std::int32_t m_textureID;
        Material::BlendMode m_blendMode;

        void updateVertexData(glm::vec2 size);
        void applyBlendMode() const;
    };
}