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
#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <array>

namespace cro
{
    class NullTarget;

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

        /*!
        \brief Returns a pointer to the currently bound RenderTarget
        or nullptr if not target has been activated.
        */
        static const RenderTarget* getActiveTarget();

    protected:
        /*!
        \brief Used to track the active frame buffer.
        Classes inheriting RenderTarget should set this to true
        when clearing the target, and false once the buffer is displayed.
        */
        void setActive(bool);

        /*!
        \brief Return the implementation's FBO ID, used by setActive()
        */
        virtual std::uint32_t getFrameBufferID() const = 0;

    private:
       friend class NullTarget;

        //used to track the active render target. This only works as
        //long as there's only one window/context active - else we
        //need to refactor this to per-context tracking
        static constexpr std::size_t MaxActiveTargets = 10;
        static std::size_t m_bufferIndex;
        static std::array<const RenderTarget*, MaxActiveTargets> m_bufferStack;
    };
}
