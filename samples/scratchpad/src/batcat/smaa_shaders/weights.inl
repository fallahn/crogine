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
    vec4 SMAA_RT_METRICS = vec4(1.0 / u_resolution.x, 1.0 / u_resolution.y, u_resolution.x, u_resolution.y);
    v_pixCoord = a_texCoord0 * SMAA_RT_METRICS.zw;

    //We will use these offsets for the searches later on (see @PSEUDO_GATHER4):
    v_offset[0] = mad(SMAA_RT_METRICS.xyxy, vec4(-0.25, -0.125,  1.25, -0.125), v_texCoord.xyxy);
    v_offset[1] = mad(SMAA_RT_METRICS.xyxy, vec4(-0.125, -0.25, -0.125,  1.25), v_texCoord.xyxy);

    //And these for the searches, they indicate the ends of the loops:
    v_offset[2] = mad(
        SMAA_RT_METRICS.xxyy,
        vec4(-2.0, 2.0, -2.0, 2.0) * float(SMAA_MAX_SEARCH_STEPS),
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
#define SMAA_MAX_SEARCH_STEPS 4
#elif defined(SMAA_PRESET_MEDIUM)
#define SMAA_MAX_SEARCH_STEPS 8
#elif defined(SMAA_PRESET_HIGH)
#define SMAA_MAX_SEARCH_STEPS 16
#elif defined(SMAA_PRESET_ULTRA)
#define SMAA_MAX_SEARCH_STEPS 32
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

//Texture Access Defines
#ifndef SMAA_AREATEX_SELECT
#define SMAA_AREATEX_SELECT(sample) sample.rg
#endif

#ifndef SMAA_SEARCHTEX_SELECT
#define SMAA_SEARCHTEX_SELECT(sample) sample.r
#endif

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
#define SMAASampleLevelZeroOffset(tex, coord, offset) texture(tex, coord + offset * SMAA_RT_METRICS.xy)

#line 179
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

vec2 SMAASearchDiag1(sampler2D edgesTex, vec2 texcoord, vec2 dir, out vec2 e)
{
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    vec3 t = vec3(SMAA_RT_METRICS.xy, 1.0);

    for (int i = 0; i < SMAA_MAX_SEARCH_STEPS; i++)
    {
        if (!(coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9)) break;

        coord.xyz = mad(t, vec3(dir, 1.0), coord.xyz);
        e = texture(edgesTex, coord.xy).rg; // LinearSampler
        coord.w = dot(e, vec2(0.5, 0.5));
    }
    return coord.zw;
}

vec2 SMAASearchDiag2(sampler2D edgesTex, vec2 texcoord, vec2 dir, out vec2 e)
{
    vec4 coord = vec4(texcoord, -1.0, 1.0);
    coord.x += 0.25 * SMAA_RT_METRICS.x; // See @SearchDiag2Optimization
    vec3 t = vec3(SMAA_RT_METRICS.xy, 1.0);

    for (int i = 0; i < SMAA_MAX_SEARCH_STEPS; i++)
    {
        if (!(coord.z < float(SMAA_MAX_SEARCH_STEPS_DIAG - 1) && coord.w > 0.9)) break;

        coord.xyz = mad(t, vec3(dir, 1.0), coord.xyz);

        // @SearchDiag2Optimization
        // Fetch both edges at once using bilinear filtering:
        e = texture(edgesTex, coord.xy).rg; // LinearSampler
        e = SMAADecodeDiagBilinearAccess(e);

        // Non-optimized version:
        // e.g = texture(edgesTex, coord.xy).g; // LinearSampler
        // e.r = SMAASampleLevelZeroOffset(edgesTex, coord.xy, vec2(1, 0)).r;

        coord.w = dot(e, vec2(0.5, 0.5));
    }
    return coord.zw;
}

vec2 SMAAAreaDiag(sampler2D areaTex, vec2 dist, vec2 e, float offset)
{
    vec2 texcoord = mad(vec2(SMAA_AREATEX_MAX_DISTANCE_DIAG, SMAA_AREATEX_MAX_DISTANCE_DIAG), e, dist);

    //We do a scale and bias for mapping to texel space:
    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);

    //Diagonal areas are on the second half of the texture:
    texcoord.x += 0.5;

    //Move to proper place, according to the subpixel offset:
    texcoord.y += SMAA_AREATEX_SUBTEX_SIZE * offset;

    //Do it!
    return SMAA_AREATEX_SELECT(texture(areaTex, texcoord)); // LinearSampler
}

