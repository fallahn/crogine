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
#include <crogine/core/App.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/graphics/Spatial.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

#include <array>
#include <functional>

namespace cro
{
    /*!
    \brief Represents a camera within the scene.
    Use Scene::setActiveCamera() to use an entity with
    a camera component as the current view
    */
    struct CRO_EXPORT_API Camera final
    {
        /*!
        \brief View Matrix.
        This matrix is, by default, an identity matrix. This
        can be updated manually, although it is recommended that
        a Scene has an active CameraSystem added to it to automatically
        calculate both the View and ViewProjection matrices for
        all entities with a Camera component.
        */
        glm::mat4 viewMatrix = glm::mat4(1.f);

        /*!
        \brief ViewProjection matrix.
        \see viewMatrix
        */
        glm::mat4 viewProjectionMatrix = glm::mat4(1.f);
        
        /*!
        \brief Projection matrix for this camera.
        This can be either a perspective or orthographic projection.
        By default it is constructed to a perspective matrix to match
        the curent window size.
        */
        glm::mat4 projectionMatrix = glm::mat4(1.f);

        /*!
        \brief Viewport.
        This is in normalised (0 - 1) coordinates to describe which part
        of the window this camera should render to. By default it is 0, 0, 1, 1
        which covers the entire screen. Changing this is useful when letterboxing
        the output to maintain a fixed aspect ratio, or for rendering sub-areas of
        the screen such as a minimap or split-screen multiplayer.
        */
        FloatRect viewport;

        /*!
        \brief Returns the camera frustum including any parent transformation,
        IE the frustum of the ViewProjection matrix. This requires an active
        CameraSystem in the Scene to be up to date.
        */
        std::array<Plane, 6u> getFrustum() const {return m_frustum; }

        Camera() : viewport(0.f, 0.f, 1.f, 1.f)
        {
            glm::vec2 windowSize(App::getWindow().getSize());
            projectionMatrix = glm::perspective(0.6f, windowSize.x / windowSize.y, 0.1f, 150.f);
        }

        /*!
        \brief Resize callback.
        Optional callback automatically called by the camera's Scene if the camera
        is currently the active one. Useful for resizing viewports or letterboxing
        2D views when the current window has changed aspect ratio. The callback
        takes the current camera as a parameter and returns void
        */
        std::function<void(Camera&)> resizeCallback;

    private:

        std::array<Plane, 6u> m_frustum = {};
        friend class CameraSystem;
    };
}