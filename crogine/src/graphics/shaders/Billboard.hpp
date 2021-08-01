/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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
    static const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position; //relative to root position (below)
        ATTRIBUTE vec3 a_normal; //actually contains root position of billboard
        ATTRIBUTE vec4 a_colour;

        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE MED vec2 a_texCoord1; //contains the size of the billboard to which this vertex belongs

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewMatrix;
        uniform mat4 u_viewProjectionMatrix;

        uniform vec4 u_clipPlane;
        uniform vec3 u_cameraWorldPosition;

        #if defined (LOCK_SCALE)
        uniform vec2 u_screenSize;
        #endif

        #if defined(RX_SHADOWS)
        uniform mat4 u_lightViewProjectionMatrix;

        VARYING_OUT LOW vec4 v_lightWorldPosition;
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

        void main()
        {
            vec3 position = (u_worldMatrix * vec4(a_normal, 1.0)).xyz;

#if defined (LOCK_SCALE)

            gl_Position = u_viewProjectionMatrix * vec4(position, 1.0);
            gl_Position /= gl_Position.w;
            gl_Position.xy += a_position.xy * (a_texCoord1 / u_screenSize);

            #if defined (VERTEX_LIT)
            v_normalVector = vec3(u_viewMatrix[0][2], u_viewMatrix[1][2], u_viewMatrix[2][2]);
            v_worldPosition = position;
            #endif
#else
            //TODO setting these as uniforms is more efficient, but also more faff.
            vec3 camRight = vec3(u_viewMatrix[0][0], u_viewMatrix[1][0], u_viewMatrix[2][0]);
#if defined(LOCK_ROTATION)
            vec3 camUp = vec3(0.0, 1.0, 0.0);
#else
            vec3 camUp = vec3(u_viewMatrix[0][1], u_viewMatrix[1][1], u_viewMatrix[2][1]);
#endif
            position = position + camRight * a_position.x
                                + camUp * a_position.y;

            gl_Position = u_viewProjectionMatrix * vec4(position, 1.0);

            #if defined (VERTEX_LIT)
            v_normalVector = normalize(cross(camRight, camUp));
            v_worldPosition = position;
            #endif
#endif




//TODO: defs for scaled billboards

            #if defined (RX_SHADOWS)
                v_lightWorldPosition = u_lightViewProjectionMatrix * vec4(position, 1.0);
            #endif

            #if defined (VERTEX_COLOUR)
                v_colour = a_colour;
                const float nearFadeDistance = 12.0; //TODO make this a uniform
                const float farFadeDistance = 150.f;
                float distance = length(position - u_cameraWorldPosition);

                v_colour.a = pow(clamp(distance / nearFadeDistance, 0.0, 1.0), 5.0);
                v_colour.a *= 1.0 - clamp((distance - farFadeDistance) / nearFadeDistance, 0.0, 1.0);

            #endif
            #if defined (TEXTURED)
                v_texCoord0 = a_texCoord0;
            #endif

            #if defined (MOBILE)

            #else
                gl_ClipDistance[0] = dot(u_worldMatrix * vec4(position, 1.0), u_clipPlane);
            #endif
        })";

    //not actually used, rather the VertxLit/Unlit fragment shaders are
    static const std::string Fragment = R"(

        #if defined (VERTEX_COLOUR)
        VARYING_IN LOW vec4 v_colour;
        #endif

        OUTPUT

        void main()
        {
            FRAG_OUT = v_colour;
        })";
}