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

inline const std::string TVFragment =
R"(
uniform sampler2D u_diffuseMap;
uniform samplerCube u_reflectMap;

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER

VARYING_IN vec2 v_texCoord;
VARYING_IN vec3 v_normal;
VARYING_IN vec3 v_cameraWorldPosition;
VARYING_IN vec3 v_worldPosition;

OUTPUT

const float LineCount = 6.28 * 256.0;

float rand(vec2 pos)
{
    return fract(sin(dot(pos, vec2(12.9898, 4.1414) + (u_windData.w * 0.1))) * 43758.5453);
}

void main()
{
    float band = smoothstep(0.9951, 0.9992, sin((v_texCoord.y * 4.0) + (u_windData.w * 0.2))) * 0.06;
    vec2 coord = v_texCoord;
    coord.x = (band * 0.1) + coord.x;

    vec4 colour = TEXTURE(u_diffuseMap, coord);

	vec3 grey = vec3(dot(vec3(0.299, 0.587, 0.114), colour.rgb));
	colour.rgb = mix(grey, colour.rgb, 0.6 - (band * 5.0));

    colour.rgb = mix(grey, colour.rgb, rand(gl_FragCoord.xy));

    float scanline = 1.0 - (0.1 * mod(gl_FragCoord.y, 2.0));
    colour.rgb *= scanline;

    scanline = (sin(v_texCoord.y * LineCount) + 1.0) * 0.5;
    scanline = (0.1 * scanline) + 0.9;
    colour.rgb *= scanline;


    colour.rgb *= 1.1;




    vec3 viewDirection = v_cameraWorldPosition - v_worldPosition;
    viewDirection = normalize(viewDirection);

    vec3 normal = normalize(v_normal);

    colour.rgb += TEXTURE_CUBE(u_reflectMap, reflect(-viewDirection, normal)).rgb * 0.15;


    FRAG_OUT = colour;

})";