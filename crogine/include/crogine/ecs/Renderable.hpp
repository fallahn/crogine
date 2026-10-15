/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2026
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
#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <array>
#include <functional>

namespace cro
{
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
        (or some other method) to determine an entity's visibility. Once
        visible entities are found (and potentially depth sorted) they
        should be stored in a list indexed by the Camera's draw list index

        This is called during the Scene update once for each Camera in the
        CameraSystem

        \see Camera::getDrawListIndex()
        \see CameraSystem::process()
        */
        virtual void updateDrawList(Entity camera) = 0;

        /*!
        \brief Pre render pass
        Use this to render the system to a different render target, such as
        an internal render texture or depth map. This function is always called
        when the default frame buffer (the main window) is unbound, to prevent
        unnecessarily binding it. To render to the main window use render()
        */
        virtual void preRender() {}


        /*!
        \brief Renders this system to the default render target (the window).
        \param camera Entity containing a camera and transform component, automatically
        passed in by the scene when this system is drawn. Use this to accurately draw the
        system, usually via the camera's draw list as determined by updateDrawList().
        \param target RenderTarget currently being drawn to. Provides information such as
        the dimensions of the target.
        Note for performance reasons this should always be the main window, and is
        left here for compatibility. For rendering to other targets used preRender()
        which ensures the main window is not the active frame buffer and prevents
        unnecessary switching.
        */
        virtual void render(Entity camera, const RenderTarget& target) {};

        /*!
        \brief Sets the function to render the Scene's skybox.
        Only applicable to the ModelRenderer which needs to render
        the skybox between opaqueand transparent passes.
        */
        void setSkyboxCallback(const std::function<void(const Camera&)>&);

        
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

        /*!
        \brief Implement this and call it during onEntityRemoved() to flush the removed
        entity from any draw lists
        */
        virtual void flushEntity(Entity) = 0;

        /*!
        \brief Calls the Skybox callback set by the Scene using this system
        */
        void drawSkybox(const Camera&);

    private:
        std::array<std::int32_t, 4> m_previousViewport{};

        std::function<void(const Camera&)> m_skyboxCallback;
    };
}