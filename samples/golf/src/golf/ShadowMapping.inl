/*-----------------------------------------------------------------------

Matt Marchant 2022
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

//modified shadow mapping shader for wind animated models
//and models with VATs
static const std::string ShadowVertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE vec4 a_colour;

    #if defined (ALPHA_CLIP)
        ATTRIBUTE vec2 a_texCoord0;
    #endif
    #if defined (VATS)
        ATTRIBUTE vec2 a_texCoord1;
    #endif

    #if defined(INSTANCING)
        ATTRIBUTE mat4 a_instanceWorldMatrix;
    #endif

    #if defined(SKINNED)
        ATTRIBUTE vec4 a_boneIndices;
        ATTRIBUTE vec4 a_boneWeights;
        uniform mat4 u_boneMatrices[MAX_BONES];
    #endif

    #if defined(INSTANCING)
        uniform mat4 u_viewMatrix;        
    #else
        uniform mat4 u_worldViewMatrix;
    #endif
        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;
        uniform vec4 u_clipPlane;

    #if defined (VATS)
        #define MAX_INSTANCE 3
        uniform sampler2D u_vatsPosition;
        uniform float u_time;
    #endif

#if defined(WIND_WARP)
        layout (std140) uniform WindValues
        {
            vec4 u_windData; //dirX, strength, dirZ, elapsedTime
        };
#endif

    #if defined (MOBILE)
        VARYING_OUT vec4 v_position;
    #endif

    #if defined (ALPHA_CLIP)
        VARYING_OUT vec2 v_texCoord0;
    #endif

        vec3 decodeVector(sampler2D source, vec2 coord)
        {
            vec3 vec = TEXTURE(source, coord).rgb;
            vec *= 2.0;
            vec -= 1.0;

            return vec;
        }

        void main()
        {
        #if defined(INSTANCING)
            mat4 worldMatrix = u_worldMatrix * a_instanceWorldMatrix;
            mat4 worldViewMatrix = u_viewMatrix * worldMatrix;
        #else
            mat4 worldMatrix = u_worldMatrix;
            mat4 worldViewMatrix = u_worldViewMatrix;
        #endif

            mat4 wvp = u_projectionMatrix * worldViewMatrix;

        #if defined (VATS)
            vec2 texCoord = a_texCoord1;
            float scale = texCoord.y;
            float instanceOffset = mod(gl_InstanceID, MAX_INSTANCE) + 1.0;
            texCoord.y = mod(u_time + (0.5 / instanceOffset), 1.0);

            vec4 position = vec4(decodeVector(u_vatsPosition, texCoord) * scale, 1.0);
        #else
            vec4 position = a_position;
        #endif

        #if defined (SKINNED)
            mat4 skinMatrix = a_boneWeights.x * u_boneMatrices[int(a_boneIndices.x)];
            skinMatrix += a_boneWeights.y * u_boneMatrices[int(a_boneIndices.y)];
            skinMatrix += a_boneWeights.z * u_boneMatrices[int(a_boneIndices.z)];
            skinMatrix += a_boneWeights.w * u_boneMatrices[int(a_boneIndices.w)];
            position = skinMatrix * position;
        #endif                    

    #if defined(WIND_WARP)
            const float xFreq = 0.6;
            const float yFreq = 0.8;
            const float scale = 0.2;

            float strength = u_windData.y;
            float totalScale = scale * strength * a_colour.b;

            position.x += sin((u_windData.w * (xFreq)) + worldMatrix[3].x) * totalScale;
            position.z += sin((u_windData.w * (yFreq)) + worldMatrix[3].z) * totalScale;
            position.xz += (u_windData.xz * strength * 2.0) * totalScale;
    #endif


            gl_Position = wvp * position;

        #if defined (MOBILE)
            v_position = gl_Position;
        #else
            gl_ClipDistance[0] = dot(worldMatrix * position, u_clipPlane);
        #endif

        #if defined (ALPHA_CLIP)
            v_texCoord0 = a_texCoord0;
        #endif

        })";

const static std::string ShadowFragment = R"(
        #if defined(ALPHA_CLIP)

        uniform sampler2D u_diffuseMap;
        uniform float u_alphaClip;

        in vec2 v_texCoord0;

        void main()
        {
            if(texture(u_diffuseMap, v_texCoord0).a < 0.5) discard;
        }
        #else

        OUTPUT             
        void main()
        {
            FRAG_OUT = vec4(1.0);
        }
        #endif
        )";