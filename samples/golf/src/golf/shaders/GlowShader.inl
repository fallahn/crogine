/*-----------------------------------------------------------------------

Matt Marchant 2023
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

inline const std::string GlowVertex = 
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec4 a_colour;

#include WVP_UNIFORMS

VARYING_OUT vec4 v_colour;
VARYING_OUT vec3 v_normal;
VARYING_OUT vec3 v_worldPosition;

void main()
{
    gl_Position = u_projectionMatrix * u_worldViewMatrix * a_position;
    v_worldPosition = (u_worldMatrix * a_position).xyz;
    v_normal = u_normalMatrix * a_normal;
    v_colour = a_colour;
}

)";

inline const std::string GlowFragment =
R"(
#define USE_MRT
#include OUTPUT_LOCATION

uniform vec3 u_cameraWorldPosition;

VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_normal;
VARYING_IN vec3 v_worldPosition;

#include HSV

void main()
{
    vec3 normal = normalize(v_normal);

    NORM_OUT = vec4(normal, 0.0);
    POS_OUT = vec4(v_worldPosition, 1.0);

    vec3 viewDist = u_cameraWorldPosition - v_worldPosition;
    
    vec3 eyeDirection = normalize(viewDist);

    float outer = smoothstep(0.25, 0.8, dot(normal, eyeDirection));
    float inner = 1.0 - outer;

    vec3 colour = rgb2hsv(v_colour.rgb);
    colour.g *= (0.4 * outer) + 0.6;
    colour.g *= (inner * 0.3) + 1.0;

    //colour.b *= (outer * 0.3) + 1.0;

    colour = hsv2rgb(colour);

colour += vec3((1.0 - outer) * 0.01);

    FRAG_OUT = vec4(colour, 1.0);
    LIGHT_OUT = vec4(FRAG_OUT.rgb * smoothstep(0.01, 12.0, dot(viewDist, viewDist)), 1.0);
}
)";