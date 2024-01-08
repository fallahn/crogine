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

const std::string RainVert = 
R"(
ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;
ATTRIBUTE vec4 a_colour;

uniform mat4 u_worldMatrix;
uniform mat4 u_projectionMatrix;

VARYING_OUT vec2 v_texCoord;
VARYING_OUT LOW vec4 v_colour;

void main()
{
    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord0;
    v_colour = a_colour;
})";

const std::string RainFrag = 
R"(
OUTPUT

uniform sampler2D u_texture;
uniform sampler2D u_rainMap;
uniform vec4 u_subrect;

VARYING_IN vec4 v_colour;
VARYING_IN vec2 v_texCoord;

const vec3 lDir = vec3(0.01, -0.9, 0.1);


void main()
{
    vec2 coord = v_texCoord;

    vec2 rainCoord  = u_subrect.xy + (v_texCoord * u_subrect.zw);
    vec4 rain = TEXTURE(u_rainMap, rainCoord) * 2.0 - 1.0;
    coord += rain.xy;

    vec4 colour = TEXTURE(u_texture, coord) * v_colour;

    float specAmount = clamp(dot(normalize(lDir), rain.xyz), 0.0, 1.0);
    specAmount = pow(specAmount, 244.0);
    colour.rgb += specAmount;

    FRAG_OUT = colour;

    //FRAG_OUT = mix(colour, rain, 0.95);
})";