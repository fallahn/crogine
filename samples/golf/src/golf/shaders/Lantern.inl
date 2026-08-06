/*-----------------------------------------------------------------------

Matt Marchant 2024 - 2026
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

static const inline std::string BuntingVert =
R"(
ATTRIBUTE vec4 a_position;

#include WVP_UNIFORMS

out WORLD_POS
{
    vec4 position;
}world_pos_out;

void main()
{
    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
    world_pos_out.position = u_worldMatrix * a_position;
    gl_Position = wvp * a_position;
})";

static const inline std::string BuntingGeom =
R"(
layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in WORLD_POS
{
    vec4 position;
}world_pos_in[];
out vec3 v_normal;

void main()
{
    vec3 a = vec3(world_pos_in[0].position - world_pos_in[1].position);
    vec3 b = vec3(world_pos_in[2].position - world_pos_in[1].position);
    v_normal = normalize(cross(a, b));

    gl_PrimitiveID = gl_PrimitiveIDIn;

    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();
    EndPrimitive();
})";


/*
#if defined (VIEW_POS)
    POS_OUT.r = v_viewPosition.z;
#else
    POS_OUT = vec4(v_worldPosition, 1.0);
#endif

White
Yellow
Blue
Orange
Purple
Green
Pink
(Red 184, 53, 48) 0.7216, 0.2078, 0.1882
*/

static const inline std::string BuntingFrag =
R"(
#include LIGHT_UBO
#include LIGHT_COLOUR
#include OUTPUT_LOCATION

in vec3 v_normal;

#define COLOUR_COUNT 7
const vec4 Colours[COLOUR_COUNT] = vec4[COLOUR_COUNT]
(
vec4(1.0, 0.9735, 0.8824, 1.0),
vec4(0.94902, 0.811765, 0.360784, 1.0),
vec4(0.2392, 0.6941, 0.9255, 1.0),
vec4(0.92549, 0.466667, 0.239216, 1.0),
vec4(0.4627, 0.2431, 0.4941, 1.0),
vec4(0.4313, 0.7450, 0.4392, 1.0),
vec4(0.92549, 0.6, 0.513726, 1.0)
);

void main()
{
    float id = gl_PrimitiveID;
    int i = int(mod(floor(id), COLOUR_COUNT));

    vec3 normal = normalize(v_normal);
    if (!gl_FrontFacing)
    {
        normal *= -1.0;
    }

    vec3 lightDirection = normalize(-u_lightDirection);
    float amount = clamp(dot(normal, lightDirection), 0.0, 1.0);
    amount = (0.3 * amount) + 0.7;

    FRAG_OUT = Colours[i] * getLightColour() * amount;

#if defined(USE_MRT)
    POS_OUT = vec4(1.0);
    NORM_OUT = vec4(normal, 0.0); //mask off lightmap
    LIGHT_OUT = vec4(vec3(0.0), 1.0);
#endif
})";

static const inline std::string RopeFrag =
R"(
uniform vec4 u_colour = vec4(0.6784, 0.7255, 0.7216, 1.0);
OUTPUT
void main(){FRAG_OUT = u_colour;}
)";

static const inline std::string LanternVert =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec4 a_colour;
ATTRIBUTE vec3 a_normal;

#include CAMERA_UBO
#include WVP_UNIFORMS

VARYING_OUT vec3 v_worldPosition;
VARYING_OUT vec4 v_colour;
VARYING_OUT vec3 v_normalVector;

void main()
{
    mat4 wvp = u_projectionMatrix * u_worldViewMatrix;
    gl_Position = wvp * a_position;

    v_worldPosition = (u_worldMatrix * a_position).xyz;
    v_colour = a_colour;

    v_normalVector = u_normalMatrix * a_normal;
}
)";

static const inline std::string LanternFrag =
R"(
#include CAMERA_UBO

uniform vec4 u_ballColour; //not a ball, but we need the name to match up (see createRopes() in MainMenu)

VARYING_IN vec3 v_worldPosition;
VARYING_IN vec4 v_colour;
VARYING_IN vec3 v_normalVector;

#if defined(USE_MRT)
#include OUTPUT_LOCATION
#else
OUTPUT
#endif

//hacky way of working around different rim constants
//when using this shader with the tee markers
#if defined(USE_MRT)
const float RimStart = 0.15;
const float RimEnd = 0.99;
const float RimAttenuation = 0.4;
#else
const float RimStart = 0.5;
const float RimEnd = 0.99;
const float RimAttenuation = 0.4;
#endif

void main()
{
    vec3 normal = normalize(v_normalVector);
    vec3 eyeDirection = normalize(u_cameraWorldPosition - v_worldPosition);
    float rim = dot(normal, eyeDirection);
    rim = smoothstep(RimStart, RimEnd, rim) * RimAttenuation;

    FRAG_OUT = (v_colour * u_ballColour) + vec4(rim);
    FRAG_OUT.a = 1.0;

#if defined(USE_MRT)
//this is tee markers at night

#if defined(VIEW_POS)
    POS_OUT.r = v_viewPosition.z;
#else
    POS_OUT = vec4(v_worldPosition, 1.0);
#endif
LIGHT_OUT = vec4(FRAG_OUT.rgb, 1.0);
#endif

})";