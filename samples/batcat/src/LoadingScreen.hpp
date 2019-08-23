/*-----------------------------------------------------------------------

Matt Marchant 2017
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

#ifndef DH_LOADING_SCREEN_HPP_
#define DH_LOADING_SCREEN_HPP_

#include <crogine/graphics/LoadingScreen.hpp>
#include <crogine/detail/Types.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Texture.hpp>
#include <crogine/core/Clock.hpp>

#include <crogine/detail/glm/mat4x4.hpp>

#include <vector>

class LoadingScreen final : public cro::LoadingScreen
{
public:
    LoadingScreen();
    ~LoadingScreen();

    void update() override;
    void draw() override;

private:
    cro::uint32 m_vbo;
    cro::uint32 m_transformIndex;

    cro::Shader m_shader;
    glm::mat4 m_transform;
    glm::mat4 m_projectionMatrix;
    glm::uvec2 m_viewport;

    cro::Clock m_clock;
    std::vector<float> m_wavetable;
    std::size_t m_wavetableIndex;

    cro::Texture m_texture;
};

#endif //DH_LOADING_SCREEN_HPP_