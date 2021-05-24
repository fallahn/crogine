/*-----------------------------------------------------------------------

Matt Marchant 2021
http://trederia.blogspot.com

crogine model viewer/importer - Zlib license.

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

#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/graphics/RenderTexture.hpp>

#include <crogine/detail/glm/mat4x4.hpp>

#include <array>

/*
Provides the ImGui window for editing mask map
textures for use with PBR materials.
*/

class MaskEditor final
{
public:
    MaskEditor();
    ~MaskEditor();

    MaskEditor(const MaskEditor&) = delete;
    MaskEditor(MaskEditor&&) = delete;
    MaskEditor& operator = (const MaskEditor&) = delete;
    MaskEditor& operator = (MaskEditor&&) = delete;

    void doImGui();

private:
    struct ChannelID final
    {
        enum
        {
            Metal,
            Roughness,
            AO,

            Count
        };
    };
    std::array<cro::Texture, ChannelID::Count> m_channelTextures = {};
    cro::Texture m_whiteTexture;

    float m_metalValue;
    float m_roughnessValue;

    cro::Shader m_shader;
    cro::RenderTexture m_outputTexture;

    struct Quad final
    {
        std::uint32_t vbo = 0;
        std::uint32_t vao = 0;

        glm::mat4 projectionMatrix;

        enum Uniforms
        {
            MetalTex,
            RoughTex,
            AOTex,

            MetalVal,
            RoughVal,

            Projection,

            Count
        };
        std::array<std::int32_t, Count> uniforms = {};
        std::array<std::uint32_t, ChannelID::Count> activeTextures = {};
    }m_quad;

    void updateOutput();
};