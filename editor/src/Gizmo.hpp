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

#include "im3d.h"

#include <crogine/ecs/Entity.hpp>
#include <crogine/graphics/Shader.hpp>
#include <crogine/graphics/Rectangle.hpp>

#include <cstdint>

class Gizmo final
{
public:
    Gizmo();
    ~Gizmo();

    Gizmo(const Gizmo&) = delete;
    Gizmo(Gizmo&&) = delete;
    Gizmo& operator = (const Gizmo&) = delete;
    Gizmo& operator = (Gizmo&&) = delete;

    void init();
    void newFrame(float, cro::Entity);
    void draw();
    void unlock();
    void finalise();

private:

    bool m_frameLocked; //prevents multiple states starting a new frame
    bool m_frameEnded;
    cro::IntRect m_viewport;

    std::uint32_t m_vao;
    std::uint32_t m_vbo;
    std::uint32_t m_ubo;

    std::int32_t m_uboBlocksize;

    cro::Shader m_pointShader;
    cro::Shader m_lineShader;
    cro::Shader m_triangleShader;

    void deleteBuffers();
};