vec2 SMAACalculateDiagWeights(sampler2D edgesTex, sampler2D areaTex, vec2 texcoord, vec2 e, vec4 subsampleIndices)
{
    vec2 weights = vec2(0.0, 0.0);

    //Search for the line ends:
    vec4 d;
    vec2 end;
    if (e.r > 0.0)
    {
        d.xz = SMAASearchDiag1(edgesTex, texcoord, vec2(-1.0,  1.0), end);
        d.x += float(end.y > 0.9);
    } 
    else
    {
        d.xz = vec2(0.0, 0.0);
    }
    d.yw = SMAASearchDiag1(edgesTex, texcoord, vec2(1.0, -1.0), end);

    if (d.x + d.y > 2.0) 
    {
        //Fetch the crossing edges:
        vec4 coords = mad(vec4(-d.x + 0.25, d.x, d.y, -d.y - 0.25), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
        vec4 c;
        c.xy = SMAASampleLevelZeroOffset(edgesTex, coords.xy, vec2(-1,  0)).rg;
        c.zw = SMAASampleLevelZeroOffset(edgesTex, coords.zw, vec2( 1,  0)).rg;
        c.yxwz = SMAADecodeDiagBilinearAccess(c.xyzw);

        // Non-optimized version:
        // vec4 coords = mad(vec4(-d.x, d.x, d.y, -d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
        // vec4 c;
        // c.x = SMAASampleLevelZeroOffset(edgesTex, coords.xy, vec2(-1,  0)).g;
        // c.y = SMAASampleLevelZeroOffset(edgesTex, coords.xy, vec2( 0,  0)).r;
        // c.z = SMAASampleLevelZeroOffset(edgesTex, coords.zw, vec2( 1,  0)).g;
        // c.w = SMAASampleLevelZeroOffset(edgesTex, coords.zw, vec2( 1, -1)).r;

        //Merge crossing edges at each side into a single value:
        vec2 cc = mad(vec2(2.0, 2.0), c.xz, c.yw);

        //Remove the crossing edge if we didn't found the end of the line:
        SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

        //Fetch the areas for this line:
        weights += SMAAAreaDiag(areaTex, d.xy, cc, subsampleIndices.z);
    }

    //Search for the line ends:
    d.xz = SMAASearchDiag2(edgesTex, texcoord, vec2(-1.0, -1.0), end);
    if (SMAASampleLevelZeroOffset(edgesTex, texcoord, vec2(1, 0)).r > 0.0)
    {
        d.yw = SMAASearchDiag2(edgesTex, texcoord, vec2(1.0, 1.0), end);
        d.y += float(end.y > 0.9);
    } 
    else
    {
        d.yw = vec2(0.0, 0.0);
    }

    if (d.x + d.y > 2.0)
    {
        //Fetch the crossing edges:
        vec4 coords = mad(vec4(-d.x, -d.x, d.y, d.y), SMAA_RT_METRICS.xyxy, texcoord.xyxy);
        vec4 c;
        c.x  = SMAASampleLevelZeroOffset(edgesTex, coords.xy, vec2(-1,  0)).g;
        c.y  = SMAASampleLevelZeroOffset(edgesTex, coords.xy, vec2( 0, -1)).r;
        c.zw = SMAASampleLevelZeroOffset(edgesTex, coords.zw, vec2( 1,  0)).gr;
        vec2 cc = mad(vec2(2.0, 2.0), c.xz, c.yw);

        //Remove the crossing edge if we didn't found the end of the line:
        SMAAMovc(bvec2(step(0.9, d.zw)), cc, vec2(0.0, 0.0));

        //Fetch the areas for this line:
        weights += SMAAAreaDiag(areaTex, d.xy, cc, subsampleIndices.w).gr;
    }
    return weights;
}

float SMAASearchLength(sampler2D searchTex, vec2 e, float offset)
{
    //The texture is flipped vertically, with left and right cases taking half
    //of the space horizontally:
    vec2 scale = SMAA_SEARCHTEX_SIZE * vec2(0.5, -1.0);
    vec2 bias = SMAA_SEARCHTEX_SIZE * vec2(offset, 1.0);

    //Scale and bias to access texel centers:
    scale += vec2(-1.0,  1.0);
    bias  += vec2( 0.5, -0.5);

    //Convert from pixel coordinates to texcoords:
    //(We use SMAA_SEARCHTEX_PACKED_SIZE because the texture is cropped)
    scale *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;
    bias *= 1.0 / SMAA_SEARCHTEX_PACKED_SIZE;

    //Lookup the search texture:
    return SMAA_SEARCHTEX_SELECT(texture(searchTex, mad(scale, e, bias))); // LinearSampler
}

float SMAASearchXLeft(sampler2D edgesTex, sampler2D searchTex, vec2 texcoord, float end)
{
    vec2 e = vec2(0.0, 1.0);
    for (int i = 0; i < SMAA_MAX_SEARCH_STEPS; i++)
    {
        if (!(texcoord.x > end && e.g > 0.8281 && e.r == 0.0)) break;

        e = texture2D(edgesTex, texcoord).rg; // LinearSampler
        texcoord = mad(-vec2(2.0, 0.0), SMAA_RT_METRICS.xy, texcoord);
    }

    float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e, 0.0), 3.25);
    return mad(SMAA_RT_METRICS.x, offset, texcoord.x);
}

