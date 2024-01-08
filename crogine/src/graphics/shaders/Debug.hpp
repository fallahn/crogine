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

namespace cro::Shaders::Debug
{
    inline const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;
        ATTRIBUTE LOW vec4 a_colour;

        uniform mat4 u_projectionMatrix;

        VARYING_OUT MED vec4 v_colour;

        void main()
        {
            gl_Position = u_projectionMatrix * a_position;
            v_colour = a_colour;
        })";

    inline const std::string Fragment = R"(
        VARYING_IN LOW vec4 v_colour;
        OUTPUT

        void main()
        {
            FRAG_OUT = v_colour;
        })";

    //based on example at geeks3D
    inline const std::string NormalGeometry = 
        R"(
        layout(triangles) in;

        // Three lines will be generated: 6 vertices
        layout(line_strip, max_vertices=6) out;

        uniform mat4 u_modelMatrix;
        uniform mat4 u_viewProjectionMatrix;

        in Vertex
        {
          vec4 normal;
        } vertex[];

        out vec4 o_colour;

        const float NormalLength = 0.02;
        const vec4 Colour = vec4 (1.0, 1.0, 0.0, 1.0);

        void main()
        {
            mat4 mvp = u_viewProjectionMatrix * u_modelMatrix;

            int i = 0;
            for(i = 0; i < gl_in.length(); i++)
            {
                vec3 P = gl_in[i].gl_Position.xyz;
                vec3 N = vertex[i].normal.xyz;
    
                gl_Position = mvp * vec4(P, 1.0);
                o_colour = Colour;
                EmitVertex();
    
                gl_Position = mvp * vec4(P + N * NormalLength, 1.0);
                o_colour = Colour;
                EmitVertex();
    
                EndPrimitive();
            }
        }
        )";
}