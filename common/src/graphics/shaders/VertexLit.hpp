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

#ifndef CRO_SHADER_VERTEX_HPP_
#define CRO_SHADER_VERTEX_HPP_

#include <string>

namespace cro
{
    namespace Shaders
    {
        namespace VertexLit
        {
            const static std::string Vertex = R"(
                attribute vec4 a_position;
                #if defined (VERTEX_COLOUR)
                attribute vec4 a_colour;
                #endif
                attribute vec3 a_normal;
                #if defined(BUMP)
                attribute vec3 a_tangent;
                attribute vec3 a_bitangent;
                #endif
                #if defined(TEXTURED)
                attribute vec2 a_texCoord0;
                #if defined (LIGHTMAPPED)
                attribute vec2 a_texCoord1;
                #endif
                #endif

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;
                uniform mat3 u_normalMatrix;                
                uniform mat4 u_projectionMatrix;
                
                varying vec3 v_worldPosition;
                #if defined (VERTEX_COLOUR)
                varying vec4 v_colour;
                #endif
                #if defined(BUMP)
                varying vec3 v_tbn[3];
                #else
                varying vec3 v_normalVector;
                #endif
                #if defined(TEXTURED)
                varying vec2 v_texCoord0;
                #if defined(LIGHTMAPPED)
                varying vec2 v_texCoord1;
                #endif
                #endif


                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    gl_Position = wvp * a_position;

                    v_worldPosition = (u_worldMatrix * a_position).xyz;
                #if defined(VERTEX_COLOUR)
                    v_colour = a_colour;
                #endif

                #if defined (BUMP)
                    v_tbn[0] = normalize(u_worldMatrix * vec4(a_tangent, 0.0)).xyz;
                    v_tbn[1] = normalize(u_worldMatrix * vec4(a_bitangent, 0.0)).xyz;
                    v_tbn[2] = normalize(u_worldMatrix * vec4(a_normal, 0.0)).xyz;
                #else
                    v_normalVector = u_normalMatrix * a_normal;
                #endif

                #if defined(TEXTURED)
                    v_texCoord0 = a_texCoord0;
                #if defined(LIGHTMAPPED)
                    v_texCoord1 = a_texCoord1;
                #endif
                #endif
                })";

            const static std::string Fragment = R"(
                #if defined(TEXTURED)
                uniform sampler2D u_diffuseMap;
                uniform sampler2D u_maskMap;
                #if defined(BUMP)
                uniform sampler2D u_normalMap;
                #if defined(LIGHTMAPPED)
                uniform sampler2D u_lightMap;
                #endif
                #endif
                #endif

                uniform vec3 u_cameraWorldPosition;
                #if defined(COLOURED)
                uniform vec4 u_colour;
                #endif

                varying vec3 v_worldPosition;
                #if defined(VERTEX_COLOUR)
                varying vec4 v_colour;
                #endif
                #if defined (BUMP)
                varying vec3 v_tbn[3];
                #else
                varying vec3 v_normalVector;
                #endif
                #if defined(TEXTURED)
                varying vec2 v_texCoord0;
                #if defined(LIGHTMAPPED)
                varying vec2 v_texCoord1;
                #endif
                #endif
                
                vec3 lightDir = vec3(0.1, -0.8, -0.2);

                vec3 diffuseColour;
                vec3 eyeDirection;
                vec3 mask;
                vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
                {
                    float diffuseAmount = max(dot(normal, lightDirection), 0.0);
                    //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
                    vec3 mixedColour = diffuseColour * lightDiffuse * diffuseAmount * falloff;

                    vec3 halfVec = normalize(eyeDirection + lightDirection);
                    float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
                    vec3 specularColour = lightSpecular * vec3(pow(specularAngle, ((254.0 * mask.r) + 1.0))) * falloff;

                    return mixedColour + (specularColour * mask.g);
                }

                void main()
                {
                #if defined (BUMP)
                    vec3 texNormal = texture2D(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
                    vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
                #else
                    vec3 normal = normalize(v_normalVector);
                #endif
                #if defined (TEXTURED)
                    vec4 diffuse = texture2D(u_diffuseMap, v_texCoord0);
                    mask = texture2D(u_maskMap, v_texCoord0).rgb;
                #elif defined(COLOURED)
                    vec4 diffuse = u_colour;
                #elif defined(VERTEX_COLOURED)
                    vec4 diffuse = v_colour;
                #else
                    vec4 diffuse = vec4(1.0);
                #endif
                    diffuseColour = diffuse.rgb;
                    vec3 blendedColour = diffuse.rgb * 0.2; //ambience
                    eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

                    blendedColour += calcLighting(normal, normalize(-lightDir), vec3(0.48), vec3(1.0), 1.0);
                #if defined(TEXTURED)
                    gl_FragColor.rgb = mix(blendedColour, diffuseColour, mask.b);
                #else     
                    gl_FragColor.rgb = blendedColour;
                #endif
                    gl_FragColor.a = diffuse.a;
                })";
        }
    }
}

#endif //CRO_SHADER_VERTEX_HPP_