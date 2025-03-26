/*-----------------------------------------------------------------------

Matt Marchant 2020 - 2025
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

#include <crogine/graphics/LoadingScreen.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/detail/glm/mat4x4.hpp>

#include <crogine/graphics/Font.hpp>
#include <crogine/graphics/SimpleText.hpp>
#include <crogine/graphics/SimpleQuad.hpp>

#include <vector>
#include <memory>

struct SharedStateData;
class LoadingScreen final : public cro::LoadingScreen
{
public:
    explicit LoadingScreen(SharedStateData&);
    ~LoadingScreen();

    void launch() override;
    void update() override;
    void draw() override;

private:
    SharedStateData& m_sharedData;

    std::uint32_t m_vao;
    std::uint32_t m_vbo;
    std::int32_t m_projectionIndex;
    std::int32_t m_transformIndex;
    std::int32_t m_frameIndex;

    std::int32_t m_currentFrame;
    static constexpr std::int32_t FrameCount = 8;

    cro::Shader m_shader;
    glm::mat4 m_transform;
    glm::mat4 m_projectionMatrix;
    glm::uvec2 m_viewport;

    cro::Clock m_clock;

    cro::Texture m_texture;

    cro::Font m_font;
    std::unique_ptr<cro::SimpleText> m_tipText;
};