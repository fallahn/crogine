#pragma once

#include <string>

static inline const std::string HoleVertex =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec3 a_tangent;
ATTRIBUTE vec3 a_bitangent;
ATTRIBUTE vec2 a_texCoord0;

#include CAMERA_UBO
#include WVP_UNIFORMS

VARYING_OUT vec2 v_texCoord0;
VARYING_OUT vec4 v_colour;
VARYING_OUT vec3 v_tanWorldPosition;
VARYING_OUT vec3 v_tanCamPosition;

vec3 toTanSpace(vec3 p, vec3 t, vec3 b, vec3 n)
{
    return normalize(t * p.x + b * p.y + n * p.z);
}

void main()
{
    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
    vec4 position = a_position;


    v_texCoord0 = a_texCoord0;
    v_colour = a_colour;

    //vec3 t = normalize(u_normalMatrix * a_tangent);
    //vec3 b = normalize(u_normalMatrix * a_bitangent);
    //vec3 n = normalize(u_normalMatrix * a_normal);

    //vec3 worldPos = (u_worldMatrix * position).xyz;
    //v_tanWorldPosition = toTanSpace(worldPos, t, b, n);
    //v_tanCamPosition = toTanSpace(u_cameraWorldPosition, t, b, n);

    vec3 t = normalize(mat3(u_worldMatrix) * a_tangent);
    vec3 b = normalize(mat3(u_worldMatrix) * -a_bitangent); //WHY is this backwards??
    vec3 n = normalize(mat3(u_worldMatrix) * a_normal);

    mat3 tbn = transpose(mat3(t,b,n));

    vec3 worldPos = (u_worldMatrix * position).xyz;
    v_tanWorldPosition = tbn * worldPos;
    v_tanCamPosition = tbn * u_cameraWorldPosition;

    gl_Position = wvp * position;
}
)";

static inline const std::string HoleFragment =
R"(
OUTPUT

uniform sampler2D u_depthMap;
uniform sampler2D u_diffuseMap;

VARYING_IN vec2 v_texCoord0;
VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_tanWorldPosition;
VARYING_IN vec3 v_tanCamPosition;

const float scale = 1.2;
const uint layers = 16u;
const float minDarkness = 0.3;

vec2 calcCoords(vec3 eyeDirection)
{
    float depth = (1.0 - texture(u_depthMap, v_texCoord0).r) * scale;
    vec2 pos = (eyeDirection.xy / eyeDirection.z) * depth;
    return v_texCoord0 - pos;
}

vec2 calcCoordsStepped(vec3 eyeDirection)
{
    float layerThickness = 1.0 / float(layers);
    float currDepth = 0.0;

    vec2 pos = eyeDirection.xy * scale;
    vec2 delta = pos / float(layers);

    vec2 coords = v_texCoord0;
    float depthVal = 1.0 - TEXTURE(u_depthMap, coords).r;

    while(currDepth < depthVal)
    {
        coords -= delta;
        depthVal = 1.0 - TEXTURE(u_depthMap, coords).r;
        currDepth += layerThickness;
    }

    return coords;
}

void main()
{
    //vec2 uv = calcCoords(eyeDirection);
    //vec2 uv = calcCoordsStepped(eyeDirection);

    //we need to use a diffuse map here (maybe put depth on the alpha)
    //as we can't offset the lookup into the vertex colours...
    //FRAG_OUT = vec4(TEXTURE(u_diffuseMap, uv).rgb, 1.0);



    vec3 eyeDirection = normalize(v_tanCamPosition - v_tanWorldPosition);
    vec2 uv = v_texCoord0;

    vec2 delta = eyeDirection.xy / float(layers) * scale;
    float z = 0.0;
    float depth = 1.0 - TEXTURE(u_depthMap, uv).r;
    float lastDepth = depth;
    float lastZ = z;
    
    float layerStep = 1.0 / float(layers);
    
    for (uint i = 0u; i < layers; i++)
    {
        if( z >= depth )
        {
            break;
        }
        
        uv -= delta;
        lastZ = z;
        z += layerStep;
        lastDepth = depth;
        depth = 1.0 - TEXTURE(u_depthMap, uv).r;
    }
    
    float weight = (z - depth) / ((lastDepth - lastZ ) - ( depth - z ));
    uv -= delta * weight;
    depth -= (depth - lastDepth) * weight;
    
    vec3 colour = /*TEXTURE(u_diffuseMap, uv).rgb*/vec3(minDarkness + ((1.0 - minDarkness) * (1.0 - depth)));
    FRAG_OUT = vec4(colour, 1.0);
}
)";
