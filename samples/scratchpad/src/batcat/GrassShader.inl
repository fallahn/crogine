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

VARYING_OUT vec3 v_worldPosition;
VARYING_OUT vec3 v_normalVector;
VARYING_OUT vec2 v_texCoord0;

#include SHADOWMAP_OUTPUTS

const float Bend = 0.2; //max bend in world units
//TODO calc a world-space UV and sample a noise map to:
//vary the height of the grass
//vary the colour (lighter - shorter) of the grass
//TODO add a time variable to the coordinate and create a wind effect

void main()
{
#include INSTANCE_MATRICES

    mat4 wvp = u_projectionMatrix * worldViewMatrix;
    vec4 position = a_position;

//curves blade based on height
position.z += pow(a_texCoord0.y * Bend, 2.0);

    gl_Position = wvp * position;

    vec4 worldPos = worldMatrix * position;
    v_worldPosition = worldPos.xyz;
    gl_ClipDistance[0] = dot(worldPos, u_clipPlane);

#include SHADOWMAP_VERTEX_PROC

    vec3 normal = a_normal;
//fakes normal defomation by pointing it upwards near the top
normal = mix(normal, vec3(0.0, 1.0, 0.0), a_texCoord0.y);

    v_normalVector = normalMatrix * normal;
    v_texCoord0 = a_texCoord0;

})";


static inline const std::string GrassFrag = 
R"(
#include SHADOWMAP_UNIFORMS_FRAG
#include CAMERA_UBO
uniform vec3 u_lightDirection;

VARYING_IN vec3 v_worldPosition;
VARYING_IN vec3 v_normalVector;
VARYING_IN vec2 v_texCoord0;

#include SHADOWMAP_INPUTS
#include CASCADE_SELECTION
#include VSM_SHADOWS

OUTPUT

const vec3 ColourDark = vec3(0.188,0.332,0.357);
const vec3 ColourLight = vec3(0.157,0.306,0.263);
const vec3 ColourSpec = vec3(0.275,0.494,0.243);

void main()
{
    vec3 normal = normalize(v_normalVector);

//hmmmmm
if(!gl_FrontFacing) normal *= -1.0;

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

    FRAG_OUT  = vec4(baseColour * shadow, 1.0);
})";