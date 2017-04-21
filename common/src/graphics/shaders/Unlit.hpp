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
                #if defined(VERTEX_COLOUR)
                attribute vec4 a_colour;
                #endif
                if defined(TEXTURED)
                attribute vec2 a_texCoord0;
                if defined(LIGHTMAPPED)
                attribute vec2 a_texCoord1;
                #endif
                #endif

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;               
                uniform mat4 u_projectionMatrix;
                
                #if defined (VERTEX_COLOURED)
                varying vec4 v_colour;
                #endif
                #if defined (TEXTURED)
                varying vec2 v_texCoord0;
                #if defined (LIGHTMAPPED)
                varying vec2 v_texCoord1;
                #endif
                #endif

                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    gl_Position = wvp * vec4(a_position, 1.0);

                #if defined (VERTEX_COLOUR)
                    v_colour = a_colour;
                #endif
                #if defined (TEXTURED)
                    v_texCoord0 = a_texCoord0;
                #if defined (LIGHTMAPPED)
                    v_texCoord1 = a_texCoord1;
                #endif
                #endif
                })";

            const static std::string Fragment = R"(
                #if defined (TEXTURED)
                uniform sampler2D u_diffuseMap;
                #if defined (LIGHTMAPPED)
                uniform sampler2D u_lightMap;
                #endif
                #endif

                #if defined (VERTEX_COLOUR)
                varying vec4 v_colour;
                #endif
                #if defined (TEXTURED)
                varying vec2 v_texCoord0;
                #if defined (LIGHTMAPPED)
                varying vec2 v_texCoord1;
                #endif
                #endif
                
                void main()
                {
                #if defined (VERTEX_COLOUR)
                    gl_FragColour = v_colour;
                #else
                    gl_FragColour = vec4(1.0);
                #endif
                #if defined (TEXTURED)
                    gl_FragColor *= texture2D(u_diffuseMap, v_texCoord0);
                #if defined (LIGHTMAPPED)
                    gl_FragColour *= texture2D(u_lightMap, v_texCoord1);
                #endif
                #endif
                })";
        }
    }
}

#endif //CRO_SHADER_UNLIT_HPP_