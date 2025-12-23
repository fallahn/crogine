#pragma once

#include <string>

static inline const std::string GrassVert = 
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec2 a_texCoord0;

#include INSTANCE_ATTRIBS
#include CAMERA_UBO
#include WVP_UNIFORMS
#include SHADOWMAP_UNIFORMS_VERT

#include WIND_BUFFER
uniform sampler2D u_noiseTexture;

VARYING_OUT vec3 v_worldPosition;
VARYING_OUT vec3 v_viewPosition;
VARYING_OUT vec3 v_normalVector;
VARYING_OUT vec2 v_texCoord0;

#include SHADOWMAP_OUTPUTS

#include WIND_CALC
#include WATER_LEVEL

void main()
{
#include INSTANCE_MATRICES

    mat4 wvp = u_projectionMatrix * worldViewMatrix;
    vec4 position = a_position;

    vec4 worldPos = worldMatrix * position;
    float scale = 1.0 - smoothstep(20.0, 25.0, length(worldPos.xyz - u_cameraWorldPosition));


    //wind animation
    WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);
    //UV runs bottom to top so we use it as strength
    windResult.lowFreq *= a_texCoord0.y * 0.2;
    windResult.highFreq *= a_texCoord0.y * 0.04;

    //apply high frequency and low frequency in local space
    position.x += windResult.lowFreq.x + windResult.highFreq.x;
    position.z += windResult.lowFreq.y + windResult.highFreq.y;

    //multiply wind direction by wind strength
    //vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * a_texCoord0.y;
    //wind dir is added in world space (below)






    gl_Position = wvp * position * scale;

    v_worldPosition = worldPos.xyz * scale;
    v_viewPosition = (u_viewMatrix * vec4(v_worldPosition, 1.0)).xyz;

    gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    gl_ClipDistance[1] = dot(worldPos, vec4(vec3(0.0, 1.0, 0.0), WaterLevel - 0.001));

#include SHADOWMAP_VERTEX_PROC

    vec3 normal = a_normal;

    v_normalVector = normalMatrix * normal;
    v_texCoord0 = a_texCoord0;
})";


static inline const std::string GrassFrag = 
R"(
#include SHADOWMAP_UNIFORMS_FRAG
#include CAMERA_UBO
#include LIGHT_UBO

VARYING_IN vec3 v_worldPosition;
VARYING_IN vec3 v_viewPosition;
VARYING_IN vec3 v_normalVector;
VARYING_IN vec2 v_texCoord0;

#include SHADOWMAP_INPUTS
#include CASCADE_SELECTION
#include VSM_SHADOWS
#include LIGHT_COLOUR

#include OUTPUT_LOCATION

const vec3 ColourDark = vec3(0.188,0.332,0.357);
const vec3 ColourLight = vec3(0.157,0.306,0.263);
//const vec3 ColourLight = vec3(1.0);
const vec3 ColourSpec = vec3(0.275,0.494,0.243);

void main()
{
    vec3 normal = normalize(v_normalVector);

    //hmmmmm
    if(!gl_FrontFacing) normal *= -1.0;


#if defined(USE_MRT)
#if defined (VIEW_POS)
    POS_OUT.r = v_viewPosition.z;
#else
    POS_OUT = vec4(v_worldPosition, 1.0);
#endif
    NORM_OUT = vec4(normalize(v_normalVector) * 0.5 + 0.5, 1.0);
    LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif


    vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
    vec3 lightDirection = normalize(-u_lightDirection);

    vec3 halfVec = normalize(eyeDirection + lightDirection);
    float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);

    vec3 baseColour = mix(ColourDark * 0.5, ColourLight, v_texCoord0.y);
    baseColour += ColourSpec * pow(specularAngle, 120.0);

    /*float rim = 1.0 - dot(normal, eyeDirection);
    rim = smoothstep(0.99002, 1.0, rim);
    baseColour += vec3(rim);*/

    int cascadeIndex = getCascadeIndex();
    float shadow = shadowAmount(cascadeIndex);
    float fade = smoothstep(u_frustumSplits[cascadeIndex] + 0.5, u_frustumSplits[cascadeIndex],  v_viewDepth);
    if(fade > 0)
    {
        int nextIndex = min(cascadeIndex + 1, u_cascadeCount - 1);
        shadow = mix(shadow, shadowAmount(nextIndex), fade);
    }

    FRAG_OUT  = vec4(baseColour * shadow, 1.0) * getLightColour();
})";