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

#pragma once

#include <vector>

namespace cro::Detail
{
    /*!
    \brief Recycle VAOs when needed rather than delete/create
    */
    class VAOAllocator final
    {
    public:
        /*!
        \brief Constructor.
        \param initialPoolSize Number of VAOs to create on construction.
        If more VAOs are requested then they are created on the fly if
        none are available in the pool
        */
        explicit VAOAllocator(std::size_t initialPoolSize = 0);
        ~VAOAllocator();

        VAOAllocator(const VAOAllocator&) = delete;
        VAOAllocator(VAOAllocator&&) = delete;

        VAOAllocator& operator = (const VAOAllocator&) = delete;
        VAOAllocator& operator = (VAOAllocator&&) = delete;

        /*!
        \brief Requests a VAO form the pool, creating a new one
        if necessary.
        \returns Handle to the VAO.
        */
        std::uint32_t requestVAO();

        /*!
        \brief Returns the given VAO to the pool
        \param vao The handle to return. Once returned
        any VAOs with this handle should be considered
        invalid, as it will eventually be recycled.
        */
        void freeVAO(std::uint32_t vao);

    private:

        //handles which are assigned and need deleting on shutdown
        std::vector<std::uint32_t> m_activeVAOs;

        //handles which have been returned and can be reused
        std::vector<std::uint32_t> m_freeVAOs;
    };
}
