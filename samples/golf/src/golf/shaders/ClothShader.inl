/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2025
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

static const inline std::string ClothVertex =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec2 a_texCoord0;
//ATTRIBUTE vec3 a_normal; //hmm this will be wrong after deformation

#include CAMERA_UBO

uniform mat4 u_worldMatrix;
uniform sampler2D u_noiseTexture;

#include SHADOWMAP_UNIFORMS_VERT

#if !defined(MAX_CASCADES)
#define MAX_CASCADES 3
#endif

//dirX, strength, dirZ, elapsedTime
#include WIND_BUFFER
#include SHADOWMAP_OUTPUTS
#include WIND_CALC
#include RESOLUTION_BUFFER

VARYING_OUT float v_ditherAmount;
VARYING_OUT vec2 v_texCoord;

const float FarFadeDistance = 360.f;

//#define worldMatrix u_worldMatrix;

void main()
{
    v_texCoord = a_texCoord0;

    mat4 worldMatrix = u_worldMatrix;
    mat4 worldViewMatrix = u_viewMatrix * u_worldMatrix;

    vec4 position = a_position;
#include SHADOWMAP_VERTEX_PROC

    //red low freq, green high freq, blue direction amount

    WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);
    vec3 vertexStrength = a_colour.rgb;
    //multiply high and low frequency by vertex colours
    windResult.lowFreq *= vertexStrength.r;
    windResult.highFreq *= vertexStrength.g;

    //apply high frequency and low frequency in local space
    position.x += windResult.lowFreq.x + windResult.highFreq.x;
    position.z += windResult.lowFreq.y + windResult.highFreq.y;

    //multiply wind direction by wind strength
    vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * vertexStrength.b;
    //wind dir is added in world space (below)

    vec4 worldPosition = worldMatrix * position;
    worldPosition.xyz += windDir;
    vec4 vertPos = u_projectionMatrix * u_viewMatrix * worldPosition;
    //TODO vertex snapping would go here.
    gl_Position = vertPos;

    gl_ClipDistance[0] = dot(worldPosition, u_clipPlane);

    //dithering
    float fadeDistance = u_nearFadeDistance * 2.0;
    float distance = length(worldPosition.xyz - u_cameraWorldPosition);

    v_ditherAmount = pow(clamp((distance - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
    v_ditherAmount *= 1.0 - clamp((distance - FarFadeDistance) / fadeDistance, 0.0, 1.0);

})";

static inline const std::string ClothFragment =
R"(
#define USE_MRT
#include OUTPUT_LOCATION

uniform sampler2D u_diffuseMap;

#include LIGHT_UBO
#include SHADOWMAP_UNIFORMS_FRAG

VARYING_IN float v_ditherAmount;
VARYING_IN vec2 v_texCoord;

#include SHADOWMAP_INPUTS
#include CASCADE_SELECTION

#if !defined (CLASSIC_SHADOWS)
#include VSM_SHADOWS
#else
    const float Bias = 0.001; //0.005
    float shadowAmount(int cascadeIndex)
    {
        vec4 lightWorldPos = v_lightWorldPosition[cascadeIndex];

        vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
        projectionCoords = projectionCoords * 0.5 + 0.5;
        float currDepth = projectionCoords.z - Bias;

        if (projectionCoords.z > 1.0)
        {
            return 1.0;
        }

        float depthSample = TEXTURE(u_shadowMap, vec3(projectionCoords.xy, float(cascadeIndex))).r;
        return (currDepth < depthSample) ? 1.0 : 1.0 - (0.3);
    }
#endif

#include BAYER_MATRIX
#include LIGHT_COLOUR

void main()
{
    vec4 colour = TEXTURE(u_diffuseMap, v_texCoord) * getLightColour();

    int cascadeIndex = getCascadeIndex();
    float shadow = shadowAmount(cascadeIndex);
    colour.rgb *= shadow;


    vec2 xy = gl_FragCoord.xy;
    int x = int(mod(xy.x, MatrixSize));
    int y = int(mod(xy.y, MatrixSize));
    float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount)) * colour.a;
    if(alpha < 0.5) discard;

    FRAG_OUT = colour;
})";