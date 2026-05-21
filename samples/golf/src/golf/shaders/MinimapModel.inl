/*-----------------------------------------------------------------------

Matt Marchant 2025 - 2026
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

//used to optimise the MRT output of the minimap
static const inline std::string MinimapModelVertex =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec2 a_texCoord0;

#include CAMERA_UBO
#include WVP_UNIFORMS

VARYING_OUT vec4 v_colour;
VARYING_OUT vec3 v_worldPosition;
VARYING_OUT vec3 v_normal;
VARYING_OUT vec2 v_texCoord;

#if defined(MULTI_TARGET)
    //projective texturing for target
    uniform mat4 u_targetViewProjectionMatrix;
    VARYING_OUT vec4 v_targetProjection;
#endif

void main()
{
    vec4 worldPos = u_worldMatrix * a_position;
    gl_Position = u_viewProjectionMatrix * worldPos;

    v_worldPosition = worldPos.xyz;
    v_normal = u_normalMatrix * a_normal;
    v_colour = a_colour;
    v_texCoord = a_texCoord0;

#if defined(MULTI_TARGET)
    v_targetProjection = u_targetViewProjectionMatrix * u_worldMatrix * a_position;
#endif
})";

static const inline std::string MinimapModelFragment =
R"(
#define USE_MRT
#include OUTPUT_LOCATION
#include LIGHT_UBO

uniform sampler2D u_diffuseMap;
uniform float u_heatmap = 0.0;
uniform float u_zoom = 1.0;

VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_worldPosition;
VARYING_IN vec3 v_normal;
VARYING_IN vec2 v_texCoord;

#if defined(MULTI_TARGET)
    VARYING_IN vec4 v_targetProjection;
#endif

#define COLOUR_LEVELS 5.0
#define AMOUNT_MIN 0.8
#define AMOUNT_MAX 0.2

const vec3 BaseHeatColour = vec3(0.827, 0.599, 0.91); //stored as HSV to save on a conversion
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main()
{
    FRAG_OUT = TEXTURE(u_diffuseMap, v_texCoord);
    if (FRAG_OUT.a < 0.5)
    {
        discard; //else alpha effects create holes in the map
    }

    //NORM_OUT = vec4(normalize(v_normal) * 0.5 + 0.5, 1.0);
    //POS_OUT.r = v_worldPosition.y;
    //
    //float greenTerrain = step(0.065, v_colour.r) * (1.0 - step(0.13, v_colour.r));
    //NORM_OUT.a = greenTerrain;

    vec3 lightDirection = normalize(-u_lightDirection);
    vec3 normal = normalize(v_normal);
    float lightStrength = clamp(dot(normal, lightDirection), 0.0, 1.0);

    lightStrength *= COLOUR_LEVELS;
    lightStrength = round(lightStrength);
    lightStrength /= COLOUR_LEVELS;
    lightStrength = (lightStrength * AMOUNT_MAX) + AMOUNT_MIN;
    FRAG_OUT.rgb *= lightStrength;

    //FRAG_OUT.rgb = (FRAG_OUT.rgb * 0.5) + (FRAG_OUT.rgb * (lightStrength * 0.5));

#if defined(MULTI_TARGET)
    //this is effectively clip-space so +/- 1 is perfect for circles
    vec2 projUV = v_targetProjection.xy / v_targetProjection.w;

    float RingCount = 5.0;
    float l = length(projUV);
    float r = step(0.0, sin(min(RingCount, l * RingCount) * 3.14));
    vec3 targetColour = mix(vec3(1.0,0.972,0.882), vec3(0.721, 0.2, 0.188), r) * 0.8;

    float targetAmount = 1.0 - step(1.0, l);
    FRAG_OUT.rgb = mix(FRAG_OUT.rgb, targetColour + FRAG_OUT.rgb, targetAmount);
#endif

    vec3 c = BaseHeatColour;
    c.x += v_worldPosition.y * 0.25 * (1.0 + (u_zoom * 0.1));
    c = hsv2rgb(c);

    FRAG_OUT.rgb = mix(FRAG_OUT.rgb, c, u_heatmap * 0.6);

    /*vec2 f = fract(v_worldPosition.xz / 2.0);
    vec2 df = fwidth(v_worldPosition.xz / 2.0);
    vec2 g = step(df, f);
    float contour = (g.x * g.y);*/


    //float f = fract(v_worldPosition.y * 150.0); //larger is closer together
    //float df = fwidth(v_worldPosition.y * 150.0);
    //float contour = smoothstep(df * 1.0, df * 2.0, f);

    //FRAG_OUT.rgb = mix(FRAG_OUT.rgb, vec3(1.0) - c, contour * 0.6 * u_heatmap);
})";