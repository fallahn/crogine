/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

static const std::string TerrainVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent; //dest position
    ATTRIBUTE vec3 a_bitangent; //dest normal

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewMatrix;
    uniform mat4 u_viewProjectionMatrix;

#if defined(RX_SHADOWS)
#if !defined(MAX_CASCADES)
#define MAX_CASCADES 3
#endif
        uniform mat4 u_lightViewProjectionMatrix[MAX_CASCADES];
        uniform int u_cascadeCount = 1;
#endif


    uniform vec4 u_clipPlane;
    uniform float u_morphTime;
    uniform vec3 u_cameraWorldPosition;

#include RESOLUTION_BUFFER

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec3 v_cameraWorldPosition;

#if defined(RX_SHADOWS)
    VARYING_OUT LOW vec4 v_lightWorldPosition[MAX_CASCADES];
    VARYING_OUT float v_viewDepth;
#endif

    vec3 lerp(vec3 a, vec3 b, float t)
    {
        return a + ((b - a) * t);
    }

    void main()
    {
        v_cameraWorldPosition = u_cameraWorldPosition;

        vec4 position = u_worldMatrix * vec4(lerp(a_position.xyz, a_tangent, u_morphTime), 1.0);
        //gl_Position = u_viewProjectionMatrix * position;

        vec4 vertPos = u_viewProjectionMatrix * position;
    #if defined (WOBBLE)
        vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;
    #endif
        gl_Position = vertPos;

    #if defined (RX_SHADOWS)
        for(int i = 0; i < u_cascadeCount; i++)
        {
            v_lightWorldPosition[i] = u_lightViewProjectionMatrix[i] * u_worldMatrix * position;
        }
        v_viewDepth = (u_viewMatrix * position).z;
    #endif

        //this should be a slerp really but lerp is good enough for low spec shenanigans
        v_normal = u_normalMatrix * lerp(a_normal, a_bitangent, u_morphTime);
        v_colour = a_colour;
        v_worldPosition = position.xyz;

        gl_ClipDistance[0] = dot(position, u_clipPlane);
    })";

static const std::string SlopeVertexShader =
R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform vec3 u_centrePosition;
    uniform vec3 u_cameraWorldPosition;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

    const float Radius = 5.0;

    void main()
    {
        vec4 worldPos = u_worldMatrix * a_position;
        float alpha = 1.0 - smoothstep(Radius, Radius + 5.0, length(worldPos.xyz - u_centrePosition));
        alpha *= (1.0 - smoothstep(Radius, Radius + 1.0, length(worldPos.xyz - u_cameraWorldPosition)));

        gl_Position = u_viewProjectionMatrix * worldPos;

        v_colour = a_colour;
        v_colour.a *= alpha;

        v_texCoord = a_texCoord0;
    }   
)";

//the UV coords actually indicate direction of slope
static const std::string SlopeFragmentShader =
R"(
    OUTPUT

    //dirX, strength, dirZ, elapsedTime
    #include WIND_BUFFER

    uniform float u_alpha;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    const vec3 DotColour = vec3(1.0, 0.9, 0.0);

    void main()
    {
        float alpha = (sin(v_texCoord.x - ((u_windData.w * 10.f) * v_texCoord.y)) + 1.0) * 0.5;
        alpha = step(0.1, alpha);

        vec4 colour = v_colour;
        colour.a *= u_alpha;
        colour = mix(vec4(DotColour, smoothstep(0.05, 0.4, v_colour.a * u_alpha)), colour, alpha);

        FRAG_OUT = colour;
        //FRAG_OUT.a *= /*alpha **/ u_alpha;
    }
)";

const std::string NormalMapVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec3 a_normal;

    uniform mat4 u_projectionMatrix;
    uniform float u_maxHeight;
    uniform float u_lowestPoint;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT float v_height;

    void main()
    {
        gl_Position = u_projectionMatrix * a_position;
        v_normal = a_normal;

        //yeah should be a normal matrix, but YOLO
        float z = v_normal.y;
        v_normal.y = -v_normal.z;
        v_normal.z = z;

        v_height = clamp((a_position.y - u_lowestPoint) / u_maxHeight, 0.0, 1.0);
    }
)";

const std::string NormalMapFragmentShader = R"(
    OUTPUT

    VARYING_IN vec3 v_normal;
    VARYING_IN float v_height;

    void main()
    {
        vec3 normal = normalize(v_normal);
        normal += 1.0;
        normal /= 2.0;

        FRAG_OUT = vec4(normal, v_height);
    }
)";