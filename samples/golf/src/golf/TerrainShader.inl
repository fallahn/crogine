/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

static const std::string TerrainVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
    ATTRIBUTE vec3 a_tangent; //dest position
    ATTRIBUTE vec3 a_bitangent; //dest normal

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec4 u_clipPlane;
    uniform float u_morphTime;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;

    vec3 lerp(vec3 a, vec3 b, float t)
    {
        return a + ((b - a) * t);
    }

    void main()
    {
        vec4 position = u_worldMatrix * vec4(lerp(a_position.xyz, a_tangent, u_morphTime), 1.0);
        gl_Position = u_viewProjectionMatrix * position;

        //this should be a slerp really but lerp is good enough for low spec shenanigans
        v_normal = u_normalMatrix * lerp(a_normal, a_bitangent, u_morphTime);
        v_colour = a_colour;

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

    uniform float u_time;
    uniform float u_alpha;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    //const float Scale = 20.0;

    void main()
    {
        float alpha = (sin(v_texCoord.x - (u_time * v_texCoord.y)) + 1.0) * 0.5;
        alpha = step(0.5, alpha);

        FRAG_OUT = v_colour;
        FRAG_OUT.a *= alpha * u_alpha;
    }
)";

static const std::string CelVertexShader = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;
#if defined (TEXTURED)
    ATTRIBUTE vec2 a_texCoord0;
#endif

#if defined(SKINNED)
    #define MAX_BONES 64
    ATTRIBUTE vec4 a_boneIndices;
    ATTRIBUTE vec4 a_boneWeights;
    uniform mat4 u_boneMatrices[MAX_BONES];
#endif

#if defined(RX_SHADOWS)
    uniform mat4 u_lightViewProjectionMatrix;
#endif

    uniform mat3 u_normalMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec4 u_clipPlane;
    uniform vec3 u_cameraWorldPosition;

    VARYING_OUT float v_ditherAmount;
    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec4 v_colour;
#if defined (TEXTURED)
    VARYING_OUT vec2 v_texCoord;
#endif

#if defined(RX_SHADOWS)
    VARYING_OUT LOW vec4 v_lightWorldPosition;
#endif

#if defined (NORMAL_MAP)
    VARYING_OUT vec2 v_normalTexCoord;
    const vec2 MapSize = vec2(320.0, 200.0);
#endif

    void main()
    {
        vec4 position = a_position;

    #if defined(SKINNED)
        mat4 skinMatrix = a_boneWeights.x * u_boneMatrices[int(a_boneIndices.x)];
        skinMatrix += a_boneWeights.y * u_boneMatrices[int(a_boneIndices.y)];
        skinMatrix += a_boneWeights.z * u_boneMatrices[int(a_boneIndices.z)];
        skinMatrix += a_boneWeights.w * u_boneMatrices[int(a_boneIndices.w)];
        position = skinMatrix * position;
    #endif

    #if defined (RX_SHADOWS)
        v_lightWorldPosition = u_lightViewProjectionMatrix * u_worldMatrix * position;
    #endif

        position = u_worldMatrix * position;
        gl_Position = u_viewProjectionMatrix * position;

        vec3 normal = a_normal;
    #if defined(SKINNED)
        normal = (skinMatrix * vec4(normal, 0.0)).xyz;
    #endif
        v_normal = u_normalMatrix * normal;

        v_colour = a_colour;

#if defined (TEXTURED)
    v_texCoord = a_texCoord0;
#endif

#if defined (NORMAL_MAP)
    v_normalTexCoord = vec2(position.x / MapSize.x, -position.z / MapSize.y);
#endif
        gl_ClipDistance[0] = dot(position, u_clipPlane);

#if defined(DITHERED)
        const float fadeDistance = 4.0;
        const float nearFadeDistance = 2.0;
        const float farFadeDistance = 360.f;
        float distance = length(position.xyz - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((distance - nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / fadeDistance, 0.0, 1.0);
#endif
    })";

static const std::string CelFragmentShader = R"(
    uniform vec3 u_lightDirection;
    uniform float u_pixelScale = 1.0;

#if defined (RX_SHADOWS)
    uniform sampler2D u_shadowMap;
#endif

#if defined(TEXTURED)
    uniform sampler2D u_diffuseMap;
    VARYING_IN vec2 v_texCoord;
#endif

#if defined (NORMAL_MAP)
    uniform sampler2D u_normalMap;
    VARYING_IN vec2 v_normalTexCoord;
#endif

    VARYING_IN vec3 v_normal;
    VARYING_IN vec4 v_colour;
    VARYING_IN float v_ditherAmount;

    OUTPUT

#if defined(RX_SHADOWS)
    VARYING_IN LOW vec4 v_lightWorldPosition;

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
    float shadowAmount(vec4 lightWorldPos)
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

        float amount = shadow / 9.0;
        return 1.0 - amount;
    }
