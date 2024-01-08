/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2022
http://trederia.blogspot.com

crogine - Zlib license.

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

namespace cro::Shaders::Billboard
{
    inline const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position; //relative to root position (below)
        ATTRIBUTE vec3 a_normal; //actually contains root position of billboard
        ATTRIBUTE vec4 a_colour;

        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE MED vec2 a_texCoord1; //contains the size of the billboard to which this vertex belongs

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewMatrix;
        uniform mat4 u_viewProjectionMatrix;

    #if defined(SHADOW_MAPPING)
        uniform mat4 u_cameraViewMatrix;
        uniform mat4 u_projectionMatrix;
    #endif

        uniform vec4 u_clipPlane;
        uniform vec3 u_cameraWorldPosition;

        #if defined (LOCK_SCALE)
        uniform vec2 u_screenSize;
        #endif

        #if defined(RX_SHADOWS)
        #if !defined(MAX_CASCADES)
        #define MAX_CASCADES 4
        #endif
            uniform mat4 u_lightViewProjectionMatrix[MAX_CASCADES];
        #if defined (MOBILE)
            const int u_cascadeCount = 1;
        #else
            uniform int u_cascadeCount = 1;
        #endif
        #endif

        #if defined (VERTEX_COLOUR)
        VARYING_OUT LOW vec4 v_colour;
        #endif
        #if defined (TEXTURED)
        VARYING_OUT MED vec2 v_texCoord0;
        #endif
        #if defined (VERTEX_LIT)
        VARYING_OUT vec3 v_normalVector;
        VARYING_OUT vec3 v_worldPosition;
        #endif
        VARYING_OUT float v_ditherAmount;

        #if defined(RX_SHADOWS)
        VARYING_OUT LOW vec4 v_lightWorldPosition[MAX_CASCADES];
        VARYING_OUT float v_viewDepth;
        #endif

