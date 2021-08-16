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

static const std::string TerrainVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent; //dest position
    ATTRIBUTE vec3 a_bitangent; //dest normal

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec4 u_clipPlane;
    uniform float u_morphTime;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;

    vec3 lerp(vec3 a, vec3 b, float t)
    {
        return a + ((b - a) * t);
    }

    void main()
    {
        vec4 position = u_worldMatrix * vec4(lerp(a_position.xyz, a_tangent, u_morphTime), 1.0);
        gl_Position = u_viewProjectionMatrix * position;

        //this should be a slerp really but lerp is good enough for low spec shenanigans
        v_normal = u_normalMatrix * lerp(a_normal, a_bitangent, u_morphTime);
        v_colour = a_colour;

        gl_ClipDistance[0] = dot(position, u_clipPlane);
    })";

static const std::string SlopeVertexShader =
R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform vec3 u_centrePosition;

    VARYING_OUT vec4 v_colour;

    const float Radius = 20.0;

    void main()
    {
        vec4 worldPos = u_worldMatrix * a_position;
        float alpha = 1.0 - smoothstep(Radius, Radius + 1.5, length(worldPos.xyz - u_centrePosition));

        gl_Position = u_viewProjectionMatrix * worldPos;

        v_colour = a_colour;
        v_colour.a *= alpha;
    }   
)";

static const std::string SlopeFragmentShader =
R"(
    OUTPUT

    VARYING_IN vec4 v_colour;

    void main()
    {
        FRAG_OUT = v_colour;
    }
)";

static const std::string CelVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec4 u_clipPlane;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * position;

        v_normal = u_normalMatrix * a_normal;
        v_colour = a_colour;

        gl_ClipDistance[0] = dot(position, u_clipPlane);
    })";

static const std::string CelFragmentShader = R"(
    uniform vec3 u_lightDirection;

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;

    OUTPUT

    const float Steps[3] = float[3](0.25, 0.5, 1.0);

    void main()
    {
        float amount = dot(normalize(v_normal), normalize(-u_lightDirection));
        vec4 colour = v_colour;

        //int index = int(step(0.5, amount) + step(0.75, amount));

        amount = (0.2 * step(0.2, amount)) + 0.8;
        colour.rgb *= amount;//Steps[index];

        FRAG_OUT = vec4(colour.rgb, 1.0);
    })";
