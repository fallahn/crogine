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
#include <crogine/graphics/Rectangle.hpp>
#include <crogine/graphics/Colour.hpp>
#include <crogine/graphics/MaterialData.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/vec3.hpp>
#include <crogine/detail/glm/vec4.hpp>

#include <vector>

namespace cro
{
    class Texture;
    /*!
    \brief 2D Sprite component.
    Sprites are 2D geometry usually loaded from a SpriteSheet. When
    placed on an entity with a Drawable2D component a scene requires
    a SpriteSystem2D to update them and a RenderSystem2D to draw them.
    Sprite components can also be placed on an entity with a Model component
    in which case a SpriteSystem3D is required to update the sprite
    geometry and sprites are rendered with the ModelRenderer system.

    Sprites rendered via a Drawable2D are usually measured in pixel units
    and drawn on screen with an orthographic projection.

    Sprites with a Model component are rendered as part of a 3D scene, and
    their units will need to be scaled accordingly.

    Bother versions of a Sprite can be animated with a SpriteAnimation component
    and SpriteAnimator system.

    \see SpriteSheet, SpriteAnimation
    */
    class CRO_EXPORT_API Sprite final
    {
    public:
        Sprite();

        /*!
        \brief Construct a sprite with the given Texture
        */
        explicit Sprite(const Texture&);

        /*!
        \brief Sets the texture used to render this sprite.
        By default the Sprite is resized to match the given Texture.
        Set resize to false to maintain the current size.
        */
        void setTexture(const Texture&, bool resize = true);

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
        \brief Returns a pointer to the Sprite's texture
        */
        const Texture* getTexture() const { return m_texture; }

        /*!
        \brief Returns the current Texture Rectangle
        */
        const FloatRect& getTextureRect() const { return m_textureRect; }

        /*!
        \brief Returns the current colour of the sprite
        */
        Colour getColour() const;

        /*!
        \brief Gets the current size of the sprite
        */
        glm::vec2 getSize() const { return { m_textureRect.width, m_textureRect.height }; }
        /*!

        \brief Returns the created by the active texture rectangle's size
        */
        FloatRect getTextureBounds() const { return { glm::vec2(0.f), getSize() }; }

        /*!
        \brief Maximum number of frames in an animation
        */
        static constexpr std::size_t MaxFrames = 64;

        /*!
        \brief Maximum number of animations per sprite
        */
        static constexpr std::size_t MaxAnimations = 32;

        /*!
        \brief Represents a single animation
        */
        struct Animation final
        {
            /*!
            \brief Maximum  length of animation id
            */
            static constexpr std::size_t MaxAnimationIdLength = 32;

            std::vector<char> id;
            std::vector<FloatRect> frames;
            std::uint32_t loopStart = 0; //!< looped animations can jump to somewhere other than the beginning
            bool looped = false;
            float framerate = 12.f;
        };


        /*!
        /brief Returns a reference to the sprites animation array.
        */
        std::vector<Animation>& getAnimations() { return m_animations; }

        /*!
        /brief Returns a const reference to the sprites animation array.
        */
        const std::vector<Animation>& getAnimations() const { return m_animations; }

    private:
        FloatRect m_textureRect;
        const Texture* m_texture;
        Colour m_colour;
        bool m_dirty;

        std::vector<Animation> m_animations;

        //if this was loaded from a sprite sheet the blend
        //mode was set here and needs to be forwarded to
        //the drawable component.
        bool m_overrideBlendMode;
        Material::BlendMode m_blendMode;

        friend class SpriteSystem2D;
        friend class SpriteSystem3D;
        friend class SpriteAnimator;
        friend class SpriteSheet;
    };
}