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

static const std::string TerrainVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent; //dest position
    ATTRIBUTE vec3 a_bitangent; //dest normal

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * position;

        v_normal = u_normalMatrix * a_normal;
        v_colour = a_colour;
    })";

static const std::string TerrainFragment = R"(
    uniform vec3 u_lightDirection;

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;

    OUTPUT

    const float Steps[3] = float[3](0.25, 0.5, 1.0);

    void main()
    {
        float amount = dot(normalize(v_normal), normalize(-u_lightDirection));
        vec4 colour = v_colour;

        int index = int(step(0.5, amount) + step(0.75, amount));

        colour.rgb *= Steps[index];

        FRAG_OUT = colour;
    })";
