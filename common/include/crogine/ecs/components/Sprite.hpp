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
#include <crogine/graphics/MaterialData.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/vec4.hpp>

#include <array>

namespace cro
{
    class Texture;
    /*!
    \brief 2D Sprite component.
    Sprites are rendered with a sprite renderer system rather than
    a model renderer. The 2D renderer can be used on its own, or rendered
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
        \brief Returns the current Texture Rectangle
        */
        const FloatRect& getTextureRect() const { return m_textureRect; }

        /*!
        \brief Returns the current colour of the sprite
        */
        Colour getColour() const;

        /*!
        \brief Returns the local (pre-transform) AABB
        */
        const FloatRect& getLocalBounds() const { return m_localBounds; }

        /*!
        \brief Returns the global (post-transform) AABB
        */
        const FloatRect& getGlobalBounds() const { return m_globalBounds; }

        /*!
        \brief Sets the blend mode of the sprite.
        By default sprites are Alpha blended, but can use the Material::BlendModes
        additive, multiply and none.
        */
        void setBlendMode(Material::BlendMode mode);

        /*!
        \brief Maximum number of frames in an animation
        */
        static constexpr std::size_t MaxFrames = 100;

        /*!
        \brief Maximum number of animations per sprite
        */
        static constexpr std::size_t MaxAnimations = 10;

        /*!
        \brief Represents a single animation
        */
        struct Animation final
        {
            std::array<FloatRect, MaxFrames> frames;
            std::size_t frameCount = 0;

            bool looped = false;
            float framerate = 12.f;
        };


    private:
        int32 m_textureID;
        glm::vec3 m_textureSize;
        FloatRect m_textureRect;
        struct Vertex final
        {
            glm::vec4 position;
            glm::vec4 colour;
            glm::vec2 UV;
        };
        std::array<Vertex, 4u> m_quad;
        bool m_dirty;
        int32 m_vboOffset; //where this sprite starts in its VBO - used when updating sub buffer data

        FloatRect m_localBounds;
        FloatRect m_globalBounds;

        bool m_visible; //used in culling
        Material::BlendMode m_blendMode;
        bool m_needsSorting;

        std::size_t m_animationCount;
        std::array<Animation, MaxAnimations> m_animations;

        friend class SpriteRenderer;
        friend class SpriteAnimator;
        friend class SpriteSheet;
    };
}


#endif //CRO_SPRITE_HPP_