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
#include <crogine/graphics/Transformable2D.hpp>
#include <crogine/graphics/SimpleDrawable.hpp>

namespace cro
{
    class Texture;
    class Sprite;

    /*
    \brief Simple Quad drawable.
    The Simple Quad draws a textured quad to the active target, in target coordinates.
    Useful for quick previewing of textures. SimpleQuads are non-copyable.
    */
    class CRO_EXPORT_API SimpleQuad final : public Transformable2D, public SimpleDrawable
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
        explicit SimpleQuad(const Texture& texture);

        /*!
        \brief Assignment operator
        Allows setting the quad properties from a Sprite, eg when
        loaded from a SpriteSheet
        */
        SimpleQuad& operator = (const Sprite&);

        /*!
        \brief Sets a new texture for the quad.
        \param texture The new texture to use. The quad is automatically resized
        to the size of the texture, in pixels. Note that this resets any texture
        rect that was previously set
        */
        void setTexture(const Texture& texture);

        /*!
        \brief Set the texture of the quad with the given texture handle
        \param texture TextureID to apply to the quad
        \param size Size to set the quad to when rendering
        */
        void setTexture(TextureID texture, glm::uvec2 size);

        /*!
        \brief Sets the quad to render a sub-rectangle of the current texture
        \param subrect A FloatRect containing a sub-rectangular area of the
        texture, in pixel coords, to set the quad to. Note that this will be reset
        should a new texture be supplied.
        \see setTexture()
        */
        void setTextureRect(cro::FloatRect subrect);

        /*!
        \brief Sets the colour which is multiplied with the texture colour of the quad
        */
        void setColour(const cro::Colour& colour);

        /*!
        \brief Returns the current colour of the quad
        */
        const cro::Colour& getColour() const { return m_colour; }

        /*!
        \brief Return the current size of the quad.
        This will be the size of the assigned texture in pixels
        or zero if no texture is assigned.
        */
        glm::vec2 getSize() const { return m_size; }

        /*!
        \brief Draws the quad to the active target
        \param parentTransform An option transform with
        which the Quad's transform is multiplied, to create
        scene-graph like hierarchies
        */
        void draw(const glm::mat4& parentTransform = glm::mat4(1.f)) override;

        /*!
        \brief Returns the current UV coordinates in normalised values
        */
        const cro::FloatRect& getUVRect() const { return m_uvRect; }

    private:
        cro::Colour m_colour;
        glm::vec2 m_size;
        cro::FloatRect m_uvRect;

        void updateVertexData();
    };
}