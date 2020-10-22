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

#include <crogine/ecs/System.hpp>

namespace cro
{
    /*!
    \brief Sprite System. This system will update the vertex data
    of Drawable2D components attached to an entity which has a Sprite
    component.

    This means that each sprite entity will have its own vertex buffer
    and be drawn individually. For cases where sprite batching is
    required a custom System can be created which will batch multiple
    sprites in a single Drawable2D component.

    \see System, Sprite, RenderSystem2D
    */

    class CRO_EXPORT_API SpriteSystem final : public cro::System
    {
    public:
        /*!
        \brief Default constructor
        \param mb A reference to the active MessageBus
        */
        explicit SpriteSystem(cro::MessageBus& mb);

        void process(float) override;

    private:
        void onEntityAdded(Entity) override;
    };
}