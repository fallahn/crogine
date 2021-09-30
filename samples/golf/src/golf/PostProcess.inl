/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine application - Zlib license.

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

const std::string PostVertex =
R"(
        ATTRIBUTE vec2 a_position;
        ATTRIBUTE vec2 a_texCoord0;
        ATTRIBUTE vec4 a_colour;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_projectionMatrix;

        VARYING_OUT vec2 v_texCoord;
        VARYING_OUT LOW vec4 v_colour;

        void main()
        {
            gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_texCoord = a_texCoord0;
            v_colour = a_colour;
        })";

const std::string PostFragment =
R"(
uniform sampler2D u_texture;
uniform vec2 u_resolution; //must be normalised before setting!

VARYING_IN vec2 v_texCoord;
VARYING_IN LOW vec4 v_colour;

OUTPUT


//
// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER
//
//   by Timothy Lottes
//
// This is more along the style of a really good CGA arcade monitor.
// With RGB inputs instead of NTSC.
// The shadow mask example has the mask rotated 90 degrees for less chromatic aberration.
//
// Left it unoptimized to show the theory behind the algorithm.
//
// It is an example what I personally would want as a display option for pixel art games.
// Please take and use, change, or whatever.
//


//hardness of scanline.
//-8.0 = soft
//-16.0 = medium
const float hardScan = -8.0;

//hardness of pixels in scanline.
//-2.0 = soft
//-4.0 = hard
const float hardPix = -3.0;

//amount of shadow mask.
const float maskDark = 0.5;
const float maskLight = 1.5;

//------------------------------------------------------------------------

//sRGB to Linear.
//assmuing using sRGB typed textures this should not be needed.
float ToLinear1(float c)
{
  return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}


vec3 ToLinear(vec3 c)
{
  return vec3(ToLinear1(c.r), ToLinear1(c.g), ToLinear1(c.b));
}

//linear to sRGB.
//assuming using sRGB typed textures this should not be needed.
float ToSrgb1(float c)
{
  return(c < 0.0031308 ? c * 12.92 : 1.055 * pow(c, 0.41666) - 0.055);
}

vec3 ToSrgb(vec3 c)
{
  return vec3(ToSrgb1(c.r), ToSrgb1(c.g), ToSrgb1(c.b));
}

//nearest emulated sample given floating point position and texel offset.
//also zeroes off screen.
vec3 Fetch(vec2 pos, vec2 off)
{
  pos = floor(pos * u_resolution + off) / u_resolution;
  if(max(abs(pos.x - 0.5), abs(pos.y - 0.5)) > 0.5)
  {
    return vec3(0.0, 0.0, 0.0);
  }
  return ToLinear(TEXTURE(u_texture, pos.xy, -16.0).rgb);
}

//distance in emulated pixels to nearest texel.
vec2 Dist(vec2 pos)
{
  pos = pos * u_resolution;
  return -((pos - floor(pos)) - vec2(0.5));
}
    
//1D Gaussian.
float Gaus(float pos,float scale)
{
  return exp2(scale * pos * pos);
}

//3-tap Gaussian filter along horz line.
vec3 Horz3(vec2 pos,float off)
{
  vec3 b = Fetch(pos, vec2(-1.0, off));
  vec3 c = Fetch(pos, vec2( 0.0,off));
  vec3 d = Fetch(pos,vec2(1.0, off));
  float dst = Dist(pos).x;

  //convert distance to weight.
  float scale = hardPix;
  float wb = Gaus(dst - 1.0,scale);
  float wc = Gaus(dst + 0.0,scale);
  float wd = Gaus(dst + 1.0,scale);

  //return filtered sample.
  return (b * wb + c * wc + d * wd) / (wb + wc + wd);
}

//5-tap Gaussian filter along horz line.
vec3 Horz5(vec2 pos, float off)
{
  vec3 a = Fetch(pos, vec2(-2.0,off));
  vec3 b = Fetch(pos, vec2(-1.0,off));
  vec3 c = Fetch(pos, vec2( 0.0,off));
  vec3 d = Fetch(pos, vec2( 1.0,off));
  vec3 e = Fetch(pos, vec2( 2.0,off));
  float dst = Dist(pos).x;

  //convert distance to weight.
  float scale = hardPix;
  float wa = Gaus(dst - 2.0, scale);
  float wb = Gaus(dst - 1.0, scale);
  float wc = Gaus(dst + 0.0, scale);
  float wd = Gaus(dst + 1.0, scale);
  float we = Gaus(dst + 2.0, scale);

  //return filtered sample.
  return (a * wa + b * wb + c * wc + d * wd + e * we) / (wa + wb + wc + wd + we);
}

//return scanline weight.
float Scan(vec2 pos,float off)
{
  float dst = Dist(pos).y;
  return Gaus(dst + off, hardScan);
}