float SMAASearchXRight(sampler2D edgesTex, sampler2D searchTex, vec2 texcoord, float end)
{
    vec2 e = vec2(0.0, 1.0);
    for (int i = 0; i < SMAA_MAX_SEARCH_STEPS; i++)
    { 
        if (!(texcoord.x < end && e.g > 0.8281 && e.r == 0.0)) break;

        e = texture2D(edgesTex, texcoord).rg; // LinearSampler
        texcoord = mad(vec2(2.0, 0.0), SMAA_RT_METRICS.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e, 0.5), 3.25);
    return mad(-SMAA_RT_METRICS.x, offset, texcoord.x);
}

float SMAASearchYUp(sampler2D edgesTex, sampler2D searchTex, vec2 texcoord, float end)
{
    vec2 e = vec2(1.0, 0.0);
    for (int i = 0; i < SMAA_MAX_SEARCH_STEPS; i++)
    {
        if (!(texcoord.y > end && e.r > 0.8281 && e.g == 0.0)) break;

        e = texture2D(edgesTex, texcoord).rg; // LinearSampler
        texcoord = mad(-vec2(0.0, 2.0), SMAA_RT_METRICS.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e.gr, 0.0), 3.25);
    return mad(SMAA_RT_METRICS.y, offset, texcoord.y);
}

float SMAASearchYDown(sampler2D edgesTex, sampler2D searchTex, vec2 texcoord, float end)
{
    vec2 e = vec2(1.0, 0.0);
    for (int i = 0; i < SMAA_MAX_SEARCH_STEPS; i++)
    {
        if (!(texcoord.y < end && e.r > 0.8281 && e.g == 0.0)) break;

        e = texture(edgesTex, texcoord).rg; // LinearSampler
        texcoord = mad(vec2(0.0, 2.0), SMAA_RT_METRICS.xy, texcoord);
    }
    float offset = mad(-(255.0 / 127.0), SMAASearchLength(searchTex, e.gr, 0.5), 3.25);
    return mad(-SMAA_RT_METRICS.y, offset, texcoord.y);
}

vec2 SMAAArea(sampler2D areaTex, vec2 dist, float e1, float e2, float offset)
{
    //Rounding prevents precision errors of bilinear filtering:
    vec2 texcoord = mad(vec2(SMAA_AREATEX_MAX_DISTANCE, SMAA_AREATEX_MAX_DISTANCE), round(4.0 * vec2(e1, e2)), dist);

    //We do a scale and bias for mapping to texel space:
    texcoord = mad(SMAA_AREATEX_PIXEL_SIZE, texcoord, 0.5 * SMAA_AREATEX_PIXEL_SIZE);

    //Move to proper place, according to the subpixel offset:
    texcoord.y = mad(SMAA_AREATEX_SUBTEX_SIZE, offset, texcoord.y);

    //Do it!
    return SMAA_AREATEX_SELECT(texture(areaTex, texcoord)); // LinearSampler
}

void SMAADetectHorizontalCornerPattern(sampler2D edgesTex, inout vec2 weights, vec4 texcoord, vec2 d)
{
    #if !defined(SMAA_DISABLE_CORNER_DETECTION)
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;

    rounding /= leftRight.x + leftRight.y; // Reduce blending for pixels in the center of a line.

    vec2 factor = vec2(1.0, 1.0);
    factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, vec2(0,  1)).r;
    factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, vec2(1,  1)).r;
    factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, vec2(0, -2)).r;
    factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, vec2(1, -2)).r;

    weights *= saturate(factor);
    #endif
}

void SMAADetectVerticalCornerPattern(sampler2D edgesTex, inout vec2 weights, vec4 texcoord, vec2 d)
{
    #if !defined(SMAA_DISABLE_CORNER_DETECTION)
    vec2 leftRight = step(d.xy, d.yx);
    vec2 rounding = (1.0 - SMAA_CORNER_ROUNDING_NORM) * leftRight;

    rounding /= leftRight.x + leftRight.y;

    vec2 factor = vec2(1.0, 1.0);
    factor.x -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, vec2( 1, 0)).g;
    factor.x -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, vec2( 1, 1)).g;
    factor.y -= rounding.x * SMAASampleLevelZeroOffset(edgesTex, texcoord.xy, vec2(-2, 0)).g;
    factor.y -= rounding.y * SMAASampleLevelZeroOffset(edgesTex, texcoord.zw, vec2(-2, 1)).g;

    weights *= saturate(factor);
    #endif
}



