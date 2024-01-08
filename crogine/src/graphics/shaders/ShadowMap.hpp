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

namespace cro::Shaders::ShadowMap
{
    inline const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;

    #if defined (ALPHA_CLIP)
        ATTRIBUTE vec2 a_texCoord0;
    #endif

    #if defined(INSTANCING)
#include INSTANCE_ATTRIBS
    #endif

    #if defined(SKINNED)
#include SKIN_UNIFORMS
    #endif

#include WVP_UNIFORMS

        uniform vec4 u_clipPlane;

    #if defined (MOBILE)
        VARYING_OUT vec4 v_position;
    #endif

    #if defined (ALPHA_CLIP)
        VARYING_OUT vec2 v_texCoord0;
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
            vec4 position = a_position;

        #if defined (SKINNED)
#include SKIN_MATRIX
            position = skinMatrix * position;
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

    inline const std::string FragmentMobile = R"(
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
            PREC float distanceNorm = v_position.z / v_position.w;
            distanceNorm = (distanceNorm + 1.0) / 2.0;
            FRAG_OUT = pack(distanceNorm);
        })";


    inline static std::string FragmentDesktop = R"(
        #if defined(ALPHA_CLIP)

        uniform sampler2D u_diffuseMap;
        uniform float u_alphaClip;

        in vec2 v_texCoord0;

        void main()
        {
            if(texture(u_diffuseMap, v_texCoord0).a < u_alphaClip) discard;
        }
        #else

        OUTPUT             
        void main()
        {
            FRAG_OUT = vec4(1.0);
        }
        #endif
        )";
}