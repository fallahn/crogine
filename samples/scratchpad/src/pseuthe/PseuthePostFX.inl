#pragma once

#include <string>

//based on https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/

static inline const std::string GaussianFrag =
R"(
uniform sampler2D u_texture;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

#if defined(HORIZONTAL)
const vec2 offset = vec2(1.0/1920.0, 0.0);
#else
const vec2 offset = vec2(0.0,1.0/1080.0);
#endif

void main()
{
    vec2 texCoords = v_texCoord;
    vec4 colour = vec4(0.0);
    colour += TEXTURE(u_texture, texCoords - 4.0 * offset) * 0.0162162162;
    colour += TEXTURE(u_texture, texCoords - 3.0 * offset) * 0.0540540541;
    colour += TEXTURE(u_texture, texCoords - 2.0 * offset) * 0.1216216216;
    colour += TEXTURE(u_texture, texCoords - offset) * 0.1945945946;
    colour += TEXTURE(u_texture, texCoords) * 0.2270270270;
    colour += TEXTURE(u_texture, texCoords + offset) * 0.1945945946;
    colour += TEXTURE(u_texture, texCoords + 2.0 * offset) * 0.1216216216;
    colour += TEXTURE(u_texture, texCoords + 3.0 * offset) * 0.0540540541;
    colour += TEXTURE(u_texture, texCoords + 4.0 * offset) * 0.0162162162;
    FRAG_OUT = colour;// * v_colour;

    FRAG_OUT.a = 1.0;
})";


//R"(
//uniform sampler2D u_texture;
//
//VARYING_IN vec4 v_colour;
// 
//OUTPUT
// 
//uniform float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
//uniform float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);
//
//#if defined(HORIZONTAL)
//const float TextureSize = 1920.0; //TODO set this at compile tile from const vals
//#define OFFSET vec2(offset[i], 0.0)
//#else
//const float TextureSize = 1080.0;
//#define OFFSET vec2(0.0, offset[i])
//#endif
// 
//void main()
//{
//    vec4 colour = TEXTURE(u_texture, vec2(gl_FragCoord) / TextureSize) * weight[0];
//
//    for (int i = 1; i < 3; i++)
//    {
//        colour += TEXTURE(u_texture, (vec2(gl_FragCoord) + OFFSET) / TextureSize) * weight[i];
//        colour += TEXTURE(u_texture, (vec2(gl_FragCoord) - OFFSET) / TextureSize) * weight[i];
//    }
//
//    FRAG_OUT.rgb = colour.rgb;
//    FRAG_OUT.a = 1.0;
//
//    FRAG_OUT *= v_colour;
//}
//
//)";
