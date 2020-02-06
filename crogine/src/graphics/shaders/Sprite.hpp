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
        namespace Sprite
        {
            const static std::string Vertex = R"(
                ATTRIBUTE vec4 a_position;
                ATTRIBUTE LOW vec4 a_colour;
                ATTRIBUTE MED vec2 a_texCoord0;
                ATTRIBUTE MED vec2 a_texCoord1; //this actually has the matrix index in the x component

                uniform mat4 u_projectionMatrix;
                uniform mat4 u_worldMatrix[MAX_MATRICES];               
               
                VARYING_OUT LOW vec4 v_colour;
                VARYING_OUT MED vec2 v_texCoord0;

                void main()
                {
                    int idx = int(clamp(a_texCoord1.x, 0.0, float(MAX_MATRICES - 1)));/* u_worldMatrix[idx]*/
                    gl_Position = u_projectionMatrix * u_worldMatrix[idx] * a_position;
                    v_colour = a_colour;
                    //v_colour *= a_texCoord1.y;
                    v_texCoord0 = a_texCoord0;// * a_texCoord1;
                })";

            const static std::string Fragment = R"(
                uniform sampler2D u_texture;
                
                VARYING_IN LOW vec4 v_colour;
                VARYING_IN MED vec2 v_texCoord0;
                OUTPUT

                void main()
                {
                    FRAG_OUT = TEXTURE(u_texture, v_texCoord0) * v_colour;
                })";
        }

        namespace Text
        {
            const static std::string BitmapFragment = R"(
                uniform sampler2D u_texture;
                
                VARYING_IN LOW vec4 v_colour;
                VARYING_IN MED vec2 v_texCoord0;
                OUTPUT

                void main()
                {
                    float value = TEXTURE(u_texture, v_texCoord0).a;
                    FRAG_OUT = smoothstep(0.3, 1.0, value) * v_colour;
                    FRAG_OUT.a *= value;

                    //FRAG_OUT = v_colour;
                })";

            const static std::string SDFFragment = R"(
                uniform sampler2D u_texture;
                
                VARYING_IN LOW vec4 v_colour;
                VARYING_IN MED vec2 v_texCoord0;
                OUTPUT

                MED float spread = 4.0;
                MED float scale = 5.0;
                MED float smoothing = 1.0 / 16.0;

                void main()
                {
                    //smoothing = 0.25 / (spread * scale);
                    MED float value = TEXTURE(u_texture, v_texCoord0).r;
                    MED float alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, value);
                    FRAG_OUT = vec4(v_colour.rgb, v_colour.a * alpha);
                })";
        }
    }
}