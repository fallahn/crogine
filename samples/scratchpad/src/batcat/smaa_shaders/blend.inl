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

namespace SMAA::Blend
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
VARYING_OUT vec4 v_offset;

void main()
{
    v_colour = a_colour; //totally unnecessary and may be even breaking - however the SimpleQuad we're using expects this...
    v_texCoord = a_texCoord0;
    vec4 SMAA_RT_METRICS = vec4(1.0 / u_resolution.x, 1.0 / u_resolution.y, u_resolution.x, u_resolution.y);

    v_offset = mad(SMAA_RT_METRICS.xyxy, vec4(1.0, 0.0, 0.0, -1.0), a_texCoord0.xyxy);

    gl_Position = u_projectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
})";

    static inline const std::string Frag = 
R"(
#define mad(a, b, c) (a * b + c)

uniform sampler2D u_texture; //blend weights
uniform sampler2D u_colourTexture;

uniform vec2 u_resolution = vec2(800.0, 600.0);

VARYING_IN vec2 v_texCoord;
VARYING_IN vec4 v_offset;

vec4 SMAA_RT_METRICS = vec4(1.0 / u_resolution.x, 1.0 / u_resolution.y, u_resolution.x, u_resolution.y);

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

OUTPUT

void main()
{
    vec4 colour = vec4(1.0);

    //Fetch the blending weights for current pixel:
    vec4 a = vec4(0.0);
    a.x = texture(u_texture, v_offset.xy).a; //Right
    a.y = texture(u_texture, v_offset.zw).g; //Top
    a.wz = texture(u_texture, v_texCoord).xz; //Bottom / Left

    //Is there any blending weight with a value greater than 0.0?
    if (dot(a, vec4(1.0, 1.0, 1.0, 1.0)) <= 1e-5)
    {
        colour = textureLod(u_colourTexture, v_texCoord, 0.0);
    }
    else
    {
        bool h = max(a.x, a.z) > max(a.y, a.w);// max(-a.y, -a.w); //max(horizontal) > max(vertical)

        //Calculate the blending offsets:
        vec4 blendingOffset = vec4(0.0, a.y, 0.0, a.w);
        vec2 blendingWeight = a.yw;
        SMAAMovc(bvec4(h, h, h, h), blendingOffset, vec4(a.x, 0.0, a.z, 0.0));
        SMAAMovc(bvec2(h, h), blendingWeight, a.xz);
        blendingWeight /= dot(blendingWeight, vec2(1.0, 1.0));

        //Calculate the texture coordinates:
        vec4 blendingCoord = mad(blendingOffset, vec4(SMAA_RT_METRICS.xy, -SMAA_RT_METRICS.xy), v_texCoord.xyxy);

        //blendingCoord.y = 1.0 - blendingCoord.y;

        //We exploit bilinear filtering to mix current pixel with the chosen
        //neighbor:
        colour = blendingWeight.x * textureLod(u_colourTexture, blendingCoord.xy, 0.0);
        colour += blendingWeight.y * textureLod(u_colourTexture, blendingCoord.zw, 0.0);
    }

    FRAG_OUT = colour;// * vec4(1.0, 0.0, 0.0, 1.0);
})";
}