/**
 * Copyright (C) 2013 Jorge Jimenez (jorge@iryoku.com)
 * Copyright (C) 2013 Jose I. Echevarria (joseignacioechevarria@gmail.com)
 * Copyright (C) 2013 Belen Masia (bmasia@unizar.es)
 * Copyright (C) 2013 Fernando Navarro (fernandn@microsoft.com)
 * Copyright (C) 2013 Diego Gutierrez (diegog@unizar.es)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software. As clarification, there
 * is no requirement that the copyright notice and permission be included in
 * binary distributions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Includes OpenGL coordinate changes from mbechard
 */

#pragma once

#include <string>

namespace SMAA::Weights
{
    /**
     * SMAA_MAX_SEARCH_STEPS specifies the maximum steps performed in the
     * horizontal/vertical pattern searches, at each side of the pixel.
     *
     * In number of pixels, it's actually the double. So the maximum line length
     * perfectly handled by, for example 16, is 64 (by perfectly, we meant that
     * longer lines won't look as good, but still antialiased).
     *
     * Range: [0, 112]
     */
    static inline const std::string Vert =
R"(
#define mad(a, b, c) (a * b + c)

#if defined(SMAA_PRESET_LOW)
#define SMAA_MAX_SEARCH_STEPS 4
#elif defined(SMAA_PRESET_MEDIUM)
#define SMAA_MAX_SEARCH_STEPS 8
#elif defined(SMAA_PRESET_HIGH)
#define SMAA_MAX_SEARCH_STEPS 16
#elif defined(SMAA_PRESET_ULTRA)
#define SMAA_MAX_SEARCH_STEPS 32
#endif

#ifndef SMAA_MAX_SEARCH_STEPS
#define SMAA_MAX_SEARCH_STEPS 16
#endif

ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;
ATTRIBUTE vec4 a_colour;

uniform mat4 u_worldMatrix;
uniform mat4 u_projectionMatrix;
uniform vec2 u_resolution = vec2(800.0, 600.0);

VARYING_OUT vec2 v_texCoord;
VARYING_OUT vec2 v_pixCoord;
VARYING_OUT vec4 v_colour;
VARYING_OUT vec4 v_offset[3];

void main()
{
    v_colour = a_colour;
    v_texCoord = a_texCoord0;
    v_pixCoord = a_texCoord0 * u_resolution;

    vec4 SMAA_RT_METRICS = vec4(1.0 / u_resolution.x, 1.0 / u_resolution.y, u_resolution.x, u_resolution.y);

    v_offset[0] = mad(SMAA_RT_METRICS.xyxy, vec4(-0.25, 0.125,  1.25, 0.125), v_texCoord.xyxy);
    v_offset[1] = mad(SMAA_RT_METRICS.xyxy, vec4(-0.125, 0.25, -0.125,  -1.25), v_texCoord.xyxy);

    v_offset[2] = mad(
        SMAA_RT_METRICS.xxyy,
        vec4(-2.0, 2.0, 2.0, -2.0) * float(SMAA_MAX_SEARCH_STEPS),
        vec4(v_offset[0].xz, v_offset[1].yw)
    );

    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
})";

    /**
     * SMAA_THRESHOLD specifies the threshold or sensitivity to edges.
     * Lowering this value you will be able to detect more edges at the expense of
     * performance.
     *
     * Range: [0, 0.5]
     *   0.1 is a reasonable value, and allows to catch most visible edges.
     *   0.05 is a rather overkill value, that allows to catch 'em all.
     *
     *   If temporal supersampling is used, 0.2 could be a reasonable value, as low
     *   contrast edges are properly filtered by just 2x.
     * 
     * ------------------------------------------------------------------
     * SMAA_MAX_SEARCH_STEPS_DIAG specifies the maximum steps performed in the
     * diagonal pattern searches, at each side of the pixel. In this case we jump
     * one pixel at time, instead of two.
     *
     * Range: [0, 20]
     *
     * On high-end machines it is cheap (between a 0.8x and 0.9x slower for 16
     * steps), but it can have a significant impact on older machines.
     *
     * Define SMAA_DISABLE_DIAG_DETECTION to disable diagonal processing.
     *
     *-------------------------------------------------------------------
     * SMAA_CORNER_ROUNDING specifies how much sharp corners will be rounded.
     *
     * Range: [0, 100]
     *
     * Define SMAA_DISABLE_CORNER_DETECTION to disable corner processing.
     */

    static inline const std::string Frag =
