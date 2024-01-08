/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2022
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

inline const std::string PostVertex =
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

inline const std::string NoiseFragment =
R"(
    uniform sampler2D u_texture;

#include WIND_BUFFER
#include SCALE_BUFFER

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    OUTPUT

    float rand(vec2 position)
    {
        return fract(sin(dot(position, vec2(12.9898, 4.1414)) + u_windData.w) * 43758.5453);
    }

    void main()
    {
        FRAG_OUT = vec4(vec3(rand(floor((v_texCoord * textureSize(u_texture, 0)) / u_pixelScale))), 1.0) * v_colour; 
    }
)";

inline const std::string BWFragment =
R"(
    uniform sampler2D u_texture;
    uniform float u_time = 1.0;
    uniform vec2 u_scale = vec2(1.0);

    VARYING_IN vec2 v_texCoord;
    VARYING_IN vec4 v_colour;

    OUTPUT

    float rand(vec2 pos)
    {
        return fract(sin(dot(pos, vec2(12.9898, 4.1414) + (u_time * 0.1))) * 43758.5453);
    }

    void main()
    {
        vec4 colour = TEXTURE(u_texture, v_texCoord) * v_colour;
        float grey = dot(colour.rgb, vec3(0.299, 0.587, 0.114));

        float noise = rand(floor(gl_FragCoord.xy / u_scale));
        FRAG_OUT = vec4(mix(vec3(grey), vec3(noise), 0.07), colour.a); 
    })";

inline const std::string TerminalFragment =
R"(
    uniform sampler2D u_texture;
    uniform float u_time = 1.0;

    VARYING_IN vec2 v_texCoord;
    VARYING_IN LOW vec4 v_colour;

    OUTPUT

    const float lineSpacing = 2.0;

    float rand(vec2 pos)
    {
        return fract(sin(dot(pos, vec2(12.9898, 4.1414) + (u_time * 0.1))) * 43758.5453);
    }

    const vec2 curvature = vec2(10.0);
    vec2 curve(vec2 coord)
    {
        vec2 uv = coord;
        uv = uv * 2.0 - 1.0;

        vec2 offset = abs(uv.yx) / curvature.xy;
        uv = uv + uv * offset * offset;

        uv = uv * 0.5 + 0.5;
        return uv;
    }

    void main()
    {
        float glitchOffset = 0.05 * smoothstep(0.36, 0.998, sin(v_texCoord.y * 180.0));
        glitchOffset *= round(sin(v_texCoord.y * 90.0));

        float band = smoothstep(0.9951, 0.9992, sin((v_texCoord.y * 4.0) + (u_time * 0.2))) * 0.06;

        float glitchAmount = step(0.997, rand(vec2(u_time * 0.1))) * 0.08;
#if defined (DISTORTION)
        vec2 texCoord = curve(v_texCoord);
#else
        vec2 texCoord = v_texCoord;
#endif
        texCoord.x += 0.03 * glitchAmount; //shake
        texCoord.x += glitchOffset * glitchAmount; //tearing
        texCoord.xy += band * 0.012; //scrolling band

        vec3 noise = vec3(rand(floor((gl_FragCoord.xy + 17.034765))), 
                        1.0 - rand(floor((gl_FragCoord.xy))), 
                        rand(floor((gl_FragCoord.xy + 0.145))));
        noise *= (0.1 + (0.4 * band));

#if defined (DISTORTION)
        vec4 colour = vec4(1.0);
        colour.r = TEXTURE(u_texture, texCoord * 0.998).r;
        colour.g = TEXTURE(u_texture, texCoord).g;
        colour.b = TEXTURE(u_texture, vec2(0.002) + (texCoord * 0.998)).b;
#else
        vec4 colour = TEXTURE(u_texture, texCoord);
#endif
        colour.rgb += noise;

        colour.g += band * 0.1;
        colour.rgb *= 0.3 + (clamp(mod(gl_FragCoord.y, lineSpacing), 0.0, 1.0) * 0.7);

        FRAG_OUT = colour * v_colour;
    })";


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

inline const std::string CRTFragment =
R"(
    uniform sampler2D u_texture;
    uniform vec2 u_resolution;

    VARYING_IN vec2 v_texCoord;
    VARYING_IN LOW vec4 v_colour;

    OUTPUT

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

inline const std::string CinematicFragment =
R"(
uniform sampler2D u_texture;
uniform float u_time = 1.0;
uniform vec2 u_scale = vec2(1.0);
uniform vec2 u_resolution = vec2(1.0);

in vec2 v_texCoord;
in vec4 v_colour;

out vec4 FRAG_OUT;


//brightness contrast saturation : https://www.shadertoy.com/view/XdcXzn
mat4 brightness(float bri)
{
    return mat4(1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                bri, bri, bri, 1.0);
}

mat4 contrast(float con)
{
    float t = (1.0 - con) / 2.0;
    
    return mat4(con, 0.0, 0.0, 0.0,
                0.0, con, 0.0, 0.0,
                0.0, 0.0, con, 0.0,
                t,   t,   t,   1.0);

}

mat4 saturation(float sat)
{
    vec3 luminance = vec3(0.3086, 0.6094, 0.0820);
    
    float oneMinusSat = 1.0 - sat;
    
    vec3 red = vec3(luminance.x * oneMinusSat);
    red+= vec3(sat, 0.0, 0.0);
    
    vec3 green = vec3(luminance.y * oneMinusSat);
    green += vec3(0.0, sat, 0.0);
    
    vec3 blue = vec3(luminance.z * oneMinusSat);
    blue += vec3(0.0, 0.0, sat);
    
    return mat4(red,      0.0,
                green,    0.0,
                blue,     0.0,
                0.0, 0.0, 0.0, 1.0);
}


float noise(float offset)
{
    return fract(sin(dot(v_texCoord, vec2(12.9898,78.233) * 2.0) + u_time + offset) * 43758.5453);
}

//https://www.shadertoy.com/view/llGSz3
const float kernel = 0.5;
const float weight = 1.0;
vec4 hBlur()
{
    float pixelSize = 1.0 / u_resolution.x;
    
    vec4 accumulation = vec4(0.0);
    vec4 weightsum = vec4(0.0);

    for (float i = -kernel; i <= kernel; i++)
    {
        accumulation += TEXTURE(u_texture, v_texCoord + vec2(i * pixelSize, 0.0)) * weight;
        weightsum += weight;
    }
    
    return accumulation / weightsum;
}


void main()
{
    //vec4 colour = TEXTURE(u_texture, v_texCoord);
    vec4 colour = hBlur();
    
    //contrast / saturation
    colour = brightness(0.05) * contrast(1.2) * saturation(0.9) * colour;

    //noise
    float noiseAmount = 0.11;
    colour.r -= noise(colour.b) * noiseAmount;
    colour.g -= noise(colour.r) * noiseAmount;
    colour.b -= noise(colour.g) * noiseAmount;


    //vignette
    vec2 uv = v_texCoord * (vec2(1.0) - v_texCoord.yx);
    
    float vignette = uv.x * uv.y * 25.0;
    vignette = pow(vignette, 0.15);
    vignette = 0.5 + (0.5 * vignette);

    colour.rgb *= vignette;


    FRAG_OUT = vec4(colour.rgb, 1.0); 
})";