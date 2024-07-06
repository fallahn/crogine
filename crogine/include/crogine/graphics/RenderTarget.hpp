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
#include <crogine/graphics/Rectangle.hpp>

#include <array>
#include <cstdint>

namespace cro
{
    /*!
    \brief Base class used by Windows and RenderTextures to
    pass information about themselves to Scenes when rendering
    */
    class CRO_EXPORT_API RenderTarget
    {
    public:
        /*!
        \brief Utility for passing multiple creation parameters
        to certain types of render target.
        \see RenderTexture::create()
        */
        struct Context final
        {
            std::uint32_t width = 0u; //!< desired width
            std::uint32_t height = 0u; //!< desired height
            bool depthBuffer = true; //!< should this target create a depth buffer?
            bool depthTexture = false; //!< if a depth buffer is enabled so this be rendered to a texture?
            bool stencilBuffer = false; //!< should this target create a stencil buffer?

            std::uint32_t samples = 0u; //!< if not zero then attempt to create a multi-sampled texture with this many samples

            Context() = default;
            Context(std::uint32_t w, std::uint32_t h, bool db, bool dt, bool s, std::uint32_t ms)
                : width(w), height(h), depthBuffer(db), depthTexture(dt), stencilBuffer(s), samples(ms) {}
        };

        virtual ~RenderTarget() = default;

        virtual glm::uvec2 getSize() const = 0;

        /*!
        \brief Returns the viewport in screen coordinates 
        converted from the given normalised viewport of a camera.
        Note that this doesn't alter the target's current viewport,
        rather that this is used by a render system to set the
        correct view when rendering via a Scene.
        */
        IntRect getViewport(FloatRect normalised) const;

        /*!
        \brief Returns the currently active viewport of the target 
        in device coords
        */
        IntRect getViewport() const;

        /*!
        \brief Returns the default viewport, that is the viewport
        covering the entire target, in device coordinates.
        */
        IntRect getDefaultViewport() const;

        /*!
        \brief Sets the active viewport of this target.
        Note that when rendering to the target via a Scene that
        this will be ignored in favour of the Scene's active Camera.
        */
        void setViewport(IntRect view);

        /*!
        \brief Returns the current view
        \see setView
        */
        FloatRect getView() const;

        /*!
        \brief Sets the view size
        The size of the view is that of the world, in world units,
        as seen through the current viewport.
        \param size Size, in world units, to display in the viewport
        Note that when rendering to the target via a Scene that
        this will be ignored in favour of the Scene's active Camera.
        */
        void setView(FloatRect size);

        /*!
        \brief Returns a pointer to the currently bound RenderTarget
        */
        static const RenderTarget* getActiveTarget();

        /*!
        \brief Returns the ID of the currently active target
        */
        static std::uint32_t getActiveTargetID();

        /*!
        \brief Returns an orthographic projection matrix based on
        the RenderTarget's current view
        \see setView()
        */
        const glm::mat4& getProjectionMatrix() const;

        /*!
        \brief Return the implementation's FBO ID, used by setActive()
        */
        virtual std::uint32_t getFrameBufferID() const = 0;

    protected:
        /*!
        \brief Used to track the active frame buffer.
        Classes inheriting RenderTarget should set this to true
        when clearing the target, and false once the buffer is displayed.
        */
        void setActive(bool);


    private:
        friend class Window;

        FloatRect m_view;
        IntRect m_viewport;
        std::array<std::int32_t, 4u> m_previousViewport;

        glm::mat4 m_projectionMatrix = glm::mat4(1.f);

        //used to track the active render target. This only works as
        //long as there's only one window/context active - else we
        //need to refactor this to per-context tracking
        static constexpr std::size_t MaxActiveTargets = 10;
        static std::size_t m_bufferIndex;
        static std::array<const RenderTarget*, MaxActiveTargets> m_bufferStack;
    };
}