//allow nearest three lines to effect pixel.
vec3 Tri(vec2 pos)
{
  vec3 a = Horz3(pos, -1.0);
  vec3 b = Horz5(pos, 0.0);
  vec3 c = Horz3(pos, 1.0);
  float wa = Scan(pos, -1.0);
  float wb = Scan(pos, 0.0);
  float wc = Scan(pos, 1.0);
  return a * wa + b * wb + c * wc;
}

//shadow mask.
vec3 Mask(vec2 pos)
{
  pos.x += pos.y * 3.0;
  vec3 mask = vec3(maskDark, maskDark, maskDark);
  pos.x = fract(pos.x / 6.0);
  if(pos.x < 0.333)
  {
    mask.r = maskLight;
  }
  else if(pos.x < 0.666)
  {
    mask.g = maskLight;
  }
  else 
  {
    mask.b = maskLight;
  }
  return mask;
}    


void main()
{
  FRAG_OUT.rgb = Tri(v_texCoord) * Mask(gl_FragCoord.xy);
  FRAG_OUT.a = 1.0;  
  FRAG_OUT.rgb = ToSrgb(FRAG_OUT.rgb);
  FRAG_OUT *= v_colour;
})";

static const std::string FXAAFrag = R"(

uniform sampler2D u_texture; 

uniform vec2 u_texelStep = vec2(1.0) / vec2(1920.0, 1080.0); //normalised output resolution ie vec2(1.0) / res

uniform float u_lumaThreshold = 0.5; //0.0 - 1.0
uniform float u_mulReduce = 1.0 / 8.0; //1.0 / 1 - 256
uniform float u_minReduce = 1.0 / 128.0; //1.0 / 1 - 512
uniform float u_maxSpan = 8.0; //1.0 - 16.0

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;

OUTPUT

// see FXAA
// http://developer.download.nvidia.com/assets/gamedev/files/sdk/11/FXAA_WhitePaper.pdf
// http://iryoku.com/aacourse/downloads/09-FXAA-3.11-in-15-Slides.pdf
// http://horde3d.org/wiki/index.php5?title=Shading_Technique_-_FXAA
// https://github.com/McNopper/OpenGL/blob/master/Example42/shader/fxaa.frag.glsl

void main(void)
{
    vec3 rgbM = TEXTURE(u_texture, v_texCoord).rgb;

    vec3 rgbNW = textureOffset(u_texture, v_texCoord, ivec2(-1, 1)).rgb;
    vec3 rgbNE = textureOffset(u_texture, v_texCoord, ivec2(1, 1)).rgb;
    vec3 rgbSW = textureOffset(u_texture, v_texCoord, ivec2(-1, -1)).rgb;
    vec3 rgbSE = textureOffset(u_texture, v_texCoord, ivec2(1, -1)).rgb;

    // see http://en.wikipedia.org/wiki/Grayscale
    const vec3 toLuma = vec3(0.299, 0.587, 0.114);
    
    float lumaNW = dot(rgbNW, toLuma);
    float lumaNE = dot(rgbNE, toLuma);
    float lumaSW = dot(rgbSW, toLuma);
    float lumaSE = dot(rgbSE, toLuma);
    float lumaM = dot(rgbM, toLuma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    
    if (lumaMax - lumaMin <= lumaMax * u_lumaThreshold)
    {
        FRAG_OUT = vec4(rgbM, 1.0);
        return;
    }  
    
    vec2 samplingDirection = vec2(0.0); 
    samplingDirection.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    samplingDirection.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    
    float samplingDirectionReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * 0.25 * u_mulReduce, u_minReduce);

    float minSamplingDirectionFactor = 1.0 / (min(abs(samplingDirection.x), abs(samplingDirection.y)) + samplingDirectionReduce);
    
    samplingDirection = clamp(samplingDirection * minSamplingDirectionFactor, vec2(-u_maxSpan), vec2(u_maxSpan)) * u_texelStep;
    
    vec3 rgbSampleNeg = TEXTURE(u_texture, v_texCoord + samplingDirection * (1.0/3.0 - 0.5)).rgb;
    vec3 rgbSamplePos = TEXTURE(u_texture, v_texCoord + samplingDirection * (2.0/3.0 - 0.5)).rgb;

    vec3 rgbTwoTab = (rgbSamplePos + rgbSampleNeg) * 0.5;  

    vec3 rgbSampleNegOuter = TEXTURE(u_texture, v_texCoord + samplingDirection * (0.0/3.0 - 0.5)).rgb;
    vec3 rgbSamplePosOuter = TEXTURE(u_texture, v_texCoord + samplingDirection * (3.0/3.0 - 0.5)).rgb;
    
    vec3 rgbFourTab = (rgbSamplePosOuter + rgbSampleNegOuter) * 0.25 + rgbTwoTab * 0.5;   
    
    float lumaFourTab = dot(rgbFourTab, toLuma);
    
    if (lumaFourTab < lumaMin || lumaFourTab > lumaMax)
    {
        FRAG_OUT = vec4(rgbTwoTab, 1.0); 
    }
    else
    {
        FRAG_OUT = vec4(rgbFourTab, 1.0);
    }

    FRAG_OUT *= v_colour;
})";