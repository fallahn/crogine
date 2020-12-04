/*-----------------------------------------------------------------------

Matt Marchant 2020
http://trederia.blogspot.com

crogine application - Zlib license.

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

static const std::string SeaVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent;
    ATTRIBUTE vec3 a_bitangent;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform mat4 u_reflectionMatrix;

    VARYING_OUT vec3 v_tbn[3];
    VARYING_OUT vec2 v_texCoord;

    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec4 v_reflectionPosition;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * position;

        v_tbn[0] = normalize(u_worldMatrix * vec4(a_tangent, 0.0)).xyz;
        v_tbn[1] = normalize(u_worldMatrix * vec4(a_bitangent, 0.0)).xyz;
        v_tbn[2] = normalize(u_worldMatrix * vec4(a_normal, 0.0)).xyz;

        v_texCoord = a_texCoord0;

        v_worldPosition = position.xyz;
        v_reflectionPosition = u_reflectionMatrix * position;

    })";

static const std::string SeaFragment = R"(
    OUTPUT

    uniform sampler2D u_depthMap;
    uniform sampler2D u_normalMap;
    uniform sampler2D u_reflectionMap;

    uniform vec3 u_lightDirection;
    uniform vec4 u_lightColour;

    uniform vec2 u_textureOffset;
    uniform vec3 u_cameraWorldPosition;

    uniform float u_time;

    VARYING_IN vec3 v_tbn[3];
    VARYING_IN vec2 v_texCoord;

    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec4 v_reflectionPosition;

    const vec2 TextureScale = vec2(8.0);
    const vec3 colour = vec3(0.137, 0.267, 0.53);

    vec3 diffuseColour = colour;
    vec3 eyeDirection = vec3(0.0);

    vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
    {
        float diffuseAmount = max(dot(normal, lightDirection), 0.0);

        vec3 mixedColour = diffuseColour.rgb * lightDiffuse * diffuseAmount * falloff;

        vec3 halfVec = normalize(eyeDirection + lightDirection);
        float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
        vec3 specularColour = lightSpecular * vec3(pow(specularAngle, (254.0 + 1.0))) * falloff;

        return clamp(mixedColour + specularColour, 0.0, 1.0);
    }

    void main()
    {
        eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
        vec2 coord = (v_texCoord + u_textureOffset) * TextureScale;

        float time = u_time * 0.01;
        vec3 texNormal = TEXTURE(u_normalMap, coord + vec2(time, time * 0.5)).rgb * 2.0 - 1.0;
        vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);

        vec2 reflectCoords = v_reflectionPosition.xy / v_reflectionPosition.w / 2.0 + 0.5;
        vec3 reflectColour = TEXTURE(u_reflectionMap, reflectCoords + (normal.rg * 0.01)).rgb * u_lightColour.rgb * 0.25;

        vec3 blendedColour = colour * 0.2; //ambience
        blendedColour += calcLighting(normal, normalize(-u_lightDirection), u_lightColour.rgb, u_lightColour.rgb, 1.0);
        blendedColour += reflectColour;

        FRAG_OUT = vec4(blendedColour, 1.0);
    })";
