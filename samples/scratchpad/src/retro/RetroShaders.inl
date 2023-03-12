/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

const std::string FlareVertex = R"(
    ATTRIBUTE vec2 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    uniform vec2 u_screenCentre; //in world space

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;
    VARYING_OUT vec2 v_screenOffset;

    void main()
    {
        vec4 position = u_worldMatrix * vec4(a_position, 0.0, 1.0);
        v_screenOffset = (position.xy - u_screenCentre) / u_screenCentre;

        gl_Position = u_viewProjectionMatrix * position;
        v_colour = a_colour;
        v_texCoord = a_texCoord0;
    })";

const std::string FlareFragment = R"(
    OUTPUT

    uniform sampler2D u_texture;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;
    VARYING_IN vec2 v_screenOffset;

    void main()
    {
        float r = TEXTURE(u_texture, v_texCoord + (v_screenOffset * 0.05)).r;
        float g = TEXTURE(u_texture, v_texCoord + (v_screenOffset * 0.025)).g;
        float b = TEXTURE(u_texture, v_texCoord).b;

        FRAG_OUT = vec4(r,g,b,1.0) * v_colour;
    })";

const std::string CloudVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat3 u_normalMatrix;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec4 v_colour;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        
        gl_Position = u_viewProjectionMatrix * position;

        v_worldPosition = position.xyz;
        v_normal = u_normalMatrix * a_normal;
        v_colour = a_colour;
    })";

const std::string CloudFragment = R"(
    OUTPUT

    uniform vec3 u_lightDirection;
    uniform vec4 u_skyColourTop;
    uniform vec4 u_skyColourBottom;
    uniform vec3 u_cameraWorldPosition;

    VARYING_IN vec3 v_normal;
    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec4 v_colour;

    const vec4 BaseColour = vec4(1.0);
    const float PixelSize = 1.0;
    const float ColourLevels = 2.0;

    void main()
    {
        vec3 normal = normalize(v_normal);
        vec3 viewDirection = normalize(u_cameraWorldPosition - v_worldPosition);
        float rim = smoothstep(0.6, 0.95, 1.0 - dot(normal, viewDirection));
        float rimAmount = dot(vec3(0.0, -1.0, 0.0), normal);
        rimAmount += 1.0;
        rimAmount /= 2.0;
        rim *= smoothstep(0.5, 0.9, rimAmount);


        float colourAmount = pow(1.0 - dot(normal, normalize(-u_lightDirection)), 2.0);
        colourAmount *= ColourLevels;
        colourAmount = round(colourAmount);
        colourAmount /= ColourLevels;

        vec4 colour = vec4(mix(BaseColour.rgb, u_skyColourTop.rgb, colourAmount * 0.75), 1.0);
        colour.rgb = mix(colour.rgb, u_skyColourBottom.rgb * 1.5, rim * 0.8);

        FRAG_OUT = colour;// * v_colour;

    })";

const std::string ShaderVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat3 u_normalMatrix;

    VARYING_OUT vec3 v_normal;
    VARYING_OUT float v_colourIndex;

    void main()
    {
        gl_Position = u_viewProjectionMatrix * u_worldMatrix * a_position;

        v_normal = u_normalMatrix * a_normal;
        v_colourIndex = a_colour.r;
    })";


const std::string ShaderFragment = R"(
    OUTPUT

    uniform sampler2D u_palette;

    VARYING_IN vec3 v_normal;
    VARYING_IN float v_colourIndex;


    const vec3 LightDir = vec3(1.0, 1.0, 1.0);
    const float PixelSize = 1.0;

    void main()
    {
        float lightAmount = dot(normalize(v_normal), normalize(LightDir));
        lightAmount *= 2.0;
        lightAmount = round(lightAmount);
        lightAmount /= 2.0;

        float check = mod((floor(gl_FragCoord.x / PixelSize) + floor(gl_FragCoord.y / PixelSize)), 2.0);
        check *= 2.0;
        check -= 1.0;
        lightAmount += 0.01 * check; //TODO this magic number should be based on the palette texture size, but it'll do as long as it's enough to push the coord in the correct direction.

        vec2 coord = vec2(v_colourIndex, lightAmount);
        vec3 colour = TEXTURE(u_palette, coord).rgb;

        FRAG_OUT = vec4(colour, 1.0);
    })";