/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
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

namespace cro
{
    namespace Shaders
    {
        namespace ShadowMap
        {
            const static std::string Vertex = R"(
                ATTRIBUTE vec4 a_position;

                #if defined(SKINNED)
                ATTRIBUTE vec4 a_boneIndices;
                ATTRIBUTE vec4 a_boneWeights;
                uniform mat4 u_boneMatrices[MAX_BONES];
                #endif

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;
                uniform mat4 u_projectionMatrix;
                uniform vec4 u_clipPlane;

                VARYING_OUT vec4 v_position;

                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    vec4 position = a_position;

                #if defined (SKINNED)
                    mat4 skinMatrix = u_boneMatrices[int(a_boneIndices.x)] * a_boneWeights.x;
                    skinMatrix += u_boneMatrices[int(a_boneIndices.y)] * a_boneWeights.y;
                    skinMatrix += u_boneMatrices[int(a_boneIndices.z)] * a_boneWeights.z;
                    skinMatrix += u_boneMatrices[int(a_boneIndices.w)] * a_boneWeights.w;
                    position = skinMatrix * position;
                #endif                    

                    gl_Position = wvp * position;
                    v_position = gl_Position;

                #if defined (MOBILE)

                #else
                gl_ClipDistance[0] = dot(u_worldMatrix * position, u_clipPlane);
                #endif

                })";

            const static std::string FragmentMobile = R"(
                //ideally we want highp but not all mobile hardware supports it in the frag shader :(
                #if defined(MOBILE)
                #if defined (GL_FRAGMENT_PRECISION_HIGH)
                #define PREC highp
                #else
                #define PREC mediump
                #endif
                #else
                #define PREC
                #endif

                VARYING_IN vec4 v_position;
                OUTPUT

                PREC vec4 pack(const float depth)
                {
	                const PREC vec4 bitshift = vec4(16777216.0, 65536.0, 256.0, 1.0);
	                const PREC vec4 bitmask = vec4(0.0, 0.00390625, 0.00390625, 0.00390625);
	                PREC vec4 result = fract(depth * bitshift);
	                result -= result.xxyz * bitmask;
	                return result;
                }

                void main()
                {
                    PREC float distanceNorm = v_position.z /v_position.w;
                    distanceNorm = (distanceNorm + 1.0) / 2.0;
                    FRAG_OUT = pack(distanceNorm);
                })";

            const static std::string FragmentDesktop = R"()";
        }
    }
}