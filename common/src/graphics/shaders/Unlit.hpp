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

#ifndef CRO_SHADER_UNLIT_HPP_
#define CRO_SHADER_UNLIT_HPP_

#include <string>

namespace cro
{
    namespace Shaders
    {
        namespace Unlit
        {
            const static std::string Vertex = R"(
                attribute vec3 a_position;
                attribute vec3 a_colour;
                attribute vec2 a_texCoord0;
                attribute vec2 a_texCoord1;

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;               
                uniform mat4 u_projectionMatrix;
                
                varying vec3 v_worldPosition;
                varying vec3 v_colour;
                varying vec2 v_texCoord0;
                varying vec2 v_texCoord1;

                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    gl_Position = wvp * vec4(a_position, 1.0);

                    v_worldPosition = (u_worldMatrix * vec4(a_position, 1.0)).xyz;

                    v_colour = a_colour;

                    v_texCoord0 = a_texCoord0;
                    v_texCoord1 = a_texCoord1;

                })";

            const static std::string Fragment = R"(

                uniform sampler2D u_diffuseMap;
                uniform sampler2D u_lightMap;

                varying vec3 v_colour;
                varying vec2 v_texCoord0;
                varying vec2 v_texCoord1;
                
                void main()
                {
                    gl_FragColor = texture2D(u_diffuseMap, v_texCoord0);
                })";
        }
    }
}

#endif //CRO_SHADER_UNLIT_HPP_