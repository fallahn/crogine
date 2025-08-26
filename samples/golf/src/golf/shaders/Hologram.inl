/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2025
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

//u_windData.w == elapsed time
static const inline std::string HoloFrag =
R"(

#define USE_MRT
#include OUTPUT_LOCATION
#include CAMERA_UBO
#include WIND_BUFFER

uniform sampler2D u_diffuseMap;
uniform vec4 u_colour;

#line 1

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec3 v_normalVector;
VARYING_IN vec3 v_worldPosition;

const float LineFreq = 1000.0;
const float FarAmount = 0.3;
const float NearAmount = 0.6;

const vec3 LumConst = vec3(0.2125, 0.7154, 0.0721);

void main()
{
    vec2 uv = v_texCoord0.xy;
    uv.x += u_windData.w * 0.02;

    float offset = (sin((u_windData.w * 0.5) + (uv.y * 10.0)));
    uv.x += step(0.6, offset) * offset * 0.01;

    vec3 viewDir = u_cameraWorldPosition - v_worldPosition;

    vec4 tint = mix(u_colour * FarAmount, u_colour * NearAmount, 
                    step(0.5, (dot(normalize(viewDir), normalize(v_normalVector)) + 1.0 * 0.5)));

    vec4 strength = TEXTURE(u_diffuseMap, uv);
    vec4 colour = strength * tint * 0.94;

    float scanline = ((sin((uv.y * LineFreq) + (u_windData.w * 30.0)) + 1.0 * 0.5) * 0.4) + 0.6;

    scanline = mix(scanline, 1.0, clamp(length(viewDir) / 5.0, 0.0, 1.0)); //reduce scanline up to 5 metres
    colour *= mix(1.0, scanline, step(0.58, uv.y)); //arbitrary cut off for texture

    //flicker
    colour *= (step(0.95, fract(sin(u_windData.w * 0.01) * 43758.5453123)) * 0.2) + 0.8;
    FRAG_OUT = colour;
    FRAG_OUT.rgb *= 4.0;

    float luminance = clamp(dot(colour.rgb, LumConst) * 1.5, 0.0, 1.0);

    //LIGHT_OUT = vec4(colour.rgb * luminance, 1.0);
    //NORM_OUT.a = luminance;
})";