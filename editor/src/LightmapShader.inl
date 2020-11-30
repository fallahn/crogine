#pragma once

#include <string>

namespace Lightmap
{
    static const std::string Vertex = R"(
        ATTRIBUTE vec4 a_position;
        //ATTRIBUTE vec3 a_normal;
        //ATTRIBUTE vec2 a_texCoord0;

        //uniform mat4 u_modelMatrix;
        uniform mat4 u_viewMatrix;
        uniform mat4 u_projectionMatrix;

        //VARYING_OUT vec v_texCoord0;

        void main()
        {
            gl_Position = u_projectionMatrix * u_viewMatrix * a_position;
            //v_texCoord0 = a_texCoord0;
        })";

    static const std::string Fragment = R"(
        OUTPUT

        //uniform sampler2D u_texture;

        //varying vec2 v_texCoord0;

        void main()
        {
            FRAG_OUT = vec4(vec3(0.f), gl_FrontFacing ? 1.0 : 0.0);
        })";
}
