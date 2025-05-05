#pragma once

#include <string>

static inline const std::string HoleVertex =
R"(
ATTRIBUTE vec4 a_position;

ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec3 a_tangent;
ATTRIBUTE vec3 a_bitangent;
ATTRIBUTE vec2 a_texCoord0;

#include CAMERA_UBO
#include WVP_UNIFORMS

VARYING_OUT vec2 v_texCoord0;
VARYING_OUT vec3 v_tanWorldPosition;
VARYING_OUT vec3 v_tanCamPosition;

void main()
{
    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
    gl_Position = wvp * a_position;

    vec3 t = normalize(mat3(u_worldMatrix) * a_tangent);
    vec3 b = normalize(mat3(u_worldMatrix) * -a_bitangent); //WHY is this backwards??
    vec3 n = normalize(mat3(u_worldMatrix) * a_normal);

    mat3 tbn = transpose(mat3(t,b,n));

    vec3 worldPos = (u_worldMatrix * a_position).xyz;
    v_tanWorldPosition = tbn * worldPos;
    v_tanCamPosition = tbn * u_cameraWorldPosition;

    v_texCoord0 = a_texCoord0;
}
)";

static inline const std::string HoleFragment =
R"(
//OUTPUT

//uniform sampler2D u_depthMap;
//uniform sampler2D u_diffuseMap;

//vec2 calcCoords(vec3 eyeDirection)
//{
//    float depth = (1.0 - texture(u_depthMap, v_texCoord0).r) * scale;
//    vec2 pos = (eyeDirection.xy / eyeDirection.z) * depth;
//    return v_texCoord0 - pos;
//}
//
//vec2 calcCoordsStepped(vec3 eyeDirection)
//{
//    float layerThickness = 1.0 / float(layers);
//    float currDepth = 0.0;
//
//    vec2 pos = eyeDirection.xy * scale;
//    vec2 delta = pos / float(layers);
//
//    vec2 coords = v_texCoord0;
//    float depthVal = 1.0 - TEXTURE(u_depthMap, coords).r;
//
//    while(currDepth < depthVal)
//    {
//        coords -= delta;
//        depthVal = 1.0 - TEXTURE(u_depthMap, coords).r;
//        currDepth += layerThickness;
//    }
//
//    return coords;
//}

#define USE_MRT
#include OUTPUT_LOCATION

uniform vec4 u_lightColour;

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec3 v_tanWorldPosition;
VARYING_IN vec3 v_tanCamPosition;

const float Scale = 1.2;
const uint Layers = 16u;
const float MinDarkness = 0.3;

const float RadiusOuter = 0.49;
const float RadiusInner = 0.45;

float getDepth(vec2 uv)
{
    float l = length(uv - 0.5);
    float c = smoothstep(RadiusInner, RadiusOuter, l);
    c += (l/RadiusInner) * 0.2;

    return 1.0 - c;
}

#include LIGHT_COLOUR

void main()
{
    //vec2 uv = calcCoords(eyeDirection);
    //vec2 uv = calcCoordsStepped(eyeDirection);


    vec3 eyeDirection = normalize(v_tanCamPosition - v_tanWorldPosition);
    vec2 uv = v_texCoord0;

    vec2 delta = eyeDirection.xy / float(Layers) * Scale;
    float z = 0.0;
    float depth = getDepth(uv);// 1.0 - TEXTURE(u_depthMap, uv).r;
    float lastDepth = depth;
    float lastZ = z;
    
    float layerStep = 1.0 / float(Layers);
    
    for (uint i = 0u; i < Layers; i++)
    {
        if( z >= depth )
        {
            break;
        }
        
        uv -= delta;
        lastZ = z;
        z += layerStep;
        lastDepth = depth;
        depth = getDepth(uv);//1.0 - TEXTURE(u_depthMap, uv).r;
    }
    
    float weight = (z - depth) / ((lastDepth - lastZ ) - ( depth - z ));
    uv -= delta * weight;
    depth -= (depth - lastDepth) * weight;
    
    vec3 colour = vec3(1.0, 0.973, 0.882) * getLightColour().rgb * (MinDarkness + ((1.0 - MinDarkness) * (1.0 - depth)));
    FRAG_OUT = vec4(colour, 1.0);
    LIGHT_OUT = vec4(vec3(0.0), 1.0);
}
)";
