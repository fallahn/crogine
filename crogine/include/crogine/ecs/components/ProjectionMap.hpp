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

#include <crogine/detail/glm/mat4x4.hpp>
#include <crogine/detail/glm/gtc/matrix_transform.hpp>

namespace cro
{
    /*!
    \brief Projection Map Component.
    Projection map components contain information used to project
    textures in a 3D scene. The most important member is the 
    projection matrix, which is used in conjunction with the transform
    of an entity to decide where to project a texture. To draw projected
    textures a Scene must contain a ModelRenderer system and a 
    ProjectionMap system. At least one material must exist in the scene
    which uses a shader created with the ShaderResource::BuiltIn::ReceiveProjection flag.
    Materials which utilise skinning for skeletal animation cannot currently
    receive projected textures, as a limit on the number of available
    shader vectors prevents this. In the future this limit may only apply
    to mobile devices and is subject to change on desktop computers.
    Projection maps are primarily designed to 'fake' projected shadows on
    low-end hardware in lieu of shadow mapping, but setting the blend mode
    to Additive can create effects such as spot/torchlight. Currently only
    a single texture can be used with each projection, set by the Material
    property u_projectionMap. A scene may have up to 8 active projection map
    components, which are calculated by the ProjectionMap system, and retrievable 
    via Scene::getActiveProjectionMaps() which returns a pair containing a 
    pointer to the array of projection matrices and a number containing the
    size. This can be optionally used in any custom shaders for materials
    which need to receive projection mapping.
    */
    struct CRO_EXPORT_API ProjectionMap final
    {
        glm::mat4 projection = glm::mat4(1.f);
        ProjectionMap()
        {
            projection = glm::perspective(0.9f, 1.f, 0.8f, 4.f);
        }
    };
}