        void main()
        {
            vec3 position = (u_worldMatrix * vec4(a_normal, 1.0)).xyz;

#if defined (SHADOW_MAPPING)
            mat4 viewMatrix = u_cameraViewMatrix;
            mat4 viewProj = u_projectionMatrix * u_viewMatrix;
#else
            mat4 viewMatrix = u_viewMatrix;
            mat4 viewProj = u_viewProjectionMatrix;
#endif

#if defined (LOCK_SCALE)

            gl_Position = viewProj * vec4(position, 1.0);
            gl_Position /= gl_Position.w;
            gl_Position.xy = (a_position.xy * (a_texCoord1 / u_screenSize)) + gl_Position.xy;

            #if defined (VERTEX_LIT)
            v_normalVector = vec3(viewMatrix[0][2], viewMatrix[1][2], viewMatrix[2][2]);
            v_worldPosition = position;
            #endif
#else
            vec3 camRight = vec3(viewMatrix[0][0], viewMatrix[1][0], viewMatrix[2][0]);
#if defined(LOCK_ROTATION)
            vec3 camUp = vec3(0.0, 1.0, 0.0);
#else
            vec3 camUp = vec3(viewMatrix[0][1], viewMatrix[1][1], viewMatrix[2][1]) * -u_clipPlane.y;
#endif
            position = position + (camRight * a_position.x)
                                + (camUp * a_position.y);

            gl_Position = viewProj * vec4(position, 1.0);

#if defined (VERTEX_LIT)
            v_normalVector = normalize(cross(camRight, camUp));
            v_worldPosition = position;
#endif
#endif




//TODO: defs for scaled billboards

            #if defined (TEXTURED)
                v_texCoord0 = a_texCoord0;
            #endif

#if !defined(SHADOW_MAPPING)
        #if defined (RX_SHADOWS) //this should never be defined if SHADOW_MAPPING is
                for(int i = 0; i < u_cascadeCount; i++)
                {
                    v_lightWorldPosition[i] = u_lightViewProjectionMatrix[i] * worldMatrix * position;
                }
                v_viewDepth = (u_ViewMatrix * position).z;
        #endif

            #if defined (VERTEX_COLOUR)
                v_colour = a_colour;
            #endif

                const float minDistance = 2.0;
                const float nearFadeDistance = 12.0; //TODO make this a uniform
                const float farFadeDistance = 300.f;
                float distance = length(position - u_cameraWorldPosition);

                v_ditherAmount = pow(clamp((distance - minDistance) / nearFadeDistance, 0.0, 1.0), 5.0);
                v_ditherAmount *= 1.0 - clamp((distance - farFadeDistance) / nearFadeDistance, 0.0, 1.0);

                v_colour.rgb *= (((1.0 - pow(clamp(distance / farFadeDistance, 0.0, 1.0), 5.0)) * 0.8) + 0.2);
#endif
            #if defined (MOBILE)

            #else
                gl_ClipDistance[0] = dot(u_worldMatrix * vec4(position, 1.0), u_clipPlane);
            #endif
        })";

    /*
    Billboards generally don't lend themselves well to alpha blending without
    complicated depth sorting, so this shader uses bayer dithering on the alpha
    channel, which is then discarded based on the alpha clip value.
    */

    inline const std::string Fragment = R"(
        OUTPUT
        #if defined (TEXTURED)
        uniform sampler2D u_diffuseMap;
        #if defined(ALPHA_CLIP)
        uniform float u_alphaClip;
        #endif
        #endif
        #if defined(COLOURED)
        uniform LOW vec4 u_colour;
        #endif

        #if defined(MASK_MAP)
        uniform sampler2D u_maskMap;
        #else
        uniform LOW vec4 u_maskColour;
        #endif

        #if defined(VERTEX_LIT)
        uniform samplerCube u_skybox;

        uniform HIGH vec3 u_lightDirection;
        uniform LOW vec4 u_lightColour;
        uniform HIGH vec3 u_cameraWorldPosition;
        #endif
        #if defined (RX_SHADOWS)
        #if defined (MOBILE)
        uniform sampler2D u_shadowMap;
        const int u_cascadeCount = 1;
        #else
        #if !defined(MAX_CASCADES)
        #define MAX_CASCADES 4
        #endif
        uniform sampler2DArray u_shadowMap;
        uniform int u_cascadeCount = 1;
        uniform float u_frustumSplits[MAX_CASCADES];
        #endif
        #endif

        #if defined (VERTEX_COLOUR)
        VARYING_IN LOW vec4 v_colour;
        #endif
        #if defined (TEXTURED)
        VARYING_IN MED vec2 v_texCoord0;
        #endif

        VARYING_IN float v_ditherAmount;
        VARYING_IN vec3 v_normalVector;
        VARYING_IN HIGH vec3 v_worldPosition;

        #if defined(RX_SHADOWS)
        VARYING_IN LOW vec4 v_lightWorldPosition[MAX_CASCADES];
        VARYING_IN float v_viewDepth;

        #if defined(MOBILE)
        #if defined (GL_FRAGMENT_PRECISION_HIGH)
        #define PREC highp
        #else
        #define PREC mediump
        #endif
        #else
        #define PREC
        #endif

        PREC float unpack(PREC vec4 colour)
        {
            const PREC vec4 bitshift = vec4(1.0 / 16777216.0, 1.0 / 65536.0, 1.0 / 256.0, 1.0);
            return dot(colour, bitshift);
        }
                
        #if defined(MOBILE)
        PREC float shadowAmount(int)
        {
            vec4 lightWorldPos = v_lightWorldPosition[0];

            PREC vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
            projectionCoords = projectionCoords * 0.5 + 0.5;
            PREC float depthSample = unpack(TEXTURE(u_shadowMap, projectionCoords.xy));
            PREC float currDepth = projectionCoords.z - 0.005;
            return (currDepth < depthSample) ? 1.0 : 0.4;
        }
        #else
        int getCascadeIndex()
        {
            for(int i = 0; i < u_cascadeCount; ++i)
            {
                if (v_viewDepth >= u_frustumSplits[i])
                {
                    return min(u_cascadeCount - 1, i);
                }
            }
            return u_cascadeCount - 1;
        }

        //some fancier pcf on desktop
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
        float shadowAmount(int cascadeIndex)
        {
            vec4 lightWorldPosition = v_lightWorldPosition[cascadeIndex];

            vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
            projectionCoords = projectionCoords * 0.5 + 0.5;

            if(projectionCoords.z > 1.0) return 1.0;

            float shadow = 0.0;
            vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
            for(int x = 0; x < filterSize; ++x)
            {
                for(int y = 0; y < filterSize; ++y)
                {
                    float pcfDepth = TEXTURE(u_shadowMap, vec3((kernel[y * filterSize + x] * texelSize) + projectionCoords.xy, cascadeIndex)).r;
                    shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
                }
            }
            return 1.0 - (shadow / 9.0);
        }
        #endif
        #endif

        #if defined(VERTEX_LIT)
        LOW vec4 diffuseColour = vec4(1.0);
        HIGH vec3 eyeDirection;
        LOW vec4 mask = vec4(1.0, 1.0, 0.0, 1.0);
        vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
        {
            MED float diffuseAmount = max(dot(normal, lightDirection), 0.0);
            //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
            MED vec3 mixedColour = diffuseColour.rgb * lightDiffuse * diffuseAmount * falloff;

            MED vec3 halfVec = normalize(eyeDirection + lightDirection);
            MED float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
            LOW vec3 specularColour = lightSpecular * vec3(pow(specularAngle, ((254.0 * mask.r) + 1.0))) * falloff;

            return clamp(mixedColour + (specularColour * mask.g), 0.0, 1.0);
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


        void main()
        {
        //vertex lit calc
    #if defined (VERTEX_LIT)
        #if defined (DIFFUSE_MAP)
            diffuseColour *= TEXTURE(u_diffuseMap, v_texCoord0);
        #endif

        #if defined(MASK_MAP)
            mask = TEXTURE(u_maskMap, v_texCoord0);
        #else
            mask = u_maskColour;
        #endif

        #if defined(COLOURED)
            diffuseColour *= u_colour;
        #endif
                
        #if defined(VERTEX_COLOUR)
            diffuseColour *= v_colour;
        #endif

            LOW vec3 blendedColour = diffuseColour.rgb * 0.2; //ambience
            eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

            MED vec3 normal = normalize(v_normalVector);
            blendedColour += calcLighting(normal, normalize(-u_lightDirection), u_lightColour.rgb, vec3(1.0), 1.0);
        #if defined (RX_SHADOWS)
            blendedColour *= shadowAmount(v_lightWorldPosition);
        #endif

            FRAG_OUT.rgb = mix(blendedColour, diffuseColour.rgb, mask.b);
            FRAG_OUT.a = diffuseColour.a;

            vec3 I = normalize(v_worldPosition - u_cameraWorldPosition);
            vec3 R = reflect(I, normal);
            FRAG_OUT.rgb = mix(TEXTURE_CUBE(u_skybox, R).rgb, FRAG_OUT.rgb, mask.a);

    #else
        //unlit calc
        #if defined (VERTEX_COLOUR)
            FRAG_OUT = v_colour;
        #else
            FRAG_OUT = vec4(1.0);
        #endif
        #if defined (TEXTURED)
            FRAG_OUT *= TEXTURE(u_diffuseMap, v_texCoord0);
        #endif

        #if defined(COLOURED)
            FRAG_OUT *= u_colour;
        #endif
    #endif


        #if defined (RX_SHADOWS)

            int cascadeIndex = getCascadeIndex();
            float shadow = shadowAmount(cascadeIndex);
            float fade = smoothstep(u_frustumSplits[cascadeIndex] + 0.5, u_frustumSplits[cascadeIndex],  v_viewDepth);
            if(fade > 0)
            {
                int nextIndex = min(cascadeIndex + 1, u_cascadeCount - 1);
                shadow = mix(shadow, shadowAmount(nextIndex), fade);
            }

            FRAG_OUT.rgb *= shadow;


//vec3 Colours[4] = vec3[4](vec3(0.2,0.0,0.0), vec3(0.0,0.2,0.0),vec3(0.0,0.0,0.2),vec3(0.2,0.0,0.2));
//for(int i = 0; i < u_cascadeCount; ++i)
//{
//    if (v_lightWorldPosition[i].w > 0.0)
//    {
//        vec2 coords = v_lightWorldPosition[i].xy / v_lightWorldPosition[i].w / 2.0 + 0.5;
//        if (coords.x > 0 && coords.x < 1 
//                && coords.y > 0 && coords.y < 1)
//        {
//            FRAG_OUT.rgb += Colours[i];
//        }
//    }
//}
        #endif

        vec2 xy = gl_FragCoord.xy;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, v_ditherAmount);
        FRAG_OUT.a *= alpha;

        #if defined(ALPHA_CLIP)
            if(FRAG_OUT.a < u_alphaClip) discard;
        #endif
        })";
}