R"(
#if defined(SMAA_PRESET_LOW)
#define SMAA_THRESHOLD 0.15
#define SMAA_MAX_SEARCH_STEPS 4
#define SMAA_DISABLE_DIAG_DETECTION
#define SMAA_DISABLE_CORNER_DETECTION
#elif defined(SMAA_PRESET_MEDIUM)
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 8
#define SMAA_DISABLE_DIAG_DETECTION
#define SMAA_DISABLE_CORNER_DETECTION
#elif defined(SMAA_PRESET_HIGH)
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 16
#define SMAA_MAX_SEARCH_STEPS_DIAG 8
#define SMAA_CORNER_ROUNDING 25
#elif defined(SMAA_PRESET_ULTRA)
#define SMAA_THRESHOLD 0.05
#define SMAA_MAX_SEARCH_STEPS 32
#define SMAA_MAX_SEARCH_STEPS_DIAG 16
#define SMAA_CORNER_ROUNDING 25
#endif

//Defaults
#ifndef SMAA_THRESHOLD
#define SMAA_THRESHOLD 0.1
#endif
#ifndef SMAA_MAX_SEARCH_STEPS
#define SMAA_MAX_SEARCH_STEPS 16
#endif
#ifndef SMAA_MAX_SEARCH_STEPS_DIAG
#define SMAA_MAX_SEARCH_STEPS_DIAG 8
#endif
#ifndef SMAA_CORNER_ROUNDING
#define SMAA_CORNER_ROUNDING 25
#endif

#line 144

//Non-Configurable Defines
#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_PIXEL_SIZE (1.0 / vec2(160.0, 560.0))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)
#define SMAA_SEARCHTEX_SIZE vec2(66.0, 33.0)
#define SMAA_SEARCHTEX_PACKED_SIZE vec2(64.0, 16.0)
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)

uniform sampler2D u_texture;
uniform sampler2D u_areaTexture;
uniform sampler2D u_searchTexture;

uniform vec2 u_resolution = vec2(800.0, 600.0);

VARYING_IN vec2 v_texCoord;
VARYING_IN vec2 v_pixCoord;
VARYING_IN vec4 v_offset[3];
VARYING_IN vec4 v_colour;

vec4 SMAA_RT_METRICS = vec4(1.0 / u_resolution.x, 1.0 / u_resolution.y, u_resolution.x, u_resolution.y);

#define mad(a, b, c) (a * b + c)
#define saturate(a) clamp(a, 0.0, 1.0)
#define round(v) floor(v + 0.5)

#define SMAASampleLevelZero(tex, coord) textureLod(tex, coord, 0.0)
#define SMAASampleLevelZeroPoint(tex, coord) textureLod(tex, coord, 0.0)
#define SMAASampleLevelZeroOffset(tex, coord, offset) textureLodOffset(tex, coord, 0.0, offset)

void SMAAMovc(bvec2 cond, inout vec2 variable, vec2 value)
{
    if (cond.x) variable.x = value.x;
    if (cond.y) variable.y = value.y;
}

void SMAAMovc(bvec4 cond, inout vec4 variable, vec4 value)
{
    SMAAMovc(cond.xy, variable.xy, value.xy);
    SMAAMovc(cond.zw, variable.zw, value.zw);
}

vec2 SMAADecodeDiagBilinearAccess(vec2 e)
{
    e.r = e.r * abs(5.0 * e.r - 5.0 * 0.75);
    return round(e);
}

vec4 SMAADecodeDiagBilinearAccess(vec4 e)
{
    e.rb = e.rb * abs(5.0 * e.rb - 5.0 * 0.75);
    return round(e);
}

