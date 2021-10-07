/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

#include <string>

static const std::string WireframeVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_projectionMatrix;
    uniform vec4 u_clipPlane;

#if defined (CULLED)
    uniform HIGH vec3 u_cameraWorldPosition;
#endif

    VARYING_OUT LOW vec4 v_colour;

    void main()
    {
        mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
        vec4 position = a_position;
        gl_Position = wvp * position;

        vec4 worldPos = u_worldMatrix * position;
        v_colour = a_colour;

#if defined (CULLED)
        vec3 distance = worldPos.xyz - u_cameraWorldPosition;
        v_colour.a *= smoothstep(12.25, 16.0, dot(distance, distance));
#endif

        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    }
)";

static const std::string WireframeFragment = R"(
    OUTPUT
    uniform vec4 u_colour = vec4(1.0);

    VARYING_IN vec4 v_colour;

    void main()
    {
        FRAG_OUT = v_colour * u_colour;
    }
)";