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

#pragma once

#include "Types.hpp"

#include <crogine/detail/glm/vec2.hpp>

namespace cro
{

    //Used by orthogonal renderers
    static const glm::uvec2 DefaultSceneSize(1920, 1080);

    //this is the maximum number of projection maps
    //which can be received by a material using a built-in
    //shader compiled with the ShaderResource::ReceiveShadowMaps flag
    //this is only a preferred amount and is dependent on MAX_VARYING_VECTORS
    //available on the current hardware
    static const int32 MAX_PROJECTION_MAPS = 4;
}