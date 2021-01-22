#pragma once

#include <string>

namespace PointShader
{
    //BLOCK_SIZE is a definition passed in on shader creation
    //after the actual size if retrieved from driver query.
    //AntiAliasing is a definition passed in when the shader
    //created.
    static const std::string Vertex =
        R"(
        //aligned size of this is 32 bytes
        #line 12
        struct VertexData
        {
            vec4 positionSize;
            uint colour;
        };

        uniform VertexDataBlock
        {
            VertexData u_vertexData[BLOCK_SIZE / 32];
        };

        uniform mat4 u_viewProjectionMatrix;
        uniform vec2 u_viewportSize;

        in vec4 a_position;

        noperspective out vec2 v_texCoord;
        noperspective out float v_size;
        smooth out vec4 v_colour;

        vec4 uintToRGBA(uint colour)
        {
		    vec4 ret = vec4(0.0);
		    ret.r = float((colour & 0xff000000u) >> 24u) / 255.0;
		    ret.g = float((colour & 0x00ff0000u) >> 16u) / 255.0;
		    ret.b = float((colour & 0x0000ff00u) >> 8u)  / 255.0;
		    ret.a = float((colour & 0x000000ffu) >> 0u)  / 255.0;
		    return ret;
        }

        void main()
        {
            int vertID = gl_InstanceID;

            v_size = max(u_vertexData[vertID].positionSize.w, AntiAliasing);
            v_colour = uintToRGBA(u_vertexData[vertID].colour);
            v_colour.a *= smoothstep(0.0, 1.0, v_size / AntiAliasing);

            gl_Position = u_viewProjectionMatrix * vec4(u_vertexData[vertID].positionSize.xyz, 1.0);
            vec2 scale = 1.0 / u_viewportSize * v_size;
            gl_Position.xy += a_position.xy * scale * gl_Position.w;
            v_texCoord = a_position.xy * 0.5 + 0.5;
        })";


    static const std::string Fragment = 
        R"(
            #line 62
            noperspective in  vec2 v_texCoord;
            noperspective in float v_size;
            smooth in vec4 v_colour;

            OUTPUT

            void main()
            {
                fragOut = v_colour;

                float d = length(v_texCoord - vec2(0.5));
                d = smoothstep(0.5, 0.5 - (AntiAliasing / v_size), d);
                fragOut.a *= d;
            }
        )";
}