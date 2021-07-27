/*-----------------------------------------------------------------------

Matt Marchant 2021
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

static const std::string WaterVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform mat4 u_reflectionMatrix;
    uniform mat4 u_lightViewProjectionMatrix;

    VARYING_OUT vec3 v_normal;

    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec4 v_reflectionPosition;
    VARYING_OUT LOW vec4 v_lightWorldPosition;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * position;

        v_normal = u_normalMatrix * a_normal;

        v_worldPosition = position.xyz;
        v_reflectionPosition = u_reflectionMatrix * position;
        v_lightWorldPosition = u_lightViewProjectionMatrix * position;

    })";

static const std::string WaterFragment = R"(
    OUTPUT

    uniform sampler2D u_reflectionMap;
    uniform sampler2D u_shadowMap;

    uniform vec3 u_lightDirection;
    uniform vec4 u_lightColour;

    uniform vec3 u_cameraWorldPosition;
    uniform float u_time;

    VARYING_IN vec3 v_normal;

    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec4 v_reflectionPosition;
    VARYING_IN LOW vec4 v_lightWorldPosition;


    const vec3 WaterColour = vec3(0.02, 0.078, 0.578);
    //const vec3 WaterColour = vec3(0.2, 0.278, 0.278);
    //const vec3 WaterColour = vec3(0.137, 0.267, 0.53);

    vec3 diffuseColour = WaterColour;
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

    const vec2 kernel[16] = vec2[](
        vec2(-0.94201624, -0.39906216),
        vec2(0.94558609, -0.76890725),
        vec2(-0.094184101, -0.92938870),
        vec2(0.34495938, 0.29387760),
        vec2(-0.91588581, 0.45771432),
        vec2(-0.81544232, -0.87912464),
        vec2(-0.38277543, 0.27676845),
        vec2(0.97484398, 0.75648379),
        vec2(0.44323325, -0.97511554),
        vec2(0.53742981, -0.47373420),
        vec2(-0.26496911, -0.41893023),
        vec2(0.79197514, 0.19090188),
        vec2(-0.24188840, 0.99706507),
        vec2(-0.81409955, 0.91437590),
        vec2(0.19984126, 0.78641367),
        vec2(0.14383161, -0.14100790)
    );
    const int filterSize = 3;
    float shadowAmount(vec4 lightWorldPos, float intensity)
    {
        vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
        projectionCoords = projectionCoords * 0.5 + 0.5;

        if(projectionCoords.z > 1.0) return 1.0;

        float shadow = 0.0;
        vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
        for(int x = 0; x < filterSize; ++x)
        {
            for(int y = 0; y < filterSize; ++y)
            {
                float pcfDepth = TEXTURE(u_shadowMap, projectionCoords.xy + kernel[y * filterSize + x] * texelSize).r;
                shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
            }
        }
        return 1.0 - ((shadow / 9.0) *  clamp(intensity, 0.0, 1.0));
    }

    void main()
    {
        eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

        //reflection
        vec2 reflectCoords = v_reflectionPosition.xy / v_reflectionPosition.w / 2.0 + 0.5;

        reflectCoords.x += sin(u_time + (gl_FragCoord.z * 225.0)) * 0.001;

        vec4 reflectColour = TEXTURE(u_reflectionMap, reflectCoords);

        vec3 normal = normalize(v_normal);

        float fresnel = dot(reflect(-eyeDirection, normal), normal);
        const float bias = 0.6;
        fresnel = bias + (fresnel * (1.0 - bias));

        vec3 blendedColour = mix(reflectColour.rgb, WaterColour.rgb, fresnel);
   
        
        FRAG_OUT = vec4(blendedColour, 1.0);
        //FRAG_OUT.rgb *= shadowAmount(v_lightWorldPosition, dot(lightDir, normal));
    })";
