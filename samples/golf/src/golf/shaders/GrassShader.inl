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
VARYING_OUT float v_fadeDistance;

#include SHADOWMAP_OUTPUTS

#include WIND_CALC
#include WATER_LEVEL

void main()
{
#include INSTANCE_MATRICES

    mat4 wvp = u_projectionMatrix * worldViewMatrix;
    vec4 position = a_position;
    

    //wind animation
    WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);
    //UV runs bottom to top so we use it as strength
    windResult.lowFreq *= a_texCoord0.y * 0.2;
    windResult.highFreq *= a_texCoord0.y * 0.04;

    //apply high frequency and low frequency in local space
    position.x += windResult.lowFreq.x + windResult.highFreq.x;
    position.z += windResult.lowFreq.y + windResult.highFreq.y;
    gl_Position = wvp * position;

    //multiply wind direction by wind strength
    vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * a_texCoord0.y;
    
    vec4 worldPos = worldMatrix * position;
    worldPos.xyz += windDir;

    v_worldPosition = worldPos.xyz;
    v_viewPosition = (u_viewMatrix * vec4(v_worldPosition, 1.0)).xyz;
    v_fadeDistance = 1.0 - smoothstep(23.0, 28.0, length(worldPos.xz - u_cameraWorldPosition.xz));

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
uniform float u_alpha = 1.0;

VARYING_IN vec3 v_worldPosition;
VARYING_IN vec3 v_viewPosition;
VARYING_IN vec3 v_normalVector;
VARYING_IN vec2 v_texCoord0;
VARYING_IN float v_fadeDistance;

#include SHADOWMAP_INPUTS
#include CASCADE_SELECTION
#include VSM_SHADOWS
#include LIGHT_COLOUR
#include BAYER_MATRIX

#include OUTPUT_LOCATION

//const vec3 ColourDark = vec3(0.188,0.332,0.357);
//const vec3 ColourLight = vec3(0.157,0.306,0.263);
//const vec3 ColourLight = vec3(1.0);
#if !defined(GRASS_COL)
const vec3 ColourDark = vec3(0.1294,0.251,0.2157);
//const vec3 ColourLight = vec3(0.157,0.306,0.263);
const vec3 ColourLight = vec3(0.149,0.2863,0.2627);
//const vec3 ColourLight = vec3(0.1255,0.2706,0.2078);
#endif
//const vec3 ColourSpec = vec3(0.275,0.494,0.243);

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

    float specularAngle = dot(normal, halfVec);//clamp(dot(normal, halfVec), 0.0, 1.0);

    vec3 baseColour = mix(ColourDark * 0.8, ColourLight, 1.0 - pow(1.0 - v_texCoord0.y, 9.0));
    baseColour += (ColourLight * 0.1) * specularAngle;

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

    vec2 xy = gl_FragCoord.xy;
    int x = int(mod(xy.x, MatrixSize));
    int y = int(mod(xy.y, MatrixSize));
    float alpha = findClosest(x, y, v_fadeDistance);
    FRAG_OUT.a *= alpha * u_alpha;// * step(WaterLevel - 0.001, v_worldPosition.y);

    if(FRAG_OUT.a < 0.3) discard;
})";