#pragma once

#include <string>

static inline const std::string WaterFrag =
R"(
OUTPUT

uniform samplerCube u_skybox;
uniform sampler2DArray u_normalMap;
uniform sampler2D u_reflectionMap;
uniform vec3 u_lightDirection;
uniform float u_time = 0.0;

layout (std140) uniform CameraUniforms
{
    mat4 u_viewMatrix;
    mat4 u_viewProjectionMatrix;
    mat4 u_projectionMatrix;
    vec4 u_clipPlane;
    vec3 u_cameraWorldPosition;
};

#include SHADOWMAP_UNIFORMS_FRAG

VARYING_IN vec3 v_worldPosition;

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec3 v_tbn[3];

VARYING_IN vec4 v_reflectionPosition;
//VARYING_IN vec4 v_refractionPosition;

#include SHADOWMAP_INPUTS
#include CASCADE_SELECTION
#include VSM_SHADOWS

void main()
{
    vec3 normal = texture(u_normalMap, vec3(v_texCoord0 * 4.0, mod(u_time * 18.0, MAXFRAMES))).rgb * 2.0 - 1.0;
    normal = normalize(v_tbn[0] * normal.r + v_tbn[1] * normal.g + v_tbn[2] * normal.b);

    vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
    vec3 R = reflect(-eyeDirection, normal);
    vec4 skyboxColour = texture(u_skybox, R);

    vec2 reflectCoords = v_reflectionPosition.xy / v_reflectionPosition.w / 2.0 + 0.5;
    vec3 reflectColour = TEXTURE(u_reflectionMap, reflectCoords + (normal.xz * 0.01)).rgb;

    float fresnel = dot(reflect(-eyeDirection, normal), normal);
    const float bias = 0.6;
    fresnel = (fresnel * (1.0 - bias)) + bias;


    vec3 blendedColour = mix(reflectColour, skyboxColour.rgb, fresnel);

    int cascadeIndex = getCascadeIndex();
    float shadow = shadowAmount(cascadeIndex);
    float fade = smoothstep(u_frustumSplits[cascadeIndex] + 0.5, u_frustumSplits[cascadeIndex],  v_viewDepth);
    if(fade > 0)
    {
        int nextIndex = min(cascadeIndex + 1, u_cascadeCount - 1);
        shadow = mix(shadow, shadowAmount(nextIndex), fade);
    }

    blendedColour *= shadow;


//vec3 Colours[4] = vec3[4](vec3(0.2,0.0,0.0), vec3(0.0,0.2,0.0),vec3(0.0,0.0,0.2),vec3(0.2,0.0,0.2));
//for(int i = 0; i < u_cascadeCount; ++i)
//{
//    if (v_lightWorldPosition[i].w > 0.0)
//    {
//        vec2 coords = v_lightWorldPosition[i].xy / v_lightWorldPosition[i].w / 2.0 + 0.5;
//        if (coords.x > 0 && coords.x < 1 
//                && coords.y > 0 && coords.y < 1)
//        {
//            blendedColour += Colours[i];
//        }
//    }
//}





    FRAG_OUT = vec4(blendedColour, 1.0);
})";
