/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef CRO_SHADER_SPRITE_HPP_
#define CRO_SHADER_SPRITE_HPP_

#include <string>

namespace cro
{
    namespace Shaders
    {
        namespace Sprite
        {
            const static std::string Vertex = R"(
                attribute vec4 a_position;
                attribute vec4 a_colour;
                attribute vec2 a_texCoord0;
                attribute vec2 a_texCoord1; //this actually has the matrix index in the x component

                uniform mat4 u_projectionMatrix;
                uniform mat4 u_worldMatrix[MAX_MATRICES];               
               
                varying vec4 v_colour;
                varying vec2 v_texCoord0;

                void main()
                {
                    int idx = int(clamp(a_texCoord1.x, 0.0, MAX_MATRICES - 1));/* u_worldMatrix[idx]*/
                    gl_Position = u_projectionMatrix * a_position;
                    v_colour = a_colour;
                    //v_colour.rg *= a_texCoord1;
                    v_texCoord0 = a_texCoord0 * a_texCoord1;
                })";

            const static std::string Fragment = R"(
                uniform sampler2D u_texture;
                
                varying vec4 v_colour;
                varying vec2 v_texCoord0;

                void main()
                {
                    //vec4 buns = texture2D(u_texture, v_texCoord0) * 0.1;
                    gl_FragColor = texture2D(u_texture, v_texCoord0) * v_colour;
                })";
        }
    }
}

#endif //CRO_SHADER_SPRITE_HPP_