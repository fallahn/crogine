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
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Rectangle.hpp>

namespace cro
{
    /*!
    \brief Base class used by Windows and RenderTextures to
    pass information about themselves to Scenes when rendering
    */
    class CRO_EXPORT_API RenderTarget
    {
    public:
        virtual ~RenderTarget() = default;

        virtual glm::uvec2 getSize() const = 0;

        /*!
        \brief Returns the viewport in screen coordinates from
        the given normalised viewport of a camera.
        */
        IntRect getViewport(FloatRect normalised) const
        {
            float width = static_cast<float>(getSize().x);
            float height = static_cast<float>(getSize().y);

            return IntRect(static_cast<int>(0.5f + width * normalised.left),
                            static_cast<int>(0.5f + height * normalised.bottom),
                            static_cast<int>(0.5f + width * normalised.width),
                            static_cast<int>(0.5f + height * normalised.height));
        }

    private:

        friend class RenderTexture;
        friend class DepthTexture;

        //used by render targets such as render texture or depth texture
        //to track the  current FBO without having to query OpenGL
        static std::int32_t ActiveTarget;
    };
}
