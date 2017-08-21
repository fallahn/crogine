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

#ifndef CRO_CAMERA_HPP_
#define CRO_CAMERA_HPP_

#include <crogine/Config.hpp>
#include <crogine/core/App.hpp>
#include <crogine/core/Window.hpp>
#include <crogine/graphics/Spatial.hpp>

#include <glm/vec2.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>

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
        \brief Projection matrix for this camera.
        This can be either a perspective or orthographic projection.
        By default it is constructed to a perspective matrix to match
        the curent window size.
        */
        glm::mat4 projection;

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
        IE the frustum of the ViewProjection matrix. Only guaranteed to be up
        to date if this is currently the active scene camera
        */
        std::array<Plane, 6u> getFrustum() const {return m_frustum; }

        Camera() : viewport(0.f, 0.f, 1.f, 1.f)
        {
            glm::vec2 windowSize(App::getWindow().getSize());
            projection = glm::perspective(0.6f, windowSize.x / windowSize.y, 0.1f, 150.f);
        }

    private:

        std::array<Plane, 6u> m_frustum;
        friend class Scene;
    };
}

#endif //CRO_CAMERA_HPP_