/*-----------------------------------------------------------------------

Matt Marchant 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

static const std::string FogVert = 
R"(
uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;

ATTRIBUTE vec2 a_position;
ATTRIBUTE MED vec2 a_texCoord0;
ATTRIBUTE LOW vec4 a_colour;

VARYING_OUT LOW vec4 v_colour;
VARYING_OUT MED vec2 v_texCoord;

void main()
{
    gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_colour = a_colour;
    v_texCoord = a_texCoord0;
})";


static const std::string FogFrag = 
R"(
const float ZNear = 0.1;
#if !defined(ZFAR)
#define ZFAR 320.0
#endif
const float ZFar = ZFAR;

uniform sampler2D u_texture;
uniform sampler2D u_depthTexture;

uniform float u_density = 0.0;
uniform float u_fogStart = 0.0;
uniform float u_fogEnd = ZFAR;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

const vec4 FogColour = vec4(0.91,0.92,0.923,1.0);

float fogAmount(float distance)
{
    //linear
    return clamp(smoothstep(u_fogStart, u_fogEnd, distance * ZFar) * u_density, 0.0, 1.0);

    //exp
    //distance = smoothstep(u_fogStart, u_fogEnd, distance * ZFar) * ZFar;
    //float density = 0.1 / u_density;
    //return 1.0 - clamp(exp2(-density * density * distance * distance), 0.1, 1.0);
}

void main()
{
    vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;
    float depthSample = TEXTURE(u_depthTexture, v_texCoord).r;

    //although this is "correct" it actually looks wrong.
    //float d = (2.0 * ZNear * ZFar) / (ZFar + ZNear - depthSample * (ZFar - ZNear));

    float d = (2.0 * ZNear) / (ZFar + ZNear - depthSample * (ZFar - ZNear));
    //FRAG_OUT = mix(colour, vec4(d,d,d,1.0), u_density / 10.0);
    FRAG_OUT = mix(colour, FogColour, fogAmount(d));
})";