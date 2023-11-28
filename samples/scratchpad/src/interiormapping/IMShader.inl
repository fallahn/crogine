/*
Tangent space interior mapping shader based on https://andrewgotow.com/2018/09/09/interior-mapping-part-2/
*/

#pragma once

#include <string>

inline const std::string IMVertex = 
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;
ATTRIBUTE vec3 a_tangent;
//ATTRIBUTE vec3 a_bitangent;
ATTRIBUTE vec2 a_texCoord0;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;
uniform mat3 u_normalMatrix;
uniform vec3 u_cameraWorldPosition;

VARYING_OUT vec2 v_tanPosition;
VARYING_OUT vec3 v_tanEyeDir;

void main()
{
    vec4 worldPos = u_worldMatrix * a_position;
    vec3 eyeVec = worldPos.xyz - u_cameraWorldPosition;

//hum. fix this - or at least find the sign
vec3 bitan = /*normalize*/(cross(a_normal, a_tangent));

    mat3 tanMatrix = 
    mat3(
        normalize(u_normalMatrix * a_tangent),
        normalize(u_normalMatrix * bitan),
        normalize(u_normalMatrix * a_normal)
    );
    v_tanEyeDir = transpose(tanMatrix) * eyeVec;

    v_tanPosition = a_texCoord0;
    gl_Position = u_viewProjectionMatrix * worldPos;
}
)";

inline const std::string IMFragment = 
R"(
OUTPUT

uniform sampler2D u_roomTexture;

VARYING_IN vec2 v_tanPosition;
VARYING_IN vec3 v_tanEyeDir;

const vec3 RoomSize = vec3(1.0, 1.0, 1.0); //TODO we could encode this in the vertex colour
const float BackPlaneScale = 0.5;

void main()
{
    vec3 rayStart = fract(vec3(v_tanPosition, 0.0) / RoomSize);
    vec3 rayDir = normalize(v_tanEyeDir) / RoomSize;

    vec3 boxMin = floor(vec3(v_tanPosition, -1.0));
    vec3 boxMid = boxMin + vec3(0.5);
    vec3 boxMax = boxMin + vec3(1.0);

    vec3 planes = mix(boxMin, boxMax, step(0.0, rayDir));
    vec3 tanPlane = (planes - rayStart) / rayDir;

    float tanDist = min(min(tanPlane.x, tanPlane.y), tanPlane.z);

    vec3 roomRay = ((rayDir * tanDist) + rayStart) - boxMid;
    vec2 roomUV = (roomRay.xy * mix(BackPlaneScale, 1.0, roomRay.z + 0.5)) + 0.5;

    FRAG_OUT = vec4(TEXTURE(u_roomTexture, roomUV).rgb, 1.0);
}
)";