vec2 SMAASearchDiag1(vec2 texcoord, vec2 dir, out vec2 e)
{
    dir.y = -dir.y;
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    vec3 t = vec3(SMAA_RT_METRICS.xy, 1.0);

    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) &&
           coord.w > 0.9)
    {
        coord.xyz = mad(t, vec3(dir, 1.0), coord.xyz);
        e = SMAASampleLevelZero(u_texture, coord.xy).rg;
        coord.w = dot(e, vec2(0.5, 0.5));
    }
    return coord.zw;
}

vec2 SMAASearchDiag2(vec2 texcoord, vec2 dir, out vec2 e)
{
    dir.y = -dir.y;
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    coord.x += 0.25 * SMAA_RT_METRICS.x;
    vec3 t = vec3(SMAA_RT_METRICS.xy, 1.0);

    while (coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) &&
               coord.w > 0.9)
    {
        coord.xyz = mad(t, vec3(dir, 1.0), coord.xyz);

        // @SearchDiag2Optimization
        // Fetch both edges at once using bilinear filtering:
        e = SMAASampleLevelZero(u_texture, coord.xy).rg;
        e = SMAADecodeDiagBilinearAccess(e);

        coord.w = dot(e, vec2(0.5, 0.5));
    }
    return coord.zw;
}

vec2 SMAAAreaDiag(vec2 dist, vec2 e, float offset)
{
    vec2 texcoord = mad(vec2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);
    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);
    texcoord.x += 0.5;
    texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;
    texcoord.y = 1.0 - texcoord.y;

    return SMAASampleLevelZero(u_areaTexture, texcoord).rg;
}

vec2 SMAACalculateDiagWeights(vec2 texcoord, vec2 e, vec4 subsampleIndices)
{
    vec2 weights = vec2(0.0, 0.0);

    vec4 d = vec4(0.0);
    vec2 end = vec2(0.0);
    if (e.r > 0.0)
    {
        d.xz = SMAASearchDiag1(texcoord, vec2(-1.0,  1.0), end);
        d.x += float(end.y > 0.9);
    } 
    else
    {
        d.xz = vec2(0.0, 0.0);
    }
    d.yw = SMAASearchDiag1(texcoord, vec2(1.0, -1.0), end);

    if (d.x + d.y > 2.0) 
    {
        //Fetch the crossing edges:
        vec4 coords = mad(vec4(-d.x + 0.25, -d.x, d.y, d.y - 0.25), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
        vec4 c = vec4(0.0);
        c.xy = SMAASampleLevelZeroOffset(u_texture, coords.xy, ivec2(-1,  0)).rg;
        c.zw = SMAASampleLevelZeroOffset(u_texture, coords.zw, ivec2( 1,  0)).rg;
        c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);

        vec2 cc = mad(vec2(2.0, 2.0), c.xz, c.yw);

        SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

        weights += SMAAAreaDiag(d.xy, cc, subsampleIndices.z);
    }

    d.xz = SMAASearchDiag2(texcoord, vec2(-1.0, -1.0), end);
    if (SMAASampleLevelZeroOffset(u_texture, texcoord, ivec2(1, 0)).r > 0.0)
    {
        d.yw = SMAASearchDiag2(texcoord, vec2(1.0, 1.0), end);
        d.y += float(end.y > 0.9);
    } 
    else
    {
        d.yw = vec2(0.0, 0.0);
    }

    if (d.x + d.y > 2.0)
    {
        //Fetch the crossing edges:
        vec4 coords = mad(vec4(-d.x, d.x, d.y, -d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
        vec4 c = vec4(0.0);
        c.x  = SMAASampleLevelZeroOffset(u_texture, coords.xy, ivec2(-1,  0)).g;
        c.y  = SMAASampleLevelZeroOffset(u_texture, coords.xy, ivec2( 0,  1)).r;
        c.zw = SMAASampleLevelZeroOffset(u_texture, coords.zw, ivec2( 1,  0)).gr;
        vec2 cc = mad(vec2(2.0, 2.0), c.xz, c.yw);

        SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

        weights += SMAAAreaDiag(d.xy, cc, subsampleIndices.w).gr;
    }
    return weights;
}

float SMAASearchLength(vec2 e, float offset)
{
    vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
    vec2 bias = SMAA_SEARCHTEX_SIZE * vec2(offset, 1.0);

    scale += vec2(-1.0,  1.0);
    bias  += vec2( 0.5, -0.5);

    scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;

    vec2 coord = mad(scale, e, bias);
    coord.y = 1.0 - coord.y;
    return SMAASampleLevelZero(u_searchTexture, coord).r;
}

float SMAASearchXLeft(vec2 texcoord, float end)
{
    vec2 e = vec2(0.0, 1.0);

    while (texcoord.x > end && 
           e.g > 0.8281 &&
           e.r == 0.0)
    {
        e = SMAASampleLevelZero(u_texture, texcoord).rg;
        texcoord = mad(-vec2(2.0, 0.0), SMAA_RT_METRICS.xy, texcoord);
    }

    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e, 0.0), 3.25);
    return mad(SMAA_RT_METRICS.x, offset, texcoord.x);
}

