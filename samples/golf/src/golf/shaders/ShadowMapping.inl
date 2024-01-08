/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

//modified shadow mapping shader for wind animated models
//and models with VATs
inline const std::string ShadowVertex = R"(
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
#include INSTANCE_ATTRIBS
    #endif

    #if defined(SKINNED)
        #define MAX_BONES 64
#include SKIN_UNIFORMS
    #endif

        uniform mat4 u_viewMatrix;
    #if !defined(INSTANCING)
        uniform mat4 u_worldViewMatrix;
    #endif
        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;
        uniform mat3 u_normalMatrix;
        uniform vec3 u_cameraWorldPosition;
        uniform vec4 u_clipPlane;

    #if defined (VATS)
        #define MAX_INSTANCE 3
    #if defined (ARRAY_MAPPING)
        uniform sampler2DArray u_arrayMap;
    #else
        uniform sampler2D u_vatsPosition;
    #endif
        uniform float u_time;
        uniform float u_maxTime;
        uniform float u_offsetMultiplier;
    #endif

    #if defined(WIND_WARP) || defined(TREE_WARP) || defined(LEAF_SIZE)
    #include WIND_BUFFER

        const float MaxWindOffset = 0.2;

        uniform float u_leafSize = 0.25;
        uniform sampler2D u_noiseTexture;
    #endif

    #if defined (MOBILE)
        VARYING_OUT vec4 v_position;
    #endif

#if defined (DITHERED)
    #include RESOLUTION_BUFFER
        VARYING_OUT float v_ditherAmount;
#endif

    #if defined (ALPHA_CLIP)
        VARYING_OUT vec2 v_texCoord0;
    #endif

#include VAT_VEC

    #if defined(WIND_WARP) || defined (TREE_WARP) || defined(LEAF_SIZE)
    #include WIND_CALC
    #endif


        void main()
        {
        #if defined(INSTANCING)
#include INSTANCE_MATRICES
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

        #if defined (ARRAY_MAPPING)
            vec4 position = vec4(decodeVector(u_arrayMap, vec3(texCoord, 1.0)) * scale, 1.0);
        #else
            vec4 position = vec4(decodeVector(u_vatsPosition, texCoord) * scale, 1.0);
        #endif
        #else
            vec4 position = a_position;
        #endif

        #if defined (SKINNED)
#include SKIN_MATRIX
            position = skinMatrix * position;
        #endif                    

    #if defined(WIND_WARP)

            //red low freq, green high freq, blue direction amount
            //worldMatrix[3].xz position for all verts.
            WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);
            vec3 vertexStrength = a_colour.rgb;
            //multiply high and low frequency by vertex colours
            windResult.lowFreq *= vertexStrength.r;
            windResult.highFreq *= vertexStrength.g;

            //apply high frequency and low frequency in local space
            position.x += windResult.lowFreq.x + windResult.highFreq.x;
            position.z += windResult.lowFreq.y + windResult.highFreq.y;

            //multiply wind direction by wind strength
            vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * vertexStrength.b;
            //wind dir is added in world space (below)
    #elif defined(TREE_WARP)

            WindResult windResult = getWindData(position.xz, worldMatrix[3].xz);

            vec3 vertexStrength = a_colour.rgb;
            //multiply high and low frequency by vertex colours
            //note that in tree models red/green ARE SWAPPED >.<
            windResult.highFreq *= vertexStrength.r;
            windResult.lowFreq *= vertexStrength.g;

            //apply high frequency and low frequency in local space
            position.x += windResult.lowFreq.x + windResult.highFreq.x;
            position.z += windResult.lowFreq.y + windResult.highFreq.y;

            //multiply wind direction by wind strength
            vec3 windDir = vec3(u_windData.x, 0.0, u_windData.z) * windResult.strength * vertexStrength.b;
            //wind dir is added in world space (below)

            vec4 worldPosition = worldMatrix * position;
            worldPosition.xyz += windDir;

            gl_Position = u_projectionMatrix * u_viewMatrix * worldPosition;
            gl_PointSize = 40.0;
    #endif

    #if defined(LEAF_SIZE)

        vec4 worldPosition = worldMatrix * position;
        vec3 normal = u_normalMatrix * a_normal;

WindResult windResult = getWindData(position.xz, worldPosition.xz);
windResult.lowFreq *= 0.5 + (0.5 * u_windData.y);
windResult.highFreq *= 0.5 + (0.5 * u_windData.y);

