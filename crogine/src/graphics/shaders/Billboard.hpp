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

namespace cro::Shaders::Billboard
{
    const static std::string Vertex = R"(
        ATTRIBUTE vec4 a_position; //relative to root position (below)
        ATTRIBUTE vec3 a_normal; //actually contains root position of billboard
        #if defined(VERTEX_COLOUR)
        ATTRIBUTE vec4 a_colour;
        #endif

        #if defined(TEXTURED)
        ATTRIBUTE MED vec2 a_texCoord0;
        #endif

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewMatrix;
        uniform mat4 u_viewProjectionMatrix;

        #if defined (VERTEX_COLOUR)
        VARYING_OUT LOW vec4 v_colour;
        #endif
        #if defined (TEXTURED)
        VARYING_OUT MED vec2 v_texCoord0;
        #endif

        void main()
        {
                vec3 camRight = vec3(u_viewMatrix[0][0], u_viewMatrix[1][0], u_viewMatrix[2][0]);
                vec3 camUp = vec3(u_viewMatrix[0][1], u_viewMatrix[1][1], u_viewMatrix[2][1]);
                vec3 position = (u_worldMatrix * vec4(a_normal, 1.0)).xyz;

                position = position + camRight * a_position.x
                                    + camUp * a_position.y;

                gl_Position = u_viewProjectionMatrix * vec4(position, 1.0);

//TODO: defs for scaled billboards
//TODO: defs for rx_shadow

                #if defined (VERTEX_COLOUR)
                    v_colour = a_colour;
                #endif
                #if defined (TEXTURED)
                    v_texCoord0 = a_texCoord0;
                #endif
        })";
}