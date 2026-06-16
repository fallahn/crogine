#pragma once

#include <string>

static inline const std::string WaterFrag =
R"(
OUTPUT

uniform samplerCube u_skybox;
uniform sampler2DArray u_normalMap;
uniform sampler2D u_reflectionMap;
uniform sampler2D u_refractionMap;
uniform sampler2D u_depthMap;
uniform vec4 u_lightColour;
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
VARYING_IN vec4 v_refractionPosition;

#include SHADOWMAP_INPUTS
#include CASCADE_SELECTION
#include VSM_SHADOWS


const float ZNear = 0.1;
#if !defined(ZFAR)
#define ZFAR 50.0
#endif
const float ZFar = ZFAR;

uniform float u_density = 0.5;
uniform float u_fogStart = 0.01;
uniform float u_fogEnd = 5.5;

const vec4 FogColour = vec4(0.91,0.92,0.923,1.0);

float fogAmount(float distance)
{
    //linear
    return clamp(smoothstep(u_fogStart, u_fogEnd, distance * ZFar) * u_density, 0.0, 1.0);
}

float getDistance(float ds)
{
    return (2.0 * ZNear) / (ZFar + ZNear - ds * (ZFar - ZNear));
}




void main()
{
    vec3 normal = texture(u_normalMap, vec3(v_texCoord0 * 4.0, mod(u_time * 18.0, MAXFRAMES))).rgb * 2.0 - 1.0;
    normal = normalize(v_tbn[0] * normal.r + v_tbn[1] * normal.g + v_tbn[2] * normal.b);

    vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
    vec3 R = reflect(-eyeDirection, normal);
    vec4 skyboxColour = texture(u_skybox, R);

    vec2 reflectCoords = v_reflectionPosition.xy / v_reflectionPosition.w / 2.0 + 0.5;
    vec3 reflectColour = TEXTURE(u_reflectionMap, reflectCoords + (normal.xz * 0.05)).rgb;

    vec2 refractCoords = v_refractionPosition.xy / v_refractionPosition.w / 2.0 + 0.5;
    vec3 refractColour = TEXTURE(u_refractionMap, refractCoords + (normal.xz * 0.02)).rgb;// * 0.6;

    //float depthSample = TEXTURE(u_depthMap, refractCoords).r;
    //float d = getDistance(depthSample);
    //float fogMix = fogAmount(d);
    //refractColour = mix(refractColour, vec3(0.0, 0.0, 1.0), fogMix);
    //refractColour = mix(vec3(1.0), vec3(0.0, 0.0, 1.0), d);


    float fresnel = dot(reflect(-eyeDirection, normal), normal);
    const float bias = 0.2;
    fresnel = (fresnel * (1.0 - bias)) + bias;

    reflectColour *= refractColour;
    vec3 blendedColour = mix(reflectColour, skyboxColour.rgb * u_lightColour.rgb, fresnel);
    //blendedColour = mix(refractColour, blendedColour, 0.5);


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


static const inline std::string ShapeFrag =
R"(
OUTPUT
#include CAMERA_UBO
#include LIGHT_UBO

uniform samplerCube u_skybox;
uniform vec4 u_maskColour;
uniform vec4 u_colour = vec4(1.0);

VARYING_IN vec3 v_normalVector;
VARYING_IN vec3 v_worldPosition;

vec4 diffuseColour = u_colour;
vec3 eyeDirection;
vec4 mask = vec4(1.0, 1.0, 1.0, 0.0);

vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
{
    MED float diffuseAmount = max(dot(normal, lightDirection), 0.0);
    //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
    MED vec3 mixedColour = diffuseColour.rgb * lightDiffuse * diffuseAmount * falloff;

    MED vec3 halfVec = normalize(eyeDirection + lightDirection);
    MED float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
    LOW vec3 specularColour = lightSpecular * vec3(pow(specularAngle, ((254.0 * mask.r) + 1.0))) * falloff;

    return clamp(mixedColour + (specularColour * mask.g), 0.0, 1.0);
}

void main()
{
    vec3 normal = normalize(v_normalVector);
    mask = u_maskColour;

    vec3 blendedColour = diffuseColour.rgb * 0.2; //ambient
    eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

    blendedColour += calcLighting(normal, normalize(-u_lightDirection), u_lightColour.rgb, vec3(1.0), 1.0);

    FRAG_OUT = vec4(blendedColour, 1.0);

    vec3 R = reflect(-eyeDirection, normal);
    //FRAG_OUT.rgb = mix(TEXTURE_CUBE(u_skybox, R).rgb * u_lightColour.rgb, FRAG_OUT.rgb, mask.a);
    FRAG_OUT.rgb += TEXTURE_CUBE(u_skybox, R).rgb * u_lightColour.rgb * mask.a;


    //we're making the supposition here that the water plane is on Y zero
    //however we ought to be using the clip plane in some way, while also
    //accounting for which pass we're currently rendering

    float dist = dot(u_clipPlane.xyz, v_worldPosition) + u_clipPlane.w;
    FRAG_OUT.rgb *= 0.1 + (0.9 * smoothstep(-5.0, 0.0, -dist));
})";

