#pragma once

#include <string>

const std::string KuwaharaFragment = R"(
    uniform sampler2D u_texture;

    VARYING_IN vec2 v_texCoord;

    OUTPUT

    const int MAX_SIZE = 5;
    #define MAX_KERNEL ((MAX_SIZE * 2 + 1) * (MAX_SIZE * 2 +1))
    const vec3 Ratios = vec3(0.3, 0.59, 0.11);

    vec4 mean = vec4(0.0);
    float minVariance = -1.0;
    vec2 texSize = textureSize(u_texture, 0);
    float values[MAX_KERNEL];

    void getMean(int i0, int i1, int j0, int j1)
    {

        int count = 0;
        vec4 temp = vec4(0.0);

        for (int j = j0; j <= j1; ++j)
        {
            for (int i = i0; i <= i1; ++i)
            {
                vec4 colour = TEXTURE(u_texture, gl_FragCoord.xy + vec2(i,j) / texSize);
                temp += colour;

                values[count++] = dot(colour.rgb, Ratios);
            }
        }

        temp /= count;
        float meanValue = dot(temp.rgb, Ratios);

        float variance = 0.0;
        for (int i = 0; i < count; ++i)
        {
            variance += pow(values[i] - meanValue, 2.0);
        }
        variance /= count;

        if (variance < minVariance || minVariance <= -1)
        {
            mean = temp;
            minVariance = variance;
        }
    }

    void main()
    {
        getMean(-MAX_SIZE, 0, -MAX_SIZE, 0);
        getMean(0, MAX_SIZE, 0, MAX_SIZE);
        getMean(-MAX_SIZE, 0, 0, MAX_SIZE);
        getMean(0, MAX_SIZE, -MAX_SIZE, 0);

        FRAG_OUT = vec4(mean.rgb, 1.0);
    })";

const std::string MipmapFragment = R"(
    uniform sampler2D u_texture;

    uniform sampler2D u_depthMap;

    VARYING_IN vec2 v_texCoord;
    VARYING_IN vec4 v_colour;

    OUTPUT

    void main()
    {
        float depth = TEXTURE(u_depthMap, v_texCoord).r;

        FRAG_OUT = textureLod(u_texture, v_texCoord, smoothstep(0.6, 0.8, depth) * 2.0) * v_colour;// * vec4(1.0, 0.0, 0.0, 1.0);
    })";


const std::string GaussianFragment = R"(
    uniform sampler2D u_texture;

    VARYING_IN vec2 v_texCoord;
    VARYING_IN vec4 v_colour; 

    OUTPUT

    const float BLUR_OFFSETS[5] = float[](0.0, 1.0, 2.0, 3.0, 4.0);
    const float BLUR_WEIGHTS[5] = float[](0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

    vec4 blur(vec2 direction, vec2 texel)
    {
        vec4 color = texture(u_texture, v_texCoord) * BLUR_WEIGHTS[0];
    
        for (int i = 1; i < 5; i++)
        { 
            vec2 offset = texel * direction * BLUR_OFFSETS[i];
            color += texture(u_texture, v_texCoord + offset) * BLUR_WEIGHTS[i];
            color += texture(u_texture, v_texCoord - offset) * BLUR_WEIGHTS[i];
        }
    
        return color;
    }

    void main(void)
    {
        FRAG_OUT = blur(vec2(0.0, 1.0), vec2(1.0) / textureSize(u_texture, 0));
    })";
