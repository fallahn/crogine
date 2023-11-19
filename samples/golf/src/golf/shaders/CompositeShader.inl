/*-----------------------------------------------------------------------

Matt Marchant 2023
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

inline const std::string BlurVert =
R"(
ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;

uniform mat4 u_worldMatrix;
uniform mat4 u_projectionMatrix;

VARYING_OUT vec2 v_texCoord;

void main()
{
    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
    v_texCoord = a_texCoord0;
}
)";

inline const std::string BlurFrag =
R"(
uniform sampler2D u_texture;

VARYING_IN vec2 v_texCoord;
#include RESOLUTION_BUFFER

OUTPUT

const int size = 8;
const float separation = 0.5; //TODO use depth to control this?

void main()
{
    //FRAG_OUT = TEXTURE(u_texture, v_texCoord);

    float count = 0.0;
    vec2 pixelUV = vec2(1.0) / u_bufferResolution;//textureSize(u_texture, 0);
    vec3 colour = vec3(0.0);

    for (int y = -size; y <= size; ++y)
    {
        for (int x = -size; x <= size; ++x)
        {
            colour += texture(u_texture, (v_texCoord + (vec2(x, y) * pixelUV * separation))).rgb;
            count++;
        }
    }

    colour /= count;
    FRAG_OUT = vec4(colour, 1.0);
}
)";

inline const std::string CompositeVert = 
R"(
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


inline const std::string CompositeFrag = 
R"(
uniform sampler2D u_texture;
uniform sampler2D u_depthTexture;
uniform sampler2D u_lightTexture;
uniform sampler2D u_blurTexture;
uniform sampler2D u_maskTexture;

uniform vec4 u_lightColour;

//uniform float u_brightness = 1.1;
//uniform float u_contrast = 2.0;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

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


void main()
{
    vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;
    colour.rgb = dim(colour.rgb);

    float depthSample = TEXTURE(u_depthTexture, v_texCoord).r;
    float d = getDistance(depthSample);
    float fogMix = fogAmount(d);

#if defined (LIGHT_COLOUR)
    float maskAmount = TEXTURE(u_maskTexture, v_texCoord).a;
    float fogAddition = (0.95 * maskAmount) + 0.05;

    fogMix *= fogAddition;
#endif

    colour = mix(colour, FogColour * u_lightColour, fogMix);

#if defined(LIGHT_COLOUR)
    vec3 lightColour = TEXTURE(u_lightTexture, v_texCoord).rgb;
    colour.rgb += lightColour;

    maskAmount = (0.9 * maskAmount) + 0.1;

    vec3 blurColour = TEXTURE(u_blurTexture, v_texCoord).rgb * (1.0 - ((d * 0.9) + 0.1));
    colour.rgb = (blurColour * maskAmount * 0.65) + colour.rgb;
#endif

    FRAG_OUT = colour;
    //FRAG_OUT = mix(colour, vec4(d,d,d,1.0), u_density / 10.0);
})";