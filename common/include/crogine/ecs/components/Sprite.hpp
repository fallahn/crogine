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

#ifndef CRO_SPRITE_HPP_
#define CRO_SPRITE_HPP_

#include <crogine/Config.hpp>
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Colour.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>

namespace cro
{
    class Texture;
    /*!
    \brief 2D Sprite component.
    Sprites are rendered with a sprite renderer system rather than
    a scene renderer. The 2D renderer can be used on its own, or rendered
    over the top of a 3D scene, for example as a user interface. Sprites
    can exist on entities which are either part of the existing 3D scene
    or an independent scene used for a specific purpose.
    */
    class CRO_EXPORT_API Sprite final
    {
    public:
        Sprite();

        /*!
        \brief Sets the texture used to render this sprite.
        Sprites are batched by the sprite renderer according to
        their texture, so using a texture atlas will result in
        fewer draw calls and improved rendering. setTextureRect() can
        be used to display a smaller area of a texture. By default
        the entire texture is used across the sprite.
        */
        void setTexture(const Texture&);

        /*!
        \brief Sets the size of the sprite.
        This is initally set to the size of the texture.
        */
        void setSize(glm::vec2);

        /*!
        \brief Sets a sub-rectangle of the texture to draw.
        This effectively sets the UV coordinates of the sprite.
        \see setTexture()
        */
        void setTextureRect(FloatRect);

        /*!
        \brief Sets the colour of the sprite.
        This colour is multiplied with that of the texture
        when it is drawn.
        */
        void setColour(Colour);

        /*!
        \brief Gets the current size of the sprite
        */
        glm::vec2 getSize() const;

        /*!
        \brief Returns the current colour of the sprite
        */
        Colour getColour() const;

    private:
        int32 m_textureID;
        glm::vec3 m_textureSize;
        struct Vertex
        {
            glm::vec4 position;
            glm::vec4 colour;
            glm::vec2 UV;
        };
        std::array<Vertex, 4u> m_quad;
        bool m_dirty;
        int32 m_vboOffset; //where this sprite starts in its VBO - used when updating sub buffer data
        friend class SpriteRenderer;
    };
}


#endif //CRO_SPRITE_HPP_