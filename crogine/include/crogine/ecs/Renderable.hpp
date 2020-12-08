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
#include <crogine/detail/SDLResource.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <array>

namespace cro
{
    /*!
    \brief Render flags.
    Use these to filter renderable items which should be drawn for a
    specific pass. For example reflective plane geometry should only
    be rendered into the final buffer, not the reflection and refraction
    buffers.

    Set the render flags on the Model component to 'ReflectionPlane' to
    flag it as such. Then set the RenderFlags of the ModelRenderer to
    the inverse: 
    \begincode
    scene.getSystem<ModelRenderer>().setFlags(~RenderFlags::ReflectionPlane);
    \endcode
    This will set all flags active on the renderer so that everything but
    reflection plane geometry will be drawn.

    When renderingto the final buffer return the flags to their default
    value to render all geometry again:
    \begincode
    scene.getSystem<ModelRenderer>().setFlags(RenderFlags::All);
    \endcode

    Custom flags can be created for any renderable component starting
    at (1<<1) and incrementing (1<<x) up to the minimum value listed
    here.
    */

    struct RenderFlags final
    {
        static constexpr std::uint64_t ReflectionPlane = ~(std::numeric_limits<std::uint64_t>::max() / 2);


        static constexpr std::uint64_t All = std::numeric_limits<std::uint64_t>::max();
    };

    class Entity;
    struct Camera;
    class RenderTarget;
    /*!
    \brief Renderable interface for systems which draw parts of the scene.
    Systems which implement this will be drawn by any scene to which they are added,
    as well as having any camera components in the Scene passed to them via
    updateDrawLists() for entity culling and sorting.
    */
    class CRO_EXPORT_API Renderable : public Detail::SDLResource
    {
    public:
        Renderable() = default;
        virtual ~Renderable() = default;

        /*!
        \brief Updates the given camera's draw lists.
        For each camera component in the Scene this function is used to
        update the list of entities visible to the camera and sort their
        draw order. This function should use the given camera's frustum
        (or some other method) do determine an entity's visibility. Once
        visible entities are found (and potentially depth sorted) they
        should be appended to the camera's draw list. This list should
        then be used when rendering this system.
        \see Camera::drawList
        */
        virtual void updateDrawList(Entity camera) = 0;

        /*!
        \brief Renders this system.
        \param camera Entity containing a camera and transform component, automatically
        passed in by the scene when this system is drawn. Use this to accurately draw the
        system, usually via the camera's draw list as determined by updateDrawList().
        \param target RenderTarget currently being drawn to. Provides information such as
        the dimensions of the target
        */
        virtual void render(Entity camera, const RenderTarget& target) = 0;

        /*!
        \brief Sets the render flags for the next render operation.
        Use this to test whether the current renderable component should be rendered
        by ANDing the renderable flags with the Renderable system flags.
        \param flags A 64 bit value containing the bits making up the current render flags
        */
        void setRenderFlags(std::uint64_t flags) { m_renderFlags = flags; }

        /*!
        \brief Returns the current render flags.
        This is by default std::numeric_limits<std::uint64_t>::max() (all flags set)
        */
        std::uint64_t getRenderFlags() const { return m_renderFlags; }

    protected:
        /*!
        \brief Applies the given normalised viewport.
        Use this to set the viewport for the given camera component when rendering.
        Usually one would restore the existing viewport when done rendering, for consistency.
        \returns Newly set viewport in window coords
        */
        IntRect applyViewport(FloatRect, const RenderTarget&);

        /*!
        \brief Restores the previously active viewport after a call to applyViewport()
        */
        void restorePreviousViewport();

    private:
        std::array<int32, 4> m_previousViewport{};

        std::uint64_t m_renderFlags = std::numeric_limits<std::uint64_t>::max();
    };
}