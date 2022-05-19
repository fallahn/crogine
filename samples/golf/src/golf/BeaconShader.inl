#pragma once

#include <string>

static const std::string BeaconVertex = R"(
#line 1
    ATTRIBUTE vec4 a_position;
    ATTRIBUTE vec4 a_colour;
    ATTRIBUTE vec2 a_texCoord0;

    uniform mat4 u_worldViewMatrix;
    uniform mat4 u_worldMatrix;
    uniform mat4 u_projectionMatrix;

    uniform vec4 u_clipPlane;
    uniform float u_colourRotation = 1.0;

    VARYING_OUT vec4 v_colour;
    VARYING_OUT vec2 v_texCoord;

    vec3 rgb2hsv(vec3 c)
    {
        vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
        vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
        vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

        float d = q.x - min(q.w, q.y);
        float e = 1.0e-10;
        return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
    }

    vec3 hsv2rgb(vec3 c)
    {
        vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
        vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
        return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
    }

    void main()
    {
#if defined (SPRITE)
        vec4 position = vec4(a_position.xy, 0.0, 1.0);
#else
        vec4 position = a_position;
#endif

        gl_Position = u_projectionMatrix * u_worldViewMatrix * position;

        v_texCoord = a_texCoord0;

        vec3 hsv = rgb2hsv(a_colour.rgb);
        hsv.x += u_colourRotation;
        v_colour = vec4(hsv2rgb(hsv), a_colour.a);

        vec4 worldPos = u_worldMatrix * position;
        gl_ClipDistance[0] = dot(worldPos, u_clipPlane);
    })";


static const std::string BeaconFragment = R"(
    OUTPUT

    uniform sampler2D u_diffuseMap;
    uniform vec4 u_colour;

    VARYING_IN vec4 v_colour;
    VARYING_IN vec2 v_texCoord;

    void main()
    {
#if defined (TEXTURED)
        FRAG_OUT = TEXTURE(u_diffuseMap, v_texCoord) * v_colour * u_colour;
#else
        FRAG_OUT = v_colour;
#endif
    })";