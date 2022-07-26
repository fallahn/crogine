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
        ATTRIBUTE vec3 a_normal;

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
        #define MAX_BONES 64
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
        uniform mat3 u_normalMatrix;
        uniform vec3 u_cameraWorldPosition;
        uniform vec4 u_clipPlane;

    #if defined (VATS)
        #define MAX_INSTANCE 3
        uniform sampler2D u_vatsPosition;
        uniform float u_time;
        uniform float u_maxTime;
        uniform float u_offsetMultiplier;
    #endif

#if defined(WIND_WARP) || defined(TREE_WARP) || defined(LEAF_SIZE)
        layout (std140) uniform WindValues
        {
            vec4 u_windData; //dirX, strength, dirZ, elapsedTime
        };
        const float MaxWindOffset = 0.2;
        uniform float u_leafSize = 0.25;
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
            float instanceOffset = mod(gl_InstanceID, MAX_INSTANCE) * u_offsetMultiplier;
            texCoord.y = mod(u_time + (0.15 * instanceOffset), u_maxTime);

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
    #elif defined(TREE_WARP)
        
            float time = (u_windData.w * 15.0) + gl_InstanceID;
            float x = sin(time * 2.0) / 8.0;
            float y = cos(time) / 2.0;
            vec3 windOffset = vec3(x, y, x) * a_colour.b * 0.1;

            vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
            float dirStrength = a_colour.b;

            windOffset += windDir * u_windData.y * dirStrength;
            vec4 worldPosition = worldMatrix * position;
            worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;

            //this only works if shadows are instanced...
            gl_Position = u_projectionMatrix * u_viewMatrix * worldPosition;
            gl_PointSize = 40.0;
    #endif

    #if defined(LEAF_SIZE)

        vec4 worldPosition = worldMatrix * position;
        vec3 normal = u_normalMatrix * a_normal;

        float time = (u_windData.w * 5.0) + gl_InstanceID + gl_VertexID;
        float x = sin(time * 2.0) / 8.0;
        float y = cos(time) / 2.0;
        vec3 windOffset = vec3(x, y, x);

        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = dot(normal, windDir);
        dirStrength += 1.0;
        dirStrength /= 2.0;

        windOffset += windDir * u_windData.y * dirStrength * 2.0;
        worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;

        gl_Position = u_projectionMatrix * u_viewMatrix * worldPosition;



        float pointSize = u_leafSize;
        vec3 camForward = vec3(u_viewMatrix[0][2], u_viewMatrix[1][2], u_viewMatrix[2][2]);
        vec3 eyeDir = normalize(u_cameraWorldPosition - worldPosition.xyz);

        float facingAmount = dot(normal, camForward);
        pointSize *= 0.8 + (0.2 * facingAmount);
            
        //shrink 'backfacing' to zero
        pointSize *= smoothstep(-0.2, 0.0, facingAmount);
            
        //we use the camera's forward vector to shrink any points out of view to zero
        pointSize *= step(0.0, clamp(dot(eyeDir, (camForward)), 0.0, 1.0));

            
        //shrink with perspective/distance and scale to world units
        pointSize *= 2048.0 * (u_projectionMatrix[1][1] / gl_Position.w);

        //we scale point size by model matrix but it assumes all axis are
        //scaled equally ,as we only use the X axis
        pointSize *= length(worldMatrix[0].xyz);

        gl_PointSize = pointSize;

    #endif


    #if !defined(TREE_WARP) && !defined(LEAF_SIZE)
            gl_Position = wvp * position;
    #endif

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