float SMAASearchXRight(vec2 texcoord, float end)
{
    vec2 e = vec2(0.0, 1.0);
    while (texcoord.x < end && 
           e.g > 0.8281 &&
           e.r == 0.0)
    { 
        e = SMAASampleLevelZero(u_texture, texcoord).rg;
        texcoord = mad(vec2(2.0, 0.0), SMAA_RT_METRICS.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e, 0.5), 3.25);
    return mad(-SMAA_RT_METRICS.x, offset, texcoord.x);
}

float SMAASearchYUp(vec2 texcoord, float end)
{
    vec2 e = vec2(1.0, 0.0);

    while (texcoord.y < end && 
           e.r > 0.8281 &&
           e.g == 0.0)
    {
        e = SMAASampleLevelZero(u_texture, texcoord).rg;
        texcoord = mad(-vec2(0.0, -2.0), SMAA_RT_METRICS.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e.gr, 0.0), 3.25);
    return mad(SMAA_RT_METRICS.y, -offset, texcoord.y);
}

float SMAASearchYDown(vec2 texcoord, float end)
{
    vec2 e = vec2(1.0, 0.0);
    while (texcoord.y > end && 
           e.r > 0.8281 &&
           e.g == 0.0)
    {
        e = SMAASampleLevelZero(u_texture, texcoord).rg;
        texcoord = mad(vec2(0.0, -2.0), SMAA_RT_METRICS.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(e.gr, 0.5), 3.25);
    return mad(-SMAA_RT_METRICS.y, -offset, texcoord.y);
}

vec2 SMAAArea(vec2 dist, float e1, float e2, float offset)
{
    vec2 texcoord = mad(vec2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE), round(4.0 * vec2(e1, e2)), dist);
    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);
    texcoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, offset, texcoord.y);
    texcoord.y = 1.0 - texcoord.y;

    return SMAASampleLevelZero(u_areaTexture, texcoord).rg;
}

void SMAADetectHorizontalCornerPattern(inout vec2 weights, vec4 texcoord, vec2 d)
{
    #if !defined(SMAA_DISABLE_CORNER_DETECTION)
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;

    rounding /= leftRight.x + leftRight.y;

    vec2 factor = vec2(1.0, 1.0);
    factor.x -= rounding.x * SMAASampleLevelZeroOffset(u_texture, texcoord.xy, ivec2(0, -1)).r;
    factor.x -= rounding.y * SMAASampleLevelZeroOffset(u_texture, texcoord.zw, ivec2(1, -1)).r;
    factor.y -= rounding.x * SMAASampleLevelZeroOffset(u_texture, texcoord.xy, ivec2(0,  2)).r;
    factor.y -= rounding.y * SMAASampleLevelZeroOffset(u_texture, texcoord.zw, ivec2(1,  2)).r;

    weights *= saturate(factor);
    #endif
}

