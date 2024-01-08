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
ATTRIBUTE vec2 a_texCoord0;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;
uniform mat3 u_normalMatrix;
uniform vec3 u_cameraWorldPosition;

uniform vec2 u_texCoordScale = vec2(1.0);

VARYING_OUT vec2 v_tanPosition;
VARYING_OUT vec3 v_tanEyeDir;
#line 1
void main()
{
    vec3 bitan = normalize(cross(a_normal, a_tangent));

    vec4 camLocal = inverse(u_worldMatrix) * vec4(u_cameraWorldPosition, 1.0);
    vec3 eyeVec = a_position.xyz - camLocal.xyz;

    v_tanEyeDir = vec3(dot(eyeVec, a_tangent), dot(eyeVec, bitan), dot(eyeVec, a_normal));
    v_tanPosition = a_texCoord0 * u_texCoordScale;

    gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;
}
)";

inline const std::string IMFragment = 
R"(
OUTPUT

uniform sampler2D u_roomTexture;
//uniform samplerCube u_roomTexture;
uniform vec3 u_roomSize = vec3(1.0);
uniform float u_backPlaneScale = 0.5;

VARYING_IN vec2 v_tanPosition;
VARYING_IN vec3 v_tanEyeDir;

const vec3 RoomSize = vec3(1.0, 1.0, 1.0); //TODO we could encode this in the vertex colour
const float BackPlaneScale = 0.5;

void main()
{
    vec3 rayStart = fract(vec3(v_tanPosition, 0.0) / u_roomSize);
    vec3 rayDir = normalize(v_tanEyeDir) / u_roomSize;

    vec3 boxMin = floor(vec3(/*v_tanPosition*/0.0, 0.0, -1.0));
    vec3 boxMid = boxMin + vec3(0.5);
    vec3 boxMax = boxMin + vec3(1.0);

    vec3 planes = mix(boxMin, boxMax, step(0.0, rayDir));
    vec3 tanPlane = (planes - rayStart) / rayDir;

    float tanDist = min(min(tanPlane.x, tanPlane.y), tanPlane.z);

    vec3 roomRay = ((rayDir * tanDist) + rayStart) - boxMid;
    //FRAG_OUT = vec4(TEXTURE_CUBE(u_roomTexture, roomRay).rgb, 1.0);

    vec2 roomUV = roomRay.xy * mix(u_backPlaneScale, 1.0, roomRay.z + 0.5) + 0.5;
    FRAG_OUT = vec4(TEXTURE(u_roomTexture, roomUV).rgb, 1.0);
}
)";

/*
Alt version based on https://godotshaders.com/shader/fake-window-interior-shader/
*/

inline const std::string IMFragment2 = 
R"(
OUTPUT

uniform sampler2D u_roomTexture;
uniform float u_backPlaneScale = 0.5;

VARYING_IN vec2 v_tanPosition;
VARYING_IN vec3 v_tanEyeDir;

const float DepthScale = 0.5;

void main()
{
    float depthScale = 1.0 / (1.0 - u_backPlaneScale) - 1.0;

    vec3 rayStart = vec3(fract(v_tanPosition) * 2.0 - 1.0, -1.0);
    vec3 rayDir = v_tanEyeDir;
    rayDir.z *= -depthScale;

    vec3 inverseDir = 1.0 / rayDir;
    vec3 tanPlane = abs(inverseDir) - rayStart * inverseDir;
    float tanDist = min(min(tanPlane.x, tanPlane.y), tanPlane.z);
    rayStart = tanDist * rayDir + rayStart;


    float interp = rayStart.z * 0.5 + 0.5;

    //this corrects for perspective in the room texture
    //and assumes the FOV of the texture was atan(0.5)

    float realDepth = clamp(interp, 0.0, 1.0) / depthScale + 1.0;
    interp = 1.0 - (1.0 / realDepth);
    interp *= depthScale + 1.0;

    vec2 roomUV = rayStart.xy * mix(1.0, u_backPlaneScale, interp);
    roomUV = roomUV * 0.5 + 0.5;

    FRAG_OUT = vec4(TEXTURE(u_roomTexture, roomUV).rgb, 1.0);
}
)";