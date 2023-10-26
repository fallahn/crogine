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
uniform sampler2D u_texture;
uniform sampler2D u_depthTexture;
uniform sampler2D u_lightTexture;

uniform vec4 u_lightColour;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

#include LIGHT_COLOUR

#include FOG_COLOUR

void main()
{
    vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;

#if defined(LIGHT_COLOUR)
    colour.rgb += TEXTURE(u_lightTexture, v_texCoord).rgb;
#endif

    colour.rgb = dim(colour.rgb);

    float depthSample = TEXTURE(u_depthTexture, v_texCoord).r;

    float d = getDistance(depthSample);
    FRAG_OUT = mix(colour, FogColour, fogAmount(d));


    //FRAG_OUT = mix(colour, vec4(d,d,d,1.0), u_density / 10.0);
})";