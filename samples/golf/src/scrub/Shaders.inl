/*-----------------------------------------------------------------------

Matt Marchant 2024
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

static const inline std::string LevelMeterFragment =
R"(
uniform sampler2D u_texture;
uniform vec4 u_uvRect;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

const float DistortionAmount = 0.04;
const float HighlightAmount = 0.4;

void main()
{
    vec2 coord = v_texCoord;

    //add distortion offset - using the colour accessors here because I can't find
    //a definitive answer whether the order is wxyz or xyzw
    float u = ((coord.x - u_uvRect.r) / u_uvRect.b);
    
    float highlight = smoothstep(0.55, 0.75, u);
    highlight *= 1.0 - smoothstep(0.78, 0.80, u);

    u = pow(((u * 2.0) - 1.0), 3.0);
    coord.x += u * DistortionAmount;

    FRAG_OUT = mix(TEXTURE(u_texture, coord), v_colour, 0.25 + (abs(u) * 0.3)) + vec4(highlight * HighlightAmount);
})";

static const inline std::string SoapFragment =
R"(
uniform sampler2D u_texture; //background texture
uniform sampler2D u_bubbleTexture; //metaballs.
uniform vec4 u_uvRect;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

//crummy fake specular
const vec3 LightDir = vec3(0.5);

void main()
{
    vec2 bgCoord = v_texCoord;
    vec2 soapCoord = vec2((v_texCoord.x - u_uvRect.r) / u_uvRect.b, (v_texCoord.y - u_uvRect.g) / u_uvRect.a);

    vec3 soapMask = TEXTURE(u_bubbleTexture, soapCoord).rgb;
    
    float soapMix = soapMask.b;
    soapMix = smoothstep(0.3, 0.39, soapMix);

    vec2 soapOffset = normalize(soapMask.rg * 2.0 - 1.0);

    vec3 reallyFakeNormal = normalize(vec3(soapOffset, 1.0 - length(soapOffset)));
    float specularAngle = clamp(dot(reallyFakeNormal, normalize(LightDir)), 0.0, 1.0);
    vec3 specularColour = vec3(pow(specularAngle, 64.0)) * 2.0;

    soapOffset *= 0.025;

    vec4 bgColour = TEXTURE(u_texture, bgCoord);
    vec4 soapColour = mix(TEXTURE(u_texture, bgCoord + soapOffset), v_colour, 0.25) + vec4(specularColour, 0.0);

    FRAG_OUT = mix(bgColour, soapColour, soapMix);
    FRAG_OUT.a = soapMix; //hm if we do *this* then we don't really need to be sampling the non-offset texture?
})";

/*
https://blog.febucci.com/2019/05/fire-shader/
*/
static const inline std::string FireFragment =
R"(
uniform sampler2D u_texture;
uniform float u_time = 0.0;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

const vec4 brighterColour = vec4(0.316, 0.766, 1.0, 0.25);
const vec4 darkerColour = vec4(0.1, 0.309, 0.687, 0.0625);

//const vec4 brighterColour = vec4(1.0, 0.65, 0.1, 0.25);
//const vec4 darkerColour = vec4(1.0, 0.0, 0.15, 0.0625);

void main()
{
	vec2 uv = v_texCoord;
    float gradient = 1.0 - uv.y;
    float gradientStep = 0.2;

    vec2 pos = v_texCoord * 0.5;
    pos.y -= u_time * 0.03125;
    
    vec4 middleColour = mix(brighterColour, darkerColour, 0.5);

    float noiseTexel = TEXTURE(u_texture, pos).r;
    
    float firstStep = smoothstep(0.0, noiseTexel, gradient);
    float darkerColourStep = smoothstep(0.0, noiseTexel, gradient - gradientStep);
    float darkerColourPath = firstStep - darkerColourStep;
    vec4 colour = mix(darkerColour, brighterColour, darkerColourPath);

    float middleColourStep = smoothstep(0.0, noiseTexel, gradient - 0.2 * 2.0);
    
    colour = mix(colour, middleColour, darkerColourStep - middleColourStep);
    colour = mix(vec4(0.0), colour, firstStep) * 0.75;
//colour.rgb *= colour.a;
    colour.a = 1.0;

	FRAG_OUT = colour * v_colour;// * TEXTURE(u_texture, v_texCoord);
})";