#endif


    //function based on example by martinsh.blogspot.com
    const int MatrixSize = 8;
    float findClosest(int x, int y, float c0)
    {
        /* 8x8 Bayer ordered dithering */
        /* pattern. Each input pixel */
        /* is scaled to the 0..63 range */
        /* before looking in this table */
        /* to determine the action. */

        const int dither[64] = int[64](
         0, 32, 8, 40, 2, 34, 10, 42, 
        48, 16, 56, 24, 50, 18, 58, 26, 
        12, 44, 4, 36, 14, 46, 6, 38, 
        60, 28, 52, 20, 62, 30, 54, 22, 
         3, 35, 11, 43, 1, 33, 9, 41, 
        51, 19, 59, 27, 49, 17, 57, 25,
        15, 47, 7, 39, 13, 45, 5, 37,
        63, 31, 55, 23, 61, 29, 53, 21 );

        float limit = 0.0;
        if (x < MatrixSize)
        {
            limit = (dither[y * MatrixSize + x] + 1) / 64.0;
        }

        if (c0 < limit)
        {
            return 0.0;
        }
        return 1.0;
    }


    const float Quantise = 10.0;

    void main()
    {
        vec4 colour = vec4(1.0);

#if defined (TEXTURED)
        vec4 c = TEXTURE(u_diffuseMap, v_texCoord);

        if(c. a < 0.2)
        {
            discard;
        }
        colour *= c;

        /*colour.rgb *= Quantise;
        colour.rgb = round(colour.rgb);
        colour.rgb /= Quantise;*/
#endif
#if defined (VERTEX_COLOURED)
        colour *= v_colour;
#endif

#if defined (NORMAL_MAP)
        //colour = TEXTURE(u_normalMap, v_normalTexCoord);
        //colour.rg = v_normalTexCoord;
        vec3 normal = TEXTURE(u_normalMap, v_normalTexCoord).rgb * 2.0 - 1.0;
#else
        vec3 normal = normalize(v_normal);
#endif

        float amount = dot(normal, normalize(-u_lightDirection));

        float checkAmount = step(0.3, 1.0 - amount);

        amount *= 2.0;
        amount = round(amount);
        amount /= 2.0;
        amount = 0.8 + (amount * 0.2);

        colour.rgb *= amount;

#if !defined(NOCHEX)        
        float check = mod(floor(gl_FragCoord.x / u_pixelScale) + floor(gl_FragCoord.y / u_pixelScale), 2.0) * checkAmount;
        //float check = mod(gl_FragCoord.x + gl_FragCoord.y, 2.0) * checkAmount;
        amount = (1.0 - check) + (check * amount);
        colour.rgb *= amount;
#endif

        FRAG_OUT = vec4(colour.rgb, 1.0);

#if defined (RX_SHADOWS)
        FRAG_OUT.rgb *= shadowAmount(v_lightWorldPosition);
        /*if(v_lightWorldPosition.w > 0.0)
        {
            vec2 coords = v_lightWorldPosition.xy / v_lightWorldPosition.w / 2.0 + 0.5;
            if(coords.x>0&&coords.x<1&&coords.y>0&&coords.y<1)
            FRAG_OUT.rgb += vec3(0.0,0.0,0.5);
        }*/
#endif

#if defined(DITHERED)
        vec2 xy = gl_FragCoord.xy / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));

        if(alpha < 0.1) discard;
#endif
    })";


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