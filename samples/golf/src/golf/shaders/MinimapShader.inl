/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
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

inline const std::string MinimapVertex = R"(
        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        ATTRIBUTE vec2 a_position;
        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE LOW vec4 a_colour;

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT MED vec2 v_texCoord;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_colour = a_colour;
            v_texCoord = a_texCoord0;
        })";

//minimap as in top down view of green/flight camera
inline const std::string MinimapFragment = R"(
        
        uniform sampler2D u_texture;
        uniform sampler2D u_depthTexture;
        uniform sampler2D u_lightTexture;
        uniform sampler2D u_blurTexture;
        uniform sampler2D u_maskTexture;

#if defined (RAIN)
        uniform sampler2D u_distortionTexture;
        uniform float u_distortAmount = 1.0;
        uniform vec4 u_subrect = vec4(0.0, 0.0, 1.0, 1.0);
#endif

        uniform vec4 u_lightColour;

#include WIND_BUFFER
#include SCALE_BUFFER

        VARYING_IN LOW vec4 v_colour;
        VARYING_IN MED vec2 v_texCoord;
        OUTPUT
        
        const float stepPos = (0.49 * 0.49);
        const float borderPos = (0.46 * 0.46);
        const vec4 borderColour = vec4(vec3(0.0), 0.25);

        //these ought to be uniforms for texture
        //res and screen scale
        const float res = 100.0;
        const float invRes = 0.01;
        const float scale = 2.0;
        const float invScale = 0.5;

#include FOG_COLOUR

#if defined (LIGHT_COLOUR)
#include BAYER_MATRIX

        const float Brightness = 1.1;
        const float Contrast = 2.0;

        float brightnessContrast(float value)
        {
            return (value - 0.5) * Contrast + 0.5 + Brightness;
        }
#endif

        float rand()
        {
            return fract(sin(u_windData.w) * 1e4);
        }

        float rand(vec2 position)
        {
            return fract(sin(dot(position, vec2(12.9898, 4.1414)) + u_windData.w) * 43758.5453);
        }

        void main()
        {
            vec2 pos = (round(floor(v_texCoord * res) * scale) * invScale) * invRes;

            vec2 dir = pos - vec2(0.5);
            float length2 = dot(dir,dir);

            vec3 noise = vec3(rand(floor((v_texCoord * textureSize(u_texture, 0)) / u_pixelScale)));


            vec2 coord = v_texCoord;
#if defined (RAIN)
            vec2 rainCoord  = u_subrect.xy + (v_texCoord * u_subrect.zw);
            vec4 rain = TEXTURE(u_distortionTexture, rainCoord) * 2.0 - 1.0;
            
            coord = mix(coord, coord + rain.xy, u_distortAmount);
#endif

            vec4 colour = TEXTURE(u_texture, coord);
            colour.rgb = dim(colour.rgb);

            float depthSample = TEXTURE(u_depthTexture, coord).r;
            float d = getDistance(depthSample);
            float fogMix = fogAmount(d);

#if defined(LIGHT_COLOUR)
            float maskAmount = TEXTURE(u_maskTexture, coord).a;
            fogMix *= maskAmount;
#endif

            colour = mix(colour, FogColour * u_lightColour, fogMix);

#if defined (LIGHT_COLOUR)
            vec3 lightColour = TEXTURE(u_lightTexture, coord).rgb;
            colour.rgb += lightColour;

            maskAmount = (0.95 * maskAmount) + 0.05;

            vec3 blurColour = TEXTURE(u_blurTexture, coord).rgb * (1.0 - ((d * 0.9) + 0.1));
            colour.rgb = (blurColour * 0.5 * maskAmount) + colour.rgb;
#endif

            FRAG_OUT = vec4(mix(noise, colour.rgb, v_colour.r), 1.0);
            FRAG_OUT = mix(FRAG_OUT, borderColour, step(borderPos, length2));
            FRAG_OUT.a *= 1.0 - step(stepPos, length2);
        })";

//minimap as in mini course view
//well, this is a silly inconsistency...
inline const std::string MinimapViewVertex = R"(
        ATTRIBUTE vec2 a_position;
        ATTRIBUTE vec2 a_texCoord0;
        ATTRIBUTE vec4 a_colour;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        uniform mat4 u_coordMatrix = mat4(1.0);

        VARYING_OUT vec2 v_texCoord0;
        VARYING_OUT vec2 v_texCoord1;
        VARYING_OUT vec4 v_colour;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_texCoord0 = (u_coordMatrix * vec4(a_texCoord0, 0.0, 1.0)).xy;
            v_texCoord1 = a_texCoord0;
            v_colour = a_colour;
        })";

inline const std::string MinimapViewFragment = R"(
        uniform sampler2D u_texture;

        VARYING_IN vec2 v_texCoord0;
        VARYING_IN vec2 v_texCoord1;
        VARYING_IN vec4 v_colour;

        OUTPUT

        //const float RadiusOuter = (0.4995 * 0.4995);
        //const float RadiusInner = (0.47 * 0.47);

        //these ought to be uniforms for texture
        //res and screen scale
        const vec2 res = vec2(180.0, 100.0);
        const float scale = 2.0;

        //https://stackoverflow.com/a/43994314/6740859
        vec2 uv = vec2(0.0);
        float roundedRectangle (vec2 pos, vec2 size, float radius, float thickness)
        {
          float d = length(max(abs(uv - pos),size) - size) - radius;
          return smoothstep(0.66, 0.33, d / thickness * 5.0);
        }

        void main()
        {
            uv = (round(floor(v_texCoord1 * res) * scale) / scale) / res;
            uv = (2.0 * uv - 1.0);
            uv.x *= res.x/res.y;

            vec4 col = TEXTURE(u_texture, v_texCoord0) * v_colour;
            col.a *= roundedRectangle(vec2(0.0), vec2(1.28, 0.5), 0.42, 0.2);

            FRAG_OUT = col;

            vec4 bgColour = mix(vec4(0.0), vec4(vec3(0.0), 0.25), roundedRectangle(vec2(0.0), vec2(1.33, 0.55), 0.425, 0.2));
            FRAG_OUT = mix(bgColour, FRAG_OUT, FRAG_OUT.a);

        })";