/*-----------------------------------------------------------------------

Matt Marchant 2023 - 2025
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

static inline const std::string GlassFragment =
R"(
OUTPUT

#include CAMERA_UBO

uniform samplerCube u_reflectMap;

uniform vec4 u_colour;
uniform vec4 u_maskColour;

VARYING_IN vec3 v_normalVector;
VARYING_IN vec3 v_worldPosition;

void main()
{
    vec3 normal = normalize(v_normalVector);
    vec3 eyeDir = normalize(v_worldPosition - u_cameraWorldPosition);

    vec4 colour = u_colour;
    vec3 reflectColour = TEXTURE_CUBE(u_reflectMap, reflect(eyeDir, normal)).rgb;

    colour.rgb = mix(reflectColour, colour.rgb, u_maskColour.a);

    FRAG_OUT = colour;
})";

static inline const std::string WakeFragment =
R"(
OUTPUT

uniform sampler2D u_texture;
uniform float u_speed = 1.0;
uniform vec4 u_lightColour = vec4(1.0);

VARYING_IN vec2 v_texCoord0;

#include WIND_BUFFER
#include LIGHT_COLOUR

void main()
{
    float positionAlpha = clamp(1.0 - v_texCoord0.x, 0.0, 1.0);
    positionAlpha *= smoothstep(0.03, 0.1, v_texCoord0.x);
        
    vec4 colour = TEXTURE(u_texture, v_texCoord0 - vec2(u_windData.w * u_speed, 0.0));
    colour.rgb *= getLightColour().rgb;
    colour.a *= positionAlpha * clamp(u_speed, 0.0, 1.0);

    FRAG_OUT = colour;
})";