/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

Super Video Golf - zlib licence.

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

static inline const std::string ShopFragment = R"(
        OUTPUT
#include CAMERA_UBO
    #if defined(DIFFUSE_MAP)
        uniform sampler2D u_diffuseMap;
    #endif

    #if defined(MASK_MAP)
        uniform sampler2D u_maskMap;
    #else
        uniform vec4 u_maskColour = vec4(1.0);
    #endif

    #if defined(BUMP)
        uniform sampler2D u_normalMap;
    #endif

    #if defined(REFLECTIONS)
    #if defined (MASK_MAP)
        uniform samplerCubeArray u_reflectMap;
    #else
        uniform samplerCube u_reflectMap;
    #endif
    #endif

#include LIGHT_UBO

    #if defined (BALL_COLOUR)
        uniform vec4 u_ballColour = vec4(1.0);
    #endif

        VARYING_IN vec3 v_worldPosition;
        VARYING_IN vec4 v_colour;

    #if defined (BUMP)
        VARYING_IN vec3 v_tbn[3];
    #else
        VARYING_IN vec3 v_normalVector;
    #endif
    #if defined(TEXTURED)
        VARYING_IN vec2 v_texCoord0;
    #endif

#include LIGHT_COLOUR

        void main()
        {
        #if defined (BUMP)
            vec3 texNormal = TEXTURE(u_normalMap, v_texCoord0).rgb * 2.0 - 1.0;
            vec3 normal = normalize(v_tbn[0] * texNormal.r + v_tbn[1] * texNormal.g + v_tbn[2] * texNormal.b);
        #else
            vec3 normal = normalize(v_normalVector);
        #endif




        #if defined (DIFFUSE_MAP)
            vec4 diffuseColour = TEXTURE(u_diffuseMap, v_texCoord0);
        #else

        #if defined(NO_SUN_COLOUR)
            vec4 diffuseColour = vec4(1.0);
        #else
            vec4 diffuseColour = getLightColour();
        #endif
        #if defined (VERTEX_COLOUR)
            diffuseColour *= v_colour;
        #endif
            diffuseColour *= u_ballColour;

        #endif




        #if defined(MASK_MAP)
            vec4 mask = TEXTURE(u_maskMap, v_texCoord0);
            diffuseColour *= mask.b; //AO
        //#else
            //vec4 mask = u_maskColour;
        #endif
        
            vec3 lightDirection = normalize(-u_lightDirection);
            float amount = dot(normal, lightDirection);

#define COLOUR_LEVELS 2.0
#define AMOUNT_MIN 0.8
#define AMOUNT_MAX 0.2


            amount *= COLOUR_LEVELS;
            amount = round(amount);
            amount /= COLOUR_LEVELS;
            amount = (amount * AMOUNT_MAX) + AMOUNT_MIN;
            diffuseColour.rgb *= amount;


#if defined(REFLECTIONS)
            vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
            vec3 R = reflect(-eyeDirection, normal);

#if defined(MASK_MAP)
            //mask.g == roughness
            vec3 reflectColour = texture(u_reflectMap, vec4(R, mask.g * 4.0)).rgb;
            float smoothness = 1.0 - mask.g;

            //mask.r == metal (approx reflectiveness)
            diffuseColour.rgb = mix(diffuseColour.rgb, reflectColour, ((mask.r * 0.1) * smoothness) + (smoothness * 0.01));
#else
            vec3 reflectColour = texture(u_reflectMap, R).rgb;
            diffuseColour.rgb = (reflectColour * 0.25) + diffuseColour.rgb;
#endif
#endif

            FRAG_OUT = vec4(diffuseColour.rgb, 1.0);

        })";
