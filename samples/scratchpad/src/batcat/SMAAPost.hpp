/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

crogine test application - Zlib license.

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

#include <crogine/detail/glm/vec2.hpp>
#include <crogine/graphics/RenderTexture.hpp>
#include <crogine/graphics/SimpleQuad.hpp>
#include <crogine/graphics/Shader.hpp>

#ifdef CRO_DEBUG_
#include <crogine/gui/GuiClient.hpp>
#endif

class SMAAPost final
#ifdef CRO_DEBUG_
    : public cro::GuiClient
#endif
{
public:
    SMAAPost();
    ~SMAAPost();

    //TODO could make moveable..?
    SMAAPost(const SMAAPost&) = delete;
    SMAAPost(SMAAPost&&) = delete;
    SMAAPost& operator = (const SMAAPost&) = delete;
    SMAAPost& operator = (SMAAPost&&) = delete;

    void create(const cro::Texture& input, cro::RenderTexture& output);

    void render();

private:
    std::int32_t m_inputTexture;
    cro::RenderTexture* m_output;

    cro::RenderTexture m_edgeTexture;
    cro::RenderTexture m_blendTexture;

    cro::SimpleQuad m_edgeDetection;
    cro::SimpleQuad m_blendCalc;
    cro::SimpleQuad m_blendOutput;

    cro::Shader m_edgeShader;
    cro::Shader m_weightShader;
    cro::Shader m_blendShader;


    struct TextureID final
    {
        enum
        {
            Area, Search,

            Count
        };
    };
    std::array<std::uint32_t, TextureID::Count> m_supportTextures = {};
};