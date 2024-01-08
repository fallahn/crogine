/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2023
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

namespace cro::Shaders::Unlit
{
    inline const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;
    #if defined(VERTEX_COLOUR)
        ATTRIBUTE vec4 a_colour;
    #endif
        ATTRIBUTE vec3 a_normal;

    #if defined(TEXTURED)
        ATTRIBUTE MED vec2 a_texCoord0;
    #endif
    #if defined(LIGHTMAPPED)
        ATTRIBUTE MED vec2 a_texCoord1;
    #endif

    #if defined(INSTANCING)
#include INSTANCE_ATTRIBS
    #endif


    #if defined(SKINNED)
#include SKIN_UNIFORMS
    #endif

    #if defined(PROJECTIONS)
    #define MAX_PROJECTIONS 4
        uniform mat4 u_projectionMapMatrix[MAX_PROJECTIONS]; //VP matrices for texture projection
        uniform LOW int u_projectionMapCount; //how many to actually draw
    #endif

#include WVP_UNIFORMS

        uniform vec4 u_clipPlane;

    #if defined(RX_SHADOWS)
#include SHADOWMAP_UNIFORMS_VERT
    #endif

    #if defined (SUBRECTS)
        uniform MED vec4 u_subrect;
    #endif
                
    #if defined (RIMMING)
        VARYING_OUT vec3 v_worldPosition;
        VARYING_OUT vec3 v_normalVector;
    #endif

    #if defined (VERTEX_COLOUR)
        VARYING_OUT LOW vec4 v_colour;
    #endif

    #if defined (TEXTURED)
        VARYING_OUT MED vec2 v_texCoord0;
    #endif

    #if defined (LIGHTMAPPED)
        VARYING_OUT MED vec2 v_texCoord1;
    #endif

    #if defined(PROJECTIONS)
        VARYING_OUT LOW vec4 v_projectionCoords[MAX_PROJECTIONS];
    #endif

    #if defined(RX_SHADOWS)
#include SHADOWMAP_OUTPUTS
    #endif

        void main()
        {
        #if defined (INSTANCING)
#include INSTANCE_MATRICES
        #else
            mat4 worldMatrix = u_worldMatrix;
            mat4 worldViewMatrix = u_worldViewMatrix;
            mat3 normalMatrix = u_normalMatrix;
        #endif

            mat4 wvp = u_projectionMatrix * worldViewMatrix;
            vec4 position = a_position;

        #if defined(PROJECTIONS)
            for(int i = 0; i < u_projectionMapCount; ++i)
            {
                v_projectionCoords[i] = u_projectionMapMatrix[i] * worldMatrix * a_position;
            }
        #endif

        #if defined (RIMMING)
        vec3 normal = a_normal;
        #endif

        #if defined(SKINNED)
#include SKIN_MATRIX
            position = skinMatrix * position;
        #if defined (RIMMING)
            normal = (skinMatrix * vec4(normal, 0.0)).xyz;
        #endif
        #endif

            gl_Position = wvp * position;

        #if defined (RX_SHADOWS)
#include SHADOWMAP_VERTEX_PROC
        #endif

        #if defined (RIMMING)
            v_normalVector = normalMatrix * normal;
            v_worldPosition = (worldMatrix * position).xyz;
        #endif

        #if defined (VERTEX_COLOUR)
            v_colour = a_colour;
        #endif
        #if defined (TEXTURED)
        #if defined (SUBRECTS)
            v_texCoord0 = u_subrect.xy + (a_texCoord0 * u_subrect.zw);
        #else
            v_texCoord0 = a_texCoord0;                    
        #endif
        #endif
        #if defined (LIGHTMAPPED)
            v_texCoord1 = a_texCoord1;
        #endif

        #if defined (MOBILE)

        #else
            gl_ClipDistance[0] = dot(worldMatrix * position, u_clipPlane);
        #endif
        })";

    inline const std::string Fragment = R"(
        OUTPUT
    #if defined (TEXTURED)
        uniform sampler2D u_diffuseMap;
    #if defined(ALPHA_CLIP)
        uniform float u_alphaClip;
    #endif
    #endif
    #if defined (LIGHTMAPPED)
        uniform sampler2D u_lightMap;
    #endif
    #if defined(COLOURED)
        uniform LOW vec4 u_colour;
    #endif
    #if defined(PROJECTIONS)
    #define MAX_PROJECTIONS 4
        uniform sampler2D u_projectionMap;
        uniform LOW int u_projectionMapCount;
    #endif

    #if defined (RX_SHADOWS)
    #if defined (MOBILE)
        uniform sampler2D u_shadowMap;
        const int u_cascadeCount = 1;
    #else
#include SHADOWMAP_UNIFORMS_FRAG
    #endif
    #endif

    #if defined(RIMMING)
        uniform LOW vec4 u_rimColour;
        uniform LOW float u_rimFalloff;
        uniform HIGH vec3 u_cameraWorldPosition;
    #endif

    #if defined (VERTEX_COLOUR)
        VARYING_IN LOW vec4 v_colour;
    #endif
    #if defined (TEXTURED)
        VARYING_IN MED vec2 v_texCoord0;
    #endif
    #if defined (LIGHTMAPPED)
        VARYING_IN MED vec2 v_texCoord1;
    #endif
    #if defined(RIMMING)
        VARYING_IN HIGH vec3 v_normalVector;
        VARYING_IN HIGH vec3 v_worldPosition;
    #endif

    #if defined(PROJECTIONS)
        VARYING_IN LOW vec4 v_projectionCoords[MAX_PROJECTIONS];
    #endif

    #if defined(RX_SHADOWS)
#include SHADOWMAP_INPUTS


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
    #include PCF_SHADOWS
    #endif

    #endif

        void main()
        {
        #if defined (VERTEX_COLOUR)
            FRAG_OUT = v_colour;
        #else
            FRAG_OUT = vec4(1.0);
        #endif
        #if defined (TEXTURED)
            FRAG_OUT *= TEXTURE(u_diffuseMap, v_texCoord0);
        #if defined(ALPHA_CLIP)
            if(FRAG_OUT.a < u_alphaClip) discard;
        #endif
        #endif
        #if defined (LIGHTMAPPED)
            FRAG_OUT.rgb *= TEXTURE(u_lightMap, v_texCoord1).rgb;
        #endif

        #if defined(COLOURED)
            FRAG_OUT *= u_colour;
        #endif
        #if defined(PROJECTIONS)
            for(int i = 0; i < u_projectionMapCount; ++i)
            {
                if(v_projectionCoords[i].w > 0.0)
                {
                    vec2 coords = v_projectionCoords[i].xy / v_projectionCoords[i].w / 2.0 + 0.5;
                    FRAG_OUT *= TEXTURE(u_projectionMap, coords);
                }
            }
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

        #if defined (RIMMING)
            MED vec3 normal = normalize(v_normalVector);
            MED vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

            LOW float rim = 1.0 - dot(normal, eyeDirection);
            rim = smoothstep(u_rimFalloff, 1.0, rim);
            FRAG_OUT.rgb += u_rimColour.rgb * rim ;//* 0.5;
        #endif

        })";
}