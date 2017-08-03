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
                attribute LOW vec4 a_colour;
                #endif
                attribute vec3 a_normal;
                #if defined(BUMP)
                attribute vec3 a_tangent;
                attribute vec3 a_bitangent;
                #endif
                #if defined(TEXTURED)
                attribute MED vec2 a_texCoord0;
                #endif
                #if defined (LIGHTMAPPED)
                attribute MED vec2 a_texCoord1;
                #endif

                #if defined(SKINNED)
                attribute vec4 a_boneIndices;
                attribute vec4 a_boneWeights;
                uniform mat4 u_boneMatrices[MAX_BONES];
                #endif

                #if defined(PROJECTIONS)
                #define MAX_PROJECTIONS 8
                uniform mat4 u_projectionMapMatrix[MAX_PROJECTIONS]; //VP matrices for texture projection
                uniform LOW int u_projectionMapCount; //how many to actually draw
                #endif

                uniform mat4 u_worldMatrix;
                uniform mat4 u_worldViewMatrix;
                uniform mat3 u_normalMatrix;                
                uniform mat4 u_projectionMatrix;

                #if defined(RX_SHADOWS)
                uniform mat4 u_lightViewProjectionMatrix;
                #endif

                #if defined (SUBRECTS)
                uniform MED vec4 u_subrect;
                #endif
                
                varying vec3 v_worldPosition;
                #if defined (VERTEX_COLOUR)
                varying LOW vec4 v_colour;
                #endif
                #if defined(BUMP)
                varying vec3 v_tbn[3];
                #else
                varying vec3 v_normalVector;
                #endif
                #if defined(TEXTURED)
                varying MED vec2 v_texCoord0;
                #endif
                #if defined(LIGHTMAPPED)
                varying MED vec2 v_texCoord1;
                #endif

                #if defined(RX_SHADOWS)
                varying LOW vec4 v_lightWorldPosition;
                #endif

                void main()
                {
                    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
                    vec4 position = a_position;

                #if defined(PROJECTIONS)
                    for(int i = 0; i < u_projectionMapCount; ++i)
                    {
                        v_projectionCoords[i] = u_projectionMapMatrix[i] * u_worldMatrix * a_position;
                    }
                #endif

                #if defined(SKINNED)
                	//int idx = 0;//int(a_boneIndices.x);
                    mat4 skinMatrix = u_boneMatrices[int(a_boneIndices.x)] * a_boneWeights.x;
                	skinMatrix += u_boneMatrices[int(a_boneIndices.y)] * a_boneWeights.y;
                	skinMatrix += u_boneMatrices[int(a_boneIndices.z)] * a_boneWeights.z;
                	skinMatrix += u_boneMatrices[int(a_boneIndices.w)] * a_boneWeights.w;
                	position = skinMatrix * position;
                #endif

                    gl_Position = wvp * position;

                #if defined (RX_SHADOWS)
                    v_lightWorldPosition = u_lightViewProjectionMatrix * u_worldMatrix * position;
                #endif

                    v_worldPosition = (u_worldMatrix * a_position).xyz;
                #if defined(VERTEX_COLOUR)
                    v_colour = a_colour;
                #endif

                vec3 normal = a_normal;

                #if defined(SKINNED)
                    normal = (skinMatrix * vec4(normal, 0.0)).xyz;
                #endif

                #if defined (BUMP)
                    vec4 tangent = vec4(a_tangent, 0.0);
                    vec4 bitangent = vec4(a_bitangent, 0.0);
                #if defined (SKINNED)
                    tangent = skinMatrix * tangent;
                    bitangent = skinMatrix * bitangent;
                #endif
                    v_tbn[0] = normalize(u_worldMatrix * tangent).xyz;
                    v_tbn[1] = normalize(u_worldMatrix * bitangent).xyz;
                    v_tbn[2] = normalize(u_worldMatrix * vec4(normal, 0.0)).xyz;
                #else
                    v_normalVector = u_normalMatrix * normal;
                #endif

                #if defined(TEXTURED)
                #if defined (SUBRECTS)
                    v_texCoord0 = u_subrect.xy + (a_texCoord0 * u_subrect.zw);
                #else
                    v_texCoord0 = a_texCoord0;                    
                #endif
                #endif
                #if defined(LIGHTMAPPED)
                    v_texCoord1 = a_texCoord1;
                #endif
                })";

            const static std::string Fragment = R"(
                #if defined(TEXTURED)
                uniform sampler2D u_diffuseMap;
                uniform sampler2D u_maskMap;
                #if defined(BUMP)
                uniform sampler2D u_normalMap;
                #endif
                #endif
                #if defined(LIGHTMAPPED)
                uniform sampler2D u_lightMap;
                #endif

                uniform HIGH vec3 u_lightDirection;
                uniform LOW vec4 u_lightColour;
                uniform HIGH vec3 u_cameraWorldPosition;
                #if defined(COLOURED)
                uniform LOW vec4 u_colour;
                uniform LOW vec4 u_maskColour;
                #endif

                #if defined(PROJECTIONS)
                #define MAX_PROJECTIONS 8
                uniform sampler2D u_projectionMap;
                uniform LOW int u_projectionMapCount;
                #endif

                #if defined (RX_SHADOWS)
                uniform sampler2D u_shadowMap;
                #endif

                #if defined(RIMMING)
                uniform LOW vec4 u_rimColour;
                uniform LOW float u_rimFalloff;
                #endif

                varying HIGH vec3 v_worldPosition;
                #if defined(VERTEX_COLOUR)
                varying LOW vec4 v_colour;
                #endif
                #if defined (BUMP)
                varying HIGH vec3 v_tbn[3];
                #else
                varying HIGH vec3 v_normalVector;
                #endif
                #if defined(TEXTURED)
                varying MED vec2 v_texCoord0;
                #endif
                #if defined(LIGHTMAPPED)
                varying MED vec2 v_texCoord1;
                #endif
 
                #if defined(PROJECTIONS)
                varying LOW vec4 v_projectionCoords[MAX_PROJECTIONS];
                #endif

                #if defined(RX_SHADOWS)
                varying LOW vec4 v_lightWorldPosition;

                #if defined (GL_FRAGMENT_PRECISION_HIGH)
                #define PREC highp
                #else
                #define PREC mediump
                #endif

                PREC float unpack(PREC vec4 colour)
                {
                    const PREC vec4 bitshift = vec4(1.0 / 16777216.0, 1.0 / 65536.0, 1.0 / 256.0, 1.0);
                    return dot(colour, bitshift);
                }
                
                //#if defined(MOBILE)
                PREC float shadowAmountM(LOW vec4 lightWorldPos)
                {
                    PREC vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
                    projectionCoords = projectionCoords * 0.5 + 0.5;
                    PREC float depthSample = unpack(texture2D(u_shadowMap, projectionCoords.xy));
                    PREC float currDepth = projectionCoords.z - 0.005;
                    return (currDepth < depthSample) ? 1.0 : 0.4;
                }
                //#else
                //some fancier pcf on desktop
                const vec2 kernel[16] = vec2[](
                    vec2(-0.94201624, -0.39906216),
                    vec2(0.94558609, -0.76890725),
                    vec2(-0.094184101, -0.92938870),
                    vec2(0.34495938, 0.29387760),
                    vec2(-0.91588581, 0.45771432),
                    vec2(-0.81544232, -0.87912464),
                    vec2(-0.38277543, 0.27676845),
                    vec2(0.97484398, 0.75648379),
                    vec2(0.44323325, -0.97511554),
                    vec2(0.53742981, -0.47373420),
                    vec2(-0.26496911, -0.41893023),
                    vec2(0.79197514, 0.19090188),
                    vec2(-0.24188840, 0.99706507),
                    vec2(-0.81409955, 0.91437590),
                    vec2(0.19984126, 0.78641367),
                    vec2(0.14383161, -0.14100790)
                );
                const int filterSize = 3;
                float shadowAmount(vec4 lightWorldPos)
                {
                    vec3 projectionCoords = lightWorldPos.xyz / lightWorldPos.w;
                    projectionCoords = projectionCoords * 0.5 + 0.5;

                    if(projectionCoords.z > 1.0) return 1.0;

                    float shadow = 0.0;
                    vec2 texelSize = 1.0 / textureSize(u_shadowMap, 0).xy;
                    for(int x = 0; x < filterSize; ++x)
                    {
                        for(int y = 0; y < filterSize; ++y)
                        {
                            float pcfDepth = unpack(texture2D(u_shadowMap, projectionCoords.xy + kernel[y * filterSize + x] * texelSize));
                            shadow += (projectionCoords.z - 0.001) > pcfDepth ? 0.4 : 0.0;
                        }
                    }
                    return 1.0 - (shadow / 9.0);
                }
                //#endif

                #endif               

                LOW vec3 diffuseColour;
                HIGH vec3 eyeDirection;
                LOW vec3 mask = vec3(1.0, 1.0, 0.0);
                vec3 calcLighting(vec3 normal, vec3 lightDirection, vec3 lightDiffuse, vec3 lightSpecular, float falloff)
                {
                    MED float diffuseAmount = max(dot(normal, lightDirection), 0.0);
                    //diffuseAmount = pow((diffuseAmount * 0.5) + 5.0, 2.0);
                    MED vec3 mixedColour = diffuseColour * lightDiffuse * diffuseAmount * falloff;

                    MED vec3 halfVec = normalize(eyeDirection + lightDirection);
                    MED float specularAngle = clamp(dot(normal, halfVec), 0.0, 1.0);
                    LOW vec3 specularColour = lightSpecular * vec3(pow(specularAngle, ((254.0 * mask.r) + 1.0))) * falloff;

                    return mixedColour + (specularColour * mask.g);
                }

                void main()
                {
                #if defined (BUMP)
                    MED vec3 texNormal = texture2D(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
                    MED vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
                #else
                    MED vec3 normal = normalize(v_normalVector);
                #endif
                #if defined (TEXTURED)
                    LOW vec4 diffuse = texture2D(u_diffuseMap, v_texCoord0);
                    mask = texture2D(u_maskMap, v_texCoord0).rgb;
                #elif defined(COLOURED)
                    LOW vec4 diffuse = u_colour;
                    mask = u_maskColour.rgb;
                #elif defined(VERTEX_COLOURED)
                    LOW vec4 diffuse = v_colour;
                    mask = u_maskColour.rgb;
                #else
                    LOW vec4 diffuse = vec4(1.0);
                    mask = u_maskColour.rgb;
                #endif
                    diffuseColour = diffuse.rgb;
                    LOW vec3 blendedColour = diffuse.rgb * 0.2; //ambience
                    eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);

                    blendedColour += calcLighting(normal, normalize(-u_lightDirection), u_lightColour.rgb, vec3(1.0), 1.0);
                #if defined (RX_SHADOWS)
                    //blendedColour *= shadowAmountM(v_lightWorldPosition);
                #endif

                #if defined(TEXTURED)
                    gl_FragColor.rgb = mix(blendedColour, diffuseColour, mask.b);
                #else     
                    gl_FragColor.rgb = blendedColour;
                #endif
                #if defined (LIGHTMAPPED)
                    gl_FragColor *= texture2D(u_lightMap, v_texCoord1);
                #endif
                    gl_FragColor.a = diffuse.a;

                #if defined(PROJECTIONS)
                    for(int i = 0; i < u_projectionMapCount; ++i)
                    {
                        if(v_projectionCoords[i].w > 0.0)
                        {
                            vec2 coords = v_projectionCoords[i].xy / v_projectionCoords[i].w / 2.0 + 0.5;
                            gl_FragColor *= texture2D(u_projectionMap, coords);
                        }
                    }
                #endif

                #if defined (RIMMING)
                    LOW float rim = 1.0 - dot(normal, eyeDirection);
                    rim = smoothstep(u_rimFalloff, 1.0, rim);
                    //gl_FragColor.rgb = mix(gl_FragColor.rgb, u_rimColour.rgb, rim);
                    gl_FragColor += u_rimColour * rim ;//* 0.5;
                #endif
                })";
        }
    }
}

#endif //CRO_SHADER_VERTEX_HPP_