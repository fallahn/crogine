/*-----------------------------------------------------------------------

Matt Marchant 2026
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

static inline const std::string ShoreFragment =
R"(
#include OUTPUT_LOCATION

uniform sampler2D u_diffuseMap;
uniform vec4 u_lightColour;

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec3 v_normalVector;

#include WIND_BUFFER
#include LIGHT_COLOUR

const float AnimSpeed = 4.0;

const float c1 = 1.70158;
const float c3 = 2.70158;
float easeOut(float v)
{
    return 1 + c3 * pow(v - 1.0, 3.0) + c1 * pow(v - 1.0, 2.0);
}

vec4 getColour(float time, vec2 coord)
{
    float opacity = v_texCoord0.y * (1.0 - smoothstep(0.46, 0.85, time)) * smoothstep(0.0, 0.2, time);

    coord.y = max(0.0, coord.y - ((easeOut(time) / 1.5) / 2.0));
    vec4 colour = TEXTURE(u_diffuseMap, coord);
    colour.rgb *= opacity;
    return colour;
}

void main()
{
#if defined (USE_MRT)
    NORM_OUT = vec4(v_normalVector, 0.0); //masks off light map
    LIGHT_OUT = vec4(0.0, 0.0, 0.0, 1.0);
//#if defined (VIEW_POS)
//    POS_OUT.r = v_viewPosition.z;
//#else
//    POS_OUT = vec4(v_worldPosition, 1.0);
//#endif
#endif

    float time = (u_windData.w / AnimSpeed) - floor(u_windData.w / AnimSpeed);
    vec2 coord = v_texCoord0;

    vec4 colour = getColour(time, coord);

    time += 0.5;
    time -= floor(time);
    coord.x += 0.5;
    colour += getColour(time, coord);

    FRAG_OUT = colour * getLightColour();
})";