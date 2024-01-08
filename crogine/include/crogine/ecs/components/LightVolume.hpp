/*-----------------------------------------------------------------------

Matt Marchant 2023
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
#include <crogine/graphics/Colour.hpp>

namespace cro
{
    /*!
    \brief Light Volume component

    Used in conjunction with the LightVolumeSystem and
    the ModelRenderer light volumes can be used to create
    dynamic light maps with deferred rendering paths.

    When rendering a light volume the Entity requires
    a mesh component with a spherical mesh attached to it
    that represents a 3D volume with the same (or greater)
    radius as the radius value of the LightVolume.

    Model components used for rendering volumes should be
    set to hidden, although they can be unhidden for
    debugging purposes as this with render the mesh in the
    scene.

    \see LightVolumeSystem
    */
    struct CRO_EXPORT_API LightVolume final
    {
        float maxVisibilityDistance = 90000.f; //!< lights are culled when beyond this distance from the camera (Sqr)
        float radius = 1.f; //!< maxiumum radius of the fall-off
        Colour colour = Colour::White; //!< colour of the light

        /*!
        \brief Used on construction of the LightVolumeSystem
        \see LightVolumeSystem
        */
        enum
        {
            WorldSpace, ViewSpace
        };
    private:
        float lightScale = 1.f; //used to scale the volume to the mesh based on current transform
        float cullAttenuation = 1.f; //used to fade the light before it's culled
        friend class LightVolumeSystem;
    };
}