void SMAADetectVerticalCornerPattern(inout vec2 weights, vec4 texcoord, vec2 d)
{
    #if !defined(SMAA_DISABLE_CORNER_DETECTION)
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;

    rounding /= leftRight.x + leftRight.y;

    vec2 factor = vec2(1.0, 1.0);
    factor.x -= rounding.x * SMAASampleLevelZeroOffset(u_texture, texcoord.xy, ivec2( 1,  0)).g;
    factor.x -= rounding.y * SMAASampleLevelZeroOffset(u_texture, texcoord.zw, ivec2( 1, -1)).g;
    factor.y -= rounding.x * SMAASampleLevelZeroOffset(u_texture, texcoord.xy, ivec2(-2,  0)).g;
    factor.y -= rounding.y * SMAASampleLevelZeroOffset(u_texture, texcoord.zw, ivec2(-2, -1)).g;

    weights *= saturate(factor);
    #endif
}

//#define SMAA_DISABLE_DIAG_DETECTION
OUTPUT
void main()
{
    vec4 subsampleIndices = vec4(0.0);
    vec4 weights = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 e = texture(u_texture, v_texCoord).rg;

    if (e.g > 0.0) //Edge at north
    {
        #if !defined(SMAA_DISABLE_DIAG_DETECTION)
        weights.rg = SMAACalculateDiagWeights(v_texCoord, e, subsampleIndices);
        if (weights.r == -weights.g)// weights.r + weights.g == 0.0
        {        
        #endif

        vec2 d = vec2(0.0);

        vec3 coords = vec3(0.0);
        coords.x = SMAASearchXLeft(v_offset[0].xy, v_offset[2].x);
        coords.y = v_offset[1].y; // vOffset[1].y = vTexCoord0.y - 0.25 * SMAA_RT_METRICS.y (@CROSSING_OFFSET)
        d.x = coords.x;

        float e1 = SMAASampleLevelZero(u_texture, coords.xy).r; // LinearSampler

        coords.z = SMAASearchXRight(v_offset[0].zw, v_offset[2].y);
        d.y = coords.z;
        d = abs(round(mad(SMAA_RT_METRICS.zz, d, -v_pixCoord.xx)));

        vec2 sqrt_d = sqrt(d);

        float e2 = SMAASampleLevelZeroOffset(u_texture, coords.zy, ivec2(1, 0)).r;

        weights.rg = SMAAArea(sqrt_d, e1, e2, subsampleIndices.y);

        coords.y = v_texCoord.y;
        SMAADetectHorizontalCornerPattern(weights.rg, coords.xyzy, d);

        #if !defined(SMAA_DISABLE_DIAG_DETECTION)
        } else
        e.r = 0.0; //Skip vertical processing.
        #endif
    }

    if (e.r > 0.0) //Edge at west
    {    
        vec2 d = vec2(0.0);

        vec3 coords = vec3(0.0);
        coords.y = SMAASearchYUp(v_offset[1].xy, v_offset[2].z);
        coords.x = v_offset[0].x; // vOffset[1].x = vTexCoord0.x - 0.25 * SMAA_RT_METRICS.x;
        d.x = coords.y;

        float e1 = SMAASampleLevelZero(u_texture, coords.xy).g; // LinearSampler

        coords.z = SMAASearchYDown(v_offset[1].zw, v_offset[2].w);
        d.y = coords.z;
        d = abs(round(mad(SMAA_RT_METRICS.ww, d, -v_pixCoord.yy)));

        vec2 sqrt_d = sqrt(d);

        float e2 = SMAASampleLevelZeroOffset(u_texture, coords.xz, ivec2(0, -1)).g;

        weights.ba = SMAAArea(sqrt_d, e1, e2, subsampleIndices.x);

        coords.x = v_texCoord.x;
        SMAADetectVerticalCornerPattern(weights.ba, coords.xyxz, d);
    }

    FRAG_OUT = weights;// vec4(weights.rgb, 0.0);
    FRAG_OUT.rgb = pow(FRAG_OUT.rgb, vec3(1.0 / 2.2));
})";
}
