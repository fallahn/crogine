/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2021
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

namespace cro::Shaders::Sprite
{
    inline const std::string Vertex = R"(
        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        ATTRIBUTE vec2 a_position;
        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE LOW vec4 a_colour;

        VARYING_OUT LOW vec4 v_colour;

        #if defined(TEXTURED)
        VARYING_OUT MED vec2 v_texCoord;
        #endif

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_colour = a_colour;
        #if defined(TEXTURED)
            v_texCoord = a_texCoord0;
        #endif
        })";

    inline const std::string Coloured = R"(
        VARYING_IN LOW vec4 v_colour;
        OUTPUT
            
        void main()
        {
            FRAG_OUT = v_colour;
        })";

    inline const std::string Textured = R"(
        uniform sampler2D u_texture;

        VARYING_IN LOW vec4 v_colour;
        VARYING_IN MED vec2 v_texCoord;
        OUTPUT
            
        void main()
        {
            FRAG_OUT = TEXTURE(u_texture, v_texCoord) * v_colour;
        })";
}

namespace cro::Shaders::Text
{
    inline const std::string BitmapFragment = R"(
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

    inline const std::string SDFFragment = R"(
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