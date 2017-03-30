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

#ifndef CRO_SHADER_DEFAULT_HPP_
#define CRO_SHADER_DEFAULT_HPP_

#include <string>

namespace cro
{
    namespace Shaders
    {
        namespace Default
        {
            const static std::string Vertex = R"(
                attribute vec3 a_position;
                attribute vec2 a_texCoord0;

                uniform mat4 u_projectionMatrix;
                uniform mat4 u_worldViewMatrix;

                varying vec2 v_texCoord;

                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    gl_Position = wvp * vec4(a_position, 1.0);

                    v_texCoord = a_texCoord0;
                })";

            const static std::string Fragment = R"(
                varying vec2 v_texCoord;
                
                void main()
                {
                    gl_FragColor = vec4(v_texCoord.x, v_texCoord.y, 0.0, 1.0);
                })";
        }
    }
}

#endif //CRO_SHADER_DEFAULT_HPP_