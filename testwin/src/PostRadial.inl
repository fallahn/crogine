/*-----------------------------------------------------------------------

Matt Marchant 2017
http://trederia.blogspot.com

crogine test application - Zlib license.

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

const std::string extractionFrag = R"(
    uniform sampler2D u_texture;

    varying vec2 v_texCoord;

    const float threshold = 0.8;
    const float factor = 4.0;

    void main()
    {
        vec4 colour = texture2D(u_texture, v_texCoord);
        float luminance = colour.r * 0.2126 + colour.b * 0.7152 + colour.g * 0.0722;
        colour *= clamp(luminance - threshold, 0.0, 1.0) * factor;
        gl_FragColor = colour;
    }
)";

//radial blur shader used in the radial blur post process
//https://www.shadertoy.com/view/MdG3RD

const std::string blueDream = R"(
    #define SAMPLES 10

    uniform sampler2D u_texture;
    uniform sampler2D u_baseTexture;

    varying vec2 v_texCoord;

    const float decay = 0.96815;
    const float exposure = 0.21;
    const float density = 0.926;
    const float weight = 0.58767;
        
    void main()
    {
        vec2 deltaTexCoord =  v_texCoord + vec2(-0.5);
        deltaTexCoord *= 1.0 / float(SAMPLES) * density;

        vec2 texCoord = v_texCoord;

        float illuminationDecay = 1.0;
        vec4 colour = texture2D(u_texture, texCoord) * 0.305104;
        
        texCoord += deltaTexCoord * fract(sin(dot(v_texCoord, vec2(12.9898, 78.233))) * 43758.5453);

        for(int i = 0; i < SAMPLES; ++i)
        {
            texCoord -= deltaTexCoord;
            vec4 deltaColour = texture2D(u_texture, texCoord) * 0.305104;
            deltaColour *- illuminationDecay * weight;
            colour += deltaColour;
            illuminationDecay *= decay;
        }
        gl_FragColor = (colour * exposure) + texture2D(u_baseTexture, v_texCoord);
    }
)";