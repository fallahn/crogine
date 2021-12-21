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
    VARYING_OUT vec4 v_refractionPosition;
    VARYING_OUT LOW vec4 v_lightWorldPosition;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * position;

        v_normal = u_normalMatrix * a_normal;

        v_worldPosition = position.xyz;
        v_reflectionPosition = u_reflectionMatrix * position;
        v_refractionPosition = gl_Position;
        v_lightWorldPosition = u_lightViewProjectionMatrix * position;

    })";

static const std::string WaterFragment = R"(
    OUTPUT

    uniform sampler2D u_reflectionMap;
    uniform sampler2D u_refractionMap;

    uniform vec3 u_cameraWorldPosition;
    uniform float u_time;

    VARYING_IN vec3 v_normal;

    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec4 v_reflectionPosition;
    VARYING_IN vec4 v_refractionPosition;


    const vec3 WaterColour = vec3(0.02, 0.078, 0.578);
    //const vec3 WaterColour = vec3(0.2, 0.278, 0.278);
    //const vec3 WaterColour = vec3(0.137, 0.267, 0.53);

//float rand(float n){return fract(sin(n) * 43758.5453123);}
//
//vec2 noise(vec2 pos)
//{
//    return vec2(rand(pos.x), rand(pos.y));
//}


    void main()
    {
        vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

        //reflection
        vec2 reflectCoords = v_reflectionPosition.xy / v_reflectionPosition.w / 2.0 + 0.5;

        reflectCoords.x += sin(u_time + (gl_FragCoord.z * 325.0)) * 0.0005;

        vec4 reflectColour = TEXTURE(u_reflectionMap, reflectCoords);

        vec3 normal = normalize(v_normal);

        float fresnel = dot(reflect(-eyeDirection, normal), normal);
        const float bias = 0.6;
        fresnel = bias + (fresnel * (1.0 - bias));

        vec3 blendedColour = mix(reflectColour.rgb, WaterColour.rgb, fresnel);
   
        /*vec2 sparkle = vec2(cos(u_time * 0.5), sin(u_time * 0.5));
        float sparkleAmount = dot(normalize(noise(gl_FragCoord.xy)), sparkle);
        sparkleAmount = step(0.999, sparkleAmount);
        blendedColour.rgb += sparkleAmount;*/

        //refraction
        /*vec2 refractCoords = v_refractionPosition.xy / v_refractionPosition.w / 2.0 + 0.5;
        vec4 refractColour = TEXTURE(u_refractionMap, refractCoords);// + (normal.rg * Distortion));
        blendedColour.rgb *= refractColour.rgb;*/

        FRAG_OUT = vec4(blendedColour, 1.0);
    })";