static inline const std::string TerrainVertex =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE MED vec2 a_texCoord0;

#include CAMERA_UBO
#include WVP_UNIFORMS

//TODO height can be on normal map alpha
uniform sampler2D u_heightMapA;
uniform sampler2D u_normalMapA;
uniform sampler2D u_heightMapB;
uniform sampler2D u_normalMapB;

uniform float u_blend = 0.0;

VARYING_OUT vec3 v_normalVector;
VARYING_OUT vec3 v_worldPosition;

#if !defined(MAX_HEIGHT)
#define MAX_HEIGHT 10.0
#endif

void main()
{
    //this is in model space because we can't calc world
    //space until *after* calculating the blend...
    float blendPos = u_blend * 2.0 - 1.0;
    blendPos *= 2.0;
    float blend = smoothstep(blendPos - 0.3, blendPos + 0.3, a_position.x);


    //TODO technically this in is tangent space
    vec3 normalA = normalize(TEXTURE(u_normalMapA, a_texCoord0).rgb * 2.0 - 1.0);
    vec3 normalB = normalize(TEXTURE(u_normalMapB, a_texCoord0).rgb * 2.0 - 1.0);
    v_normalVector = u_normalMatrix * normalize(mix(normalA, normalB, blend));

    vec4 position = a_position;
    float z = mix(TEXTURE(u_heightMapA, a_texCoord0).r, TEXTURE(u_heightMapB, a_texCoord0).r, blend);
    position.z = z * MAX_HEIGHT;

    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
    gl_Position = wvp * position;

    v_worldPosition = (u_worldMatrix * position).xyz;
})";

static inline const std::string TerrainFrag =
R"(
OUTPUT

uniform sampler2D u_diffuseMap;
layout (std140) uniform CameraUniforms
{
    mat4 u_viewMatrix;
    mat4 u_viewProjectionMatrix;
    mat4 u_projectionMatrix;
    vec4 u_clipPlane;
    vec3 u_cameraWorldPosition;
};

#include LIGHT_UBO

VARYING_IN vec3 v_normalVector;
VARYING_IN vec3 v_worldPosition;

const float StartFade = 25.0;
const float EndFade = 42.0;

void main()
{
    vec3 colour = TEXTURE(u_diffuseMap, vec2(0.5, v_worldPosition.y / 6.0)).rgb;
    colour *= u_lightColour.rgb;
    FRAG_OUT = vec4(colour, 1.0) * dot(normalize(v_normalVector), normalize(-u_lightDirection));
    

    //darken below water line - TODO use this to mix a shadow colour rather than just blacken
    float dist = dot(u_clipPlane.xyz, v_worldPosition) + u_clipPlane.w;
    FRAG_OUT.rgb *= 0.1 + (0.9 * smoothstep(-5.0, 0.0, -dist));

    //fade with distance
    dist = length(v_worldPosition - u_cameraWorldPosition);
    FRAG_OUT.rgb *= 0.5 + (0.5 * (1.0 - smoothstep(StartFade, EndFade, dist)));
})";