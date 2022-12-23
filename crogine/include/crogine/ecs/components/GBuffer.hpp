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
#include <crogine/graphics/MultiRenderTexture.hpp>

namespace cro
{
    /*!
    \brief Wraps a MultiRenderTexture into a component.
    GBuffer components are used for Cameras which output
    via deferred renderering. Therefore it only makes sense
    to add this component to an entity with a Camera component
    which belongs to a Scene using the DeferredRenderSystem.

    The buffer size/setup is managed by the Scene's CameraSystem.
    */
    struct CRO_EXPORT_API GBuffer final
    {
        MultiRenderTexture buffer;
        static constexpr std::size_t TargetCount = 6;
    };
}