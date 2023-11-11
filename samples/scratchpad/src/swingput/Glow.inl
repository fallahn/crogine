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

static const std::string GlowVertex = 
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

static const std::string GlowFragment =
R"(
OUTPUT

uniform vec3 u_cameraWorldPosition;

VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_normal;
VARYING_IN vec3 v_worldPosition;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

    float rim = dot(normal, eyeDirection);
    
    vec3 colour = v_colour.rgb;

    //vec3 dark = colour * 0.1;
    //float darkAmount = 0.0;//smoothstep(0.7, 0.9, 1.0 - rim);


    vec3 light = colour * 3.1;
    float lightAmount = pow(rim, 3.0);//smoothstep(0.4,0.7,rim);

    FRAG_OUT = vec4((light * lightAmount) + colour, 1.0);// mix(colour, dark, darkAmount), 1.0);
}
)";