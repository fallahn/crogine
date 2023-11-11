/*-----------------------------------------------------------------------

Matt Marchant 2022 - 2023
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

inline const std::string CloudVertex = 
R"(
    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;

    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE MED vec2 a_texCoord0;

    VARYING_OUT vec3 v_worldPos;
    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

    void main()
    {
        vec4 worldPos = u_worldMatrix * a_position;
        gl_Position = u_viewProjectionMatrix * worldPos;
        
        v_worldPos = worldPos.xyz;
        v_colour = a_colour;
        v_texCoord = a_texCoord0;
    }
)";

inline const std::string CloudFragment =
R"(
    OUTPUT

    uniform sampler2D u_texture;
    uniform vec2 u_worldCentre = vec2(0.0);

#include SCALE_BUFFER

    VARYING_IN vec3 v_worldPos;
    VARYING_IN vec2 v_texCoord;

#if !defined(MAX_RADIUS)
    const float MaxDist = 180.0;
#else
    const float MaxDist = MAX_RADIUS;
#endif

#include BAYER_MATRIX

    void main()
    {
        FRAG_OUT = TEXTURE(u_texture, v_texCoord);

        float amount = 1.0 - smoothstep(0.7, 1.0, (length(v_worldPos.xz - u_worldCentre) / MaxDist));

        vec2 xy = gl_FragCoord.xy;// / u_pixelScale;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, amount);

        if(alpha * FRAG_OUT.a < 0.18) discard;
    }
)";

inline const std::string BowFragment =
R"(
    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    OUTPUT

    const float BowStart = 0.85;
    const float BowEnd = 0.9;
    const float BowWidth = BowEnd - BowStart;

    #define MAX_COLOURS 7
    const vec3[] Colours = vec3[MAX_COLOURS + 2]
    (
        vec3(0.0),
        vec3(0.2,   0.171, 1.0),
        vec3(0.101, 0.181, 1.0),
        vec3(0.092, 0.547, 1.0),
        vec3(0.0,   1.0,   0.312),
        vec3(0.89,  1.0,   0.074),
        vec3(1.0,   0.497, 0.088),
        vec3(1.0,   0.12,  0.12),
        vec3(0.0)
    );

    const vec3 WaterColour = vec3(0.02, 0.078, 0.578);
    const float Intensity = 0.1;

    void main()
    {
        vec2 coord = v_texCoord - 0.5;
        coord *= 2.0;

        float amount = length(coord);
        amount = max(0.0, amount - BowStart);
        amount /= BowWidth;

        int index = int(min(floor(amount * MAX_COLOURS), MAX_COLOURS + 1));

        vec3 colour = Colours[index] * Intensity;
        colour = mix(colour, WaterColour, (step(0.5, 1.0 - v_texCoord.y) * (1.0 - 0.31)) * Intensity);

        float avg = Colours[index].r + Colours[index].g + Colours[index].b;
        colour *= step(0.00001, avg / 3.0);

        FRAG_OUT = vec4(colour, 1.0);
    }
)";


//updated shader for 3D overhead clouds
inline const std::string CloudOverheadVertex = R"(
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec3 a_normal;

    uniform mat4 u_worldMatrix;
    uniform mat4 u_viewProjectionMatrix;
    uniform mat3 u_normalMatrix;

#include RESOLUTION_BUFFER

    VARYING_OUT vec3 v_normal;
    VARYING_OUT vec3 v_worldPosition;
    VARYING_OUT vec4 v_colour;

    void main()
    {
        vec4 position = u_worldMatrix * a_position;
        
#if defined(WOBBLE)
        vec4 vertPos = u_viewProjectionMatrix * position;

        vertPos.xyz /= vertPos.w;
        vertPos.xy = (vertPos.xy + vec2(1.0)) * u_scaledResolution * 0.5;
        vertPos.xy = floor(vertPos.xy);
        vertPos.xy = ((vertPos.xy / u_scaledResolution) * 2.0) - 1.0;
        vertPos.xyz *= vertPos.w;
        gl_Position = vertPos;
#else
        gl_Position = u_viewProjectionMatrix * position;
#endif

        v_worldPosition = position.xyz;
        v_normal = u_normalMatrix * a_normal;
        v_colour = a_colour;
    })";

inline const std::string CloudOverheadFragment = R"(
    OUTPUT

    uniform vec2 u_worldCentre = vec2(0.0);
    uniform vec3 u_cameraWorldPosition;
    uniform vec3 u_lightDirection;
    uniform vec4 u_lightColour;
    uniform vec4 u_skyColourTop;
    uniform vec4 u_skyColourBottom;

    VARYING_IN vec3 v_normal;
    VARYING_IN vec3 v_worldPosition;
    VARYING_IN vec4 v_colour;

#if defined (FEATHER_EDGE)
#if !defined(MAX_RADIUS)
    const float MaxDist = 200.0;
#else
    const float MaxDist = MAX_RADIUS;
#endif


#include BAYER_MATRIX
#endif

#include LIGHT_COLOUR

    const vec4 BaseColour = vec4(1.0);
    const vec3 WaterColour = vec3(0.02, 0.078, 0.578);
    const float PixelSize = 1.0;
    const float ColourLevels = 2.0;

    void main()
    {
        vec3 normal = normalize(v_normal);
        vec3 viewDirection = normalize(u_cameraWorldPosition - v_worldPosition);

#if defined(POINT_LIGHT)
        float rim = smoothstep(0.1, 0.95, 1.0 - dot(normal, viewDirection));

        vec3 lightDirection = normalize(vec3(u_worldCentre.x, 4.0, u_worldCentre.y) - v_worldPosition);
        float colourAmount = 1.0 - pow(dot(normal, lightDirection), 2.0);
#else
        float rim = smoothstep(0.7, 0.95, 1.0 - dot(normal, viewDirection));

        vec3 lightDirection = normalize(-u_lightDirection);
        float colourAmount = pow(dot(normal, lightDirection), 2.0);
#endif

        float rimAmount = dot(vec3(0.0, -1.0, 0.0), normal);
        rimAmount += 1.0;
        rimAmount *= 0.5;
        rim *= smoothstep(0.5, 0.9, rimAmount);


        colourAmount *= ColourLevels;
        colourAmount = round(colourAmount);
        colourAmount /= ColourLevels;

        vec4 colour = vec4(mix(BaseColour.rgb, u_skyColourTop.rgb, colourAmount * 0.7), 1.0);
        colour.rgb = mix(colour.rgb, u_skyColourBottom.rgb * 1.25, rim * 0.7);

#if defined(REFLECTION)
        colour.rgb = mix(WaterColour * u_lightColour.rgb, colour.rgb, v_colour.g);
#endif

        FRAG_OUT = colour * getLightColour();


#if defined(FEATHER_EDGE)
        float amount = 1.0 - smoothstep(0.9, 1.0, (length(v_worldPosition.xz - u_worldCentre) / MaxDist));

        vec2 xy = gl_FragCoord.xy;
        int x = int(mod(xy.x, MatrixSize));
        int y = int(mod(xy.y, MatrixSize));
        float alpha = findClosest(x, y, amount);

        if(alpha * FRAG_OUT.a < 0.18)
        {
            discard;
        }
#endif
    })";





//these are used in the course editor (PlaylistState)
inline const std::string CloudVertex3D =
R"(
ATTRIBUTE vec4 a_position;
ATTRIBUTE vec3 a_normal;

uniform mat4 u_worldMatrix;
uniform mat4 u_viewProjectionMatrix;
uniform mat3 u_normalMatrix;
uniform vec3 u_cameraWorldPosition;

VARYING_OUT vec3 v_normal;

void main()
{
    vec4 worldPosition = u_worldMatrix * a_position;
    gl_Position = u_viewProjectionMatrix * worldPosition;
    v_normal = u_normalMatrix * a_normal;
})";

inline const std::string CloudFragment3D =
R"(

OUTPUT

uniform vec4 u_colourA = vec4(1.0);
uniform vec4 u_colourB = vec4(0.0,0.0,0.0,1.0);
uniform vec3 u_lightDirection;

#include SCALE_BUFFER

VARYING_IN vec3 v_normal;

void main()
{
    float lightAmount = dot(normalize(u_lightDirection), normalize(v_normal));
    /*lightAmount *= 2.0;
    lightAmount = round(lightAmount);
    lightAmount /= 2.0;*/

    float check = mod((floor(gl_FragCoord.x / u_pixelScale) + floor(gl_FragCoord.y / u_pixelScale)), 2.0);
    check *= 2.0;
    check -= 1.0;
    
    lightAmount = (0.05 * check) + lightAmount;

    FRAG_OUT = mix(u_colourA, u_colourB, step(0.5, lightAmount));
})";