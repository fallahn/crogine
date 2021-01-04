#pragma once

#include <string>

namespace TriangleShader
{
    //BLOCK_SIZE is a definition passedin on shader creation
    //once the actual size has been queried from the driver
    //AntiAliasing is also a definition.
    static const std::string Vertex =
        R"(
            #line 12

            struct VertexData
            {
                vec4 positionSize;
                uint colour;
            };

            uniform VertexDataBlock
            {
                VertexData[BLOCK_SIZE / 32] u_vertexData;
            };

            uniform mat4 u_viewProjectionMatrix;

            in vec4 a_position;

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
                int vertID = gl_InstanceID * 3 + gl_InstanceID;
                v_colour = uintToRGBA(u_vertexData[vertID].colour);
                gl_Position = u_viewProjectionMatrix * vec4(u_vertexData[vertID].positionSize.xyz, 1.0);
            })";


    static const std::string Fragment =
        R"(
            #line 52

            smooth in vec4 v_colour;

            OUTPUT

            void main()
            {
                fragOut = v_colour;
            })";

}