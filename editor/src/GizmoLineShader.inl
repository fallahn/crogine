#pragma once

#include <string>

namespace LineShader
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
            uniform vec2 u_viewportSize;

            in vec4 a_position;

            noperspective out float v_edgeDistance;
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
                int startID = gl_InstanceID * 2;
                int endID = startID + 1;
                int vertID = (gl_VertexID % 2 == 0) ? startID : endID;

                v_colour = uintToRGBA(u_vertexData[vertID].colour);
                v_size = u_vertexData[vertID].positionSize.w;
                v_colour.a *= smoothstep(0.0, 1.0, v_size / AntiAliasing);
                v_size = max(v_size, AntiAliasing);
                v_edgeDistance = v_size * a_position.y;

                vec4 pos0 = u_viewProjectionMatrix * vec4(u_vertexData[startID].positionSize.xyz, 1.0);
                vec4 pos1 = u_viewProjectionMatrix * vec4(u_vertexData[endID].positionSize.xyz, 1.0);

                vec2 dir = (pos0.xy / pos0.w) - (pos1.xy / pos1.w);
                dir = normalize(vec2(dir.x, dir.y * (u_viewportSize.y / u_viewportSize.x)));

                vec2 tng = (vec2(-dir.y, dir.x) * v_size) / u_viewportSize;

                gl_Position = (gl_VertexID % 2 == 0) ? pos0 : pos1;
                gl_Position.xy += tng * a_position.y * gl_Position.w;
            })";


    static const std::string Fragment =
        R"(
            #line 71

            noperspective in float v_edgeDistance;
            noperspective in float v_size;
            smooth in vec4 v_colour;

            OUTPUT

            void main()
            {
                fragOut = v_colour;

                float d = abs(v_edgeDistance) / v_size;
                d = smoothstep(1.0, 1.0 - (AntiAliasing / v_size), d);
                fragOut.a *= d;
            })";
}
