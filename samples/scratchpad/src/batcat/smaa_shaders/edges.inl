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

namespace SMAA::Edge
{
    static inline const std::string Vert = 
R"(
#define mad(a, b, c) (a * b + c)

ATTRIBUTE vec2 a_position;
ATTRIBUTE vec2 a_texCoord0;
ATTRIBUTE vec4 a_colour;

uniform mat4 u_worldMatrix;
uniform mat4 u_projectionMatrix;
uniform vec2 u_resolution = vec2(800.0, 600.0);

VARYING_OUT vec2 v_texCoord;
VARYING_OUT vec4 v_colour;
VARYING_OUT vec4 v_offset[3];

void main()
{
    v_colour = a_colour; //totally unnecessary and may be even breaking - however the SimpleQuad we're using expects this...
    v_texCoord = a_texCoord0;
    vec4 SMAA_RT_METRICS = vec4(1.0 / u_resolution.x, 1.0 / u_resolution.y, u_resolution.x, u_resolution.y);

    v_offset[0] = mad(SMAA_RT_METRICS.xyxy, vec4(-1.0, 0.0, 0.0, 1.0), a_texCoord0.xyxy);
    v_offset[1] = mad(SMAA_RT_METRICS.xyxy, vec4( 1.0, 0.0, 0.0, -1.0), a_texCoord0.xyxy);
    v_offset[2] = mad(SMAA_RT_METRICS.xyxy, vec4(-2.0, 0.0, 0.0, 2.0), a_texCoord0.xyxy);

    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
})";


    /**
     * Luma Edge Detection
     *
     * IMPORTANT NOTICE: luma edge detection requires gamma-corrected colours, and
     * thus 'u_texture' should be a non-sRGB texture.
     * 
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
     * If there is an neighbor edge that has SMAA_LOCAL_CONTRAST_FACTOR times
     * bigger contrast than current edge, current edge will be discarded.
     *
     * This allows to eliminate spurious crossing edges, and is based on the fact
     * that, if there is too much contrast in a direction, that will hide
     * perceptually contrast in the other neighbors.
     */
    static inline const std::string LumaFragment =
R"(
uniform sampler2D u_texture;

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_colour;
VARYING_IN vec4 v_offset[3];

OUTPUT

#ifndef SMAA_THRESHOLD
#define SMAA_THRESHOLD 0.1
#endif
#ifndef SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR
#define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR 2.0
#endif

void main()
{
    vec2 threshold = vec2(SMAA_THRESHOLD);

    //Calculate lumas:
    vec3 weights = vec3(0.2126, 0.7152, 0.0722);
    float L = dot(texture(u_texture, v_texCoord).rgb, weights);

    float Lleft = dot(texture(u_texture, v_offset[0].xy).rgb, weights);
    float Ltop  = dot(texture(u_texture, v_offset[0].zw).rgb, weights);

    //We do the usual threshold:
    vec4 delta = vec4(0.0);
    delta.xy = abs(L - vec2(Lleft, Ltop));
    vec2 edges = step(threshold, delta.xy);

    //Then discard if there is no edge:
    if (dot(edges, vec2(1.0, 1.0)) == 0.0)
    {
        discard;
    }

    //Calculate right and bottom deltas:
    float Lright = dot(texture(u_texture, v_offset[1].xy).rgb, weights);
    float Lbottom  = dot(texture(u_texture, v_offset[1].zw).rgb, weights);
    delta.zw = abs(L - vec2(Lright, Lbottom));

    //Calculate the maximum delta in the direct neighborhood:
    vec2 maxDelta = max(delta.xy, delta.zw);

    //Calculate left-left and top-top deltas:
    float Lleftleft = dot(texture(u_texture, v_offset[2].xy).rgb, weights);
    float Ltoptop = dot(texture(u_texture, v_offset[2].zw).rgb, weights);
    delta.zw = abs(vec2(Lleft, Ltop) - vec2(Lleftleft, Ltoptop));

    //Calculate the final maximum delta:
    maxDelta = max(maxDelta.xy, delta.zw);
    float finalDelta = max(maxDelta.x, maxDelta.y);

    //Local contrast adaptation:
    edges.xy *= step(finalDelta, SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR * delta.xy);

    FRAG_OUT = vec4(edges, 0.0, 1.0);    
})";
}