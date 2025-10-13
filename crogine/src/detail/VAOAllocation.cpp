/*-----------------------------------------------------------------------

Matt Marchant 2025
http://trederia.blogspot.com

crogine - Zlib license.

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

#include "GLCheck.hpp"

#include <crogine/detail/Assert.hpp>
#include <crogine/detail/VAOAllocation.hpp>
#include <crogine/graphics/Shader.hpp>

using namespace cro::Detail;

VAOAllocator::VAOAllocator(std::size_t initialPoolSize)
{
    //just allocate some up front
    if (initialPoolSize)
    {
        m_activeVAOs.resize(initialPoolSize);
        glCheck(glGenVertexArrays(m_activeVAOs.size(), m_activeVAOs.data()));

        m_freeVAOs = m_activeVAOs;
    }
}

VAOAllocator::~VAOAllocator()
{
    if (!m_activeVAOs.empty())
    {
        glCheck(glDeleteVertexArrays(m_activeVAOs.size(), m_activeVAOs.data()));
    }
}

//public
std::uint32_t VAOAllocator::requestVAO()
{
    if (m_freeVAOs.empty())
    {
        //LogI << "Generated new VAO" << std::endl;
        std::uint32_t vao = 0;
        glCheck(glGenVertexArrays(1, &vao));
        m_activeVAOs.push_back(vao);
        return vao;
    }

    const auto vao = m_freeVAOs.back();
    m_freeVAOs.pop_back();

    //LogI << "Reused VAO " << vao << std::endl;
    return vao;
}

void VAOAllocator::freeVAO(std::uint32_t vao)
{
    CRO_ASSERT(vao != 0, "");

    //make sure to reset ALL the existing bindings
    glCheck(glBindVertexArray(vao));
    for (auto i = 0; i < Shader::AttributeID::Count; ++i)
    {
        glCheck(glDisableVertexAttribArray(i));
    }

    //LogI << "Freed VAO " << vao << std::endl;
    m_freeVAOs.push_back(vao);
}