float x = windResult.highFreq.x;
float y = windResult.lowFreq.y;
vec3 windOffset = vec3(x, y, windResult.highFreq.y) * 5.0;

        /*float time = (u_windData.w * 5.0) + gl_InstanceID + gl_VertexID;
        float x = sin(time * 2.0) / 8.0;
        float y = cos(time) / 2.0;
        vec3 windOffset = vec3(x, y, x);*/

        vec3 windDir = normalize(vec3(u_windData.x, 0.f, u_windData.z));
        float dirStrength = dot(normal, windDir);
        dirStrength += 1.0;
        dirStrength /= 2.0;

        windOffset += windDir * u_windData.y * dirStrength * 2.0;
        worldPosition.xyz += windOffset * MaxWindOffset * u_windData.y;

worldPosition.x += windResult.lowFreq.x;
worldPosition.z += windResult.lowFreq.y;

        gl_Position = u_projectionMatrix * u_viewMatrix * worldPosition;



        float pointSize = u_leafSize;
        vec3 camForward = vec3(u_viewMatrix[0][2], u_viewMatrix[1][2], u_viewMatrix[2][2]);
        vec3 eyeDir = u_cameraWorldPosition - worldPosition.xyz;

        float distance = length(eyeDir);
        eyeDir /= distance;

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

        //use the geom shader to cull leaves with point size of 0
        pointSize *= 1.0 - smoothstep(0.5, 0.9, distance / 40.f);

        gl_PointSize = pointSize;

    #endif


    #if !defined(TREE_WARP) && !defined(LEAF_SIZE)

    #if defined(WIND_WARP)
            position = worldMatrix * position;
            position.xyz += windDir;
            gl_Position = u_projectionMatrix * u_viewMatrix * position;
    #else
            gl_Position = wvp * position;
    #endif
    #endif

        #if defined (MOBILE)
            v_position = gl_Position;
        #else
            gl_ClipDistance[0] = dot(worldMatrix * position, u_clipPlane);
        #endif

        #if defined (ALPHA_CLIP)
            v_texCoord0 = a_texCoord0;
        #endif

#if defined(DITHERED)
        float fadeDistance = u_nearFadeDistance * 2.0;
        const float farFadeDistance = 360.f;

        vec4 p = worldMatrix * a_position;
        float d = length(p.xyz - u_cameraWorldPosition);

        v_ditherAmount = pow(clamp((d - u_nearFadeDistance) / fadeDistance, 0.0, 1.0), 2.0);
        v_ditherAmount *= 1.0 - clamp((d - farFadeDistance) / fadeDistance, 0.0, 1.0);
#endif


        })";

inline const std::string ShadowGeom = R"(

#if defined (POINTS)
    layout (points) in;
    layout (points, max_vertices = 1) out;
#define ARRAY_SIZE 1
#else
    layout (triangles) in;
    layout (triangles, max_vertices = 3) out;
#define ARRAY_SIZE 3
#endif

    void main()
    {
#if defined (POINTS)
        gl_Position = gl_in[0].gl_Position;
        gl_PointSize = gl_in[0].gl_PointSize;
        gl_ClipDistance[0] = gl_in[0].gl_ClipDistance[0];
        
        if(gl_PointSize > 0.05)
        {
            EmitVertex();
        }

#else
        for(int i = 0; i < gl_in.length(); ++i)
        {
            gl_Position = gl_in[i].gl_Position;
            gl_PointSize = gl_in[i].gl_PointSize;
            gl_ClipDistance[0] = gl_in[i].gl_ClipDistance[0];
        
            if(gl_PointSize > 0.05)
            {
                EmitVertex();
            }
        }
#endif
    })";

inline const std::string ShadowFragment = R"(
        
#if defined (DITHERED)
        in float v_ditherAmount;

    #include BAYER_MATRIX
#endif

    #if defined(ALPHA_CLIP)

        uniform sampler2D u_diffuseMap;
        uniform float u_alphaClip;

        in vec2 v_texCoord0;

        void main()
        {

#if defined(DITHERED)
        vec2 xy = gl_FragCoord.xy;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
        if(alpha < 0.5) discard;
#endif


#if defined(LEAF_SIZE)
        vec2 coord = gl_PointCoord;
#else
        vec2 coord = v_texCoord0;
#endif
            if(texture(u_diffuseMap, coord).a < 0.5) discard;
        }
        #else

        OUTPUT             
        void main()
        {

#if defined(DITHERED)
        vec2 xy = gl_FragCoord.xy;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));

        float alpha = findClosest(x, y, smoothstep(0.1, 0.95, v_ditherAmount));
        if(alpha < 0.5) discard;
#endif

            FRAG_OUT = vec4(1.0);
        }
        #endif
        )";