OUTPUT
void main()
{
    vec4 subsampleIndices = vec4(0.0); // Just pass zero for SMAA 1x, see @SUBSAMPLE_INDICES.
    //subsampleIndices = vec4(1.0, 1.0, 1.0, 0.0);
    vec4 weights = vec4(0.0);
    vec2 e = texture(u_texture, v_texCoord).rg;

    if (e.g > 0.0) //Edge at north
    {
        #if !defined(SMAA_DISABLE_DIAG_DETECTION)
        //Diagonals have both north and west edges, so searching for them in
        //one of the boundaries is enough.
        weights.rg = SMAACalculateDiagWeights(u_texture, u_areaTexture, v_texCoord, e, subsampleIndices);
        //We give priority to diagonals, so if we find a diagonal we skip
        //horizontal/vertical processing.
        if (weights.r == -weights.g)// weights.r + weights.g == 0.0
        {        
        #endif

        vec2 d = vec2(0.0);

        //Find the distance to the left:
        vec3 coords = vec3(0.0);
        coords.x = SMAASearchXLeft(u_texture, u_searchTexture, v_offset[0].xy, v_offset[2].x);
        coords.y = v_offset[1].y; // vOffset[1].y = vTexCoord0.y - 0.25 * SMAA_RT_METRICS.y (@CROSSING_OFFSET)
        d.x = coords.x;

        //Now fetch the left crossing edges, two at a time using bilinear
        //filtering. Sampling at -0.25 (see @CROSSING_OFFSET) enables to
        //discern what value each edge has:
        float e1 = texture(u_texture, coords.xy).r; // LinearSampler

        //Find the distance to the right:
        coords.z = SMAASearchXRight(u_texture, u_searchTexture, v_offset[0].zw, v_offset[2].y);
        d.y = coords.z;

        //We want the distances to be in pixel units (doing this here allow to
        //better interleave arithmetic and memory accesses):
        d = abs(round(mad(SMAA_RT_METRICS.zz, d, -v_pixCoord.xx)));

        //SMAAArea below needs a sqrt, as the areas texture is compressed
        //quadratically:
        vec2 sqrt_d = sqrt(d);

        //Fetch the right crossing edges:
        float e2 = SMAASampleLevelZeroOffset(u_texture, coords.zy, vec2(1, 0)).r;

        //Ok, we know how this pattern looks like, now it is time for getting
        //the actual area:
        weights.rg = SMAAArea(u_texture, sqrt_d, e1, e2, subsampleIndices.y);
        //Fix corners:
        coords.y = v_texCoord.y;
        SMAADetectHorizontalCornerPattern(u_texture, weights.rg, coords.xyzy, d);

        #if !defined(SMAA_DISABLE_DIAG_DETECTION)
        } else
        e.r = 0.0; //Skip vertical processing.
        #endif
    }

    if (e.r > 0.0) //Edge at west
    {    
        vec2 d = vec2(0.0);

        //Find the distance to the top:
        vec3 coords;
        coords.y = SMAASearchYUp(u_texture, u_searchTexture, v_offset[1].xy, v_offset[2].z);
        coords.x = v_offset[0].x; // vOffset[1].x = vTexCoord0.x - 0.25 * SMAA_RT_METRICS.x;
        d.x = coords.y;

        //Fetch the top crossing edges:
        float e1 = texture(u_texture, coords.xy).g; // LinearSampler

        //Find the distance to the bottom:
        coords.z = SMAASearchYDown(u_texture, u_searchTexture, v_offset[1].zw, v_offset[2].w);
        d.y = coords.z;

        //We want the distances to be in pixel units:
        d = abs(round(mad(SMAA_RT_METRICS.ww, d, -v_pixCoord.yy)));

        //SMAAArea below needs a sqrt, as the areas texture is compressed
        //quadratically:
        vec2 sqrt_d = sqrt(d);

        //Fetch the bottom crossing edges:
        float e2 = SMAASampleLevelZeroOffset(u_texture, coords.xz, vec2(0, 1)).g;

        //Get the area for this direction:
        weights.ba = SMAAArea(u_areaTexture, sqrt_d, e1, e2, subsampleIndices.x);

        //Fix corners:
        coords.x = v_texCoord.x;
        SMAADetectVerticalCornerPattern(u_texture, weights.ba, coords.xyxz, d);
    }

    FRAG_OUT = weights;// * v_colour;
})";
}
