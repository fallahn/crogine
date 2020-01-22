/*-----------------------------------------------------------------------

Matt Marchant 2020
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
#include <crogine/ecs/System.hpp>


namespace cro
{
    struct Camera;

    /*!
    \brief Updates camera properties.
    The CameraSystem ensures all Camera components add to a
    Scene correctly have their current view and viewProjection
    matrices updated. It also makes sure the Frustum property
    is current. All Scenes should have an active camera system
    to ensure correct rendering. NOTE it is worth ensuring that
    the CameraSystem is added to a Scene AFTER a SceneGraph
    but before any rendering systems, so that the values are
    updated in the correct order.
    */

    class CRO_EXPORT_API CameraSystem final : public cro::System
    {
    public:
        explicit CameraSystem(cro::MessageBus&);

        void process(Time) override;


    private:

        void updateFrustum(Camera&);
    };
}