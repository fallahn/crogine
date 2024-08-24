#pragma once

#include <string>

//based on https://www.rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/

static inline const std::string GaussianFrag =
R"(
uniform sampler2D u_texture;

VARYING_IN vec4 v_colour;
 
OUTPUT
 
uniform float offset[3] = float[](0.0, 1.3846153846, 3.2307692308);
uniform float weight[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

#if defined(HORIZONTAL)
const float TextureSize = 1920.0; //TODO set this at compile tile from const vals
#define OFFSET vec2(offset[i], 0.0)
#else
const float TextureSize = 1080.0;
#define OFFSET vec2(0.0, offset[i])
#endif
 
void main()
{
    vec4 colour = TEXTURE(u_texture, vec2(gl_FragCoord) / TextureSize) * weight[0];

    for (int i = 1; i < 3; i++)
    {
        colour += TEXTURE(u_texture, (vec2(gl_FragCoord) + OFFSET) / TextureSize) * weight[i];
        colour += TEXTURE(u_texture, (vec2(gl_FragCoord) - OFFSET) / TextureSize) * weight[i];
    }

    FRAG_OUT.rgb = colour.rgb;
    FRAG_OUT.a = 1.0;

    FRAG_OUT *= v_